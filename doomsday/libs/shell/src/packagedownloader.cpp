/** @file packagedownloader.cpp  Utility for downloading packages from a remote repository.
 *
 * @authors Copyright (c) 2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#include "de/shell/PackageDownloader"

#include <de/App>
#include <de/FileSystem>
#include <de/Folder>
#include <de/Garbage>
#include <de/LinkFile>
#include <de/Loop>
#include <de/filesys/NativeLink>
#include <de/PackageLoader>
#include <de/RemoteFeedRelay>
#include <de/RemoteFile>

namespace de { namespace shell {

static String const PATH_REMOTE_PACKS  = "/remote/packs";
static String const PATH_REMOTE_SERVER = "/remote/server"; // local folder for RemoteFeed

DENG2_PIMPL(PackageDownloader)
, DENG2_OBSERVES(filesys::RemoteFeedRelay, Status)
, DENG2_OBSERVES(Asset, StateChange)
, DENG2_OBSERVES(RemoteFile, Download)
, DENG2_OBSERVES(Deletable, Deletion)
{
    String                           fileRepository;
    MountCallback                    afterConnected;
    bool                             isCancelled  = false;
    dint64                           totalBytes   = 0;
    int                              numDownloads = 0;
    AssetGroup                       downloads;
    QHash<IDownloadable *, Rangei64> downloadBytes;
    std::function<void()>            postDownloadCallback;
    LoopCallback                     deferred;

    Impl(Public *i) : Base(i) {}

    void remoteRepositoryStatusChanged(String const &address,
                                       filesys::RemoteFeedRelay::Status) override
    {
        if (address == fileRepository)
        {
            // When NativeLink is connected, any pending folder populations will be
            // started. We'll defer this callback so that NativeLink gets to react
            // first to the status change notification.

            deferred.enqueue([this] ()
            {
                auto *relay = &filesys::RemoteFeedRelay::get();
                relay->audienceForStatus() -= this;

                // Populate remote folders before notifying so everything is ready to go.
                Folder::afterPopulation([this, relay] ()
                {
                    if (afterConnected)
                    {
                        afterConnected(relay->repository(fileRepository));
                    }
                });
            });
        }
    }

    void downloadFile(File &file)
    {
        if (auto *folder = maybeAs<Folder>(file))
        {
            folder->forContents([this] (String, File &f)
            {
                downloadFile(f);
                return LoopContinue;
            });
        }
        if (auto *dl = maybeAs<IDownloadable>(file))
        {
            LOG_NET_VERBOSE("Downloading from server: %s") << file.description();

            downloads.insert(dl->asset());
            dl->audienceForDownload += this;
            file.Deletable::audienceForDeletion += this;
            downloadBytes.insert(dl, Rangei64(dint64(dl->downloadSize()),
                                              dint64(dl->downloadSize())));
            numDownloads++;
            totalBytes += dl->downloadSize();
            isCancelled = false;

            dl->download();
        }
    }

    void downloadProgress(IDownloadable &dl, dsize remainingBytes) override
    {
        DENG2_ASSERT_IN_MAIN_THREAD();

        auto found = downloadBytes.find(&dl);
        if (found == downloadBytes.end()) return;

        found.value().start = dint64(remainingBytes);

        if (remainingBytes == 0)
        {
            dl.audienceForDownload -= this;
            maybeAs<File>(dl)->Deletable::audienceForDeletion -= this;
            downloadBytes.erase(found);
        }

        // Update total for the UI popup.
        {
            dint64 totalRemaining = 0;
            foreach (auto bytes, downloadBytes)
            {
                totalRemaining += bytes.start;
            }

            DENG2_FOR_PUBLIC_AUDIENCE2(Status, i)
            {
                i->downloadStatusUpdate(Rangei64(totalRemaining, totalBytes),
                                        Rangei(downloadBytes.size(), numDownloads));
            }
        }
    }

    void objectWasDeleted(Deletable *del) override
    {
        DENG2_ASSERT_IN_MAIN_THREAD();
        downloadBytes.remove(static_cast<RemoteFile *>(del));
    }

    void assetStateChanged(Asset &) override
    {
        if (downloads.isReady())
        {
            LOG_NET_VERBOSE(isCancelled? "Remote file downloads cancelled"
                                       : "All downloads of remote files finished");
            Loop::mainCall([this] ()
            {
                DENG2_ASSERT(downloadBytes.isEmpty());
                if (postDownloadCallback) postDownloadCallback();
            });
        }
    }

    void finishDownloads()
    {
        // Final status update.
        DENG2_FOR_PUBLIC_AUDIENCE2(Status, i)
        {
            i->downloadStatusUpdate(Rangei64(0, totalBytes), Rangei(0, numDownloads));
        }
        numDownloads = 0;
        totalBytes = 0;
        downloads.clear();
    }

    void clearDownloads()
    {
        for (auto i = downloadBytes.begin(); i != downloadBytes.end(); ++i)
        {
            auto *dl = i.key();

            // Ongoing (partial) downloads will be cancelled.
            dl->cancelDownload();

            dl->audienceForDownload -= this;
            maybeAs<File>(dl)->Deletable::audienceForDeletion -= this;
        }
        numDownloads = 0;
        totalBytes = 0;
        downloadBytes.clear();
        downloads.clear();
    }

    /**
     * Makes remote packages available for loading locally.
     *
     * Once remote files have been downloaded, PackageLoader still needs to be made aware
     * that the packages are available. This is done via link files that have the ".pack"
     * extension and thus are treated as loadable packages.
     *
     * @param ids  Identifiers of remote packages that have been downloaded and are now
     *             being prepared for loading.
     */
    void linkRemotePackages(filesys::PackagePaths const &pkgPaths)
    {
        Folder &remotePacks = FS::get().makeFolder(PATH_REMOTE_PACKS);
        for (auto i = pkgPaths.begin(); i != pkgPaths.end(); ++i)
        {
            LOG_RES_VERBOSE("Registering remote package \"%s\"") << i.key();
            if (auto *file = FS::tryLocate<File>(i.value().localPath))
            {
                LOGDEV_RES_VERBOSE("Cached metadata:\n") << file->objectNamespace().asText();

                auto *pack = LinkFile::newLinkToFile(*file, file->name() + ".pack");
                Record &meta = pack->objectNamespace();
                meta.add("package", new Record(file->objectNamespace().subrecord("package")));
                meta.set("package.path", file->path());
                remotePacks.add(pack);
                FS::get().index(*pack);

                LOG_RES_VERBOSE("\"%s\" linked as ") << i.key() << pack->path();
            }
        }
    }

    void unlinkRemotePackages()
    {
        // Unload all server packages. Note that the entire folder will be destroyed, too.
        if (std::unique_ptr<Folder> remotePacks { FS::tryLocate<Folder>(PATH_REMOTE_PACKS) })
        {
            remotePacks->forContents([] (String, File &file)
            {
                LOG_RES_VERBOSE("Unloading remote package: ") << file.description();
                PackageLoader::get().unload(Package::identifierForFile(file));
                return LoopContinue;
            });
        }
    }

    DENG2_PIMPL_AUDIENCE(Status)
};

DENG2_AUDIENCE_METHOD(PackageDownloader, Status)

PackageDownloader::PackageDownloader()
    : d(new Impl(this))
{}

String PackageDownloader::fileRepository() const
{
    return d->fileRepository;
}

void PackageDownloader::cancel()
{
    d->isCancelled = true;
    DENG2_FOR_AUDIENCE2(Status, i)
    {
        i->downloadStatusUpdate(Rangei64(), Rangei());
    }
    d->clearDownloads();
}

bool PackageDownloader::isCancelled() const
{
    return d->isCancelled;
}

bool PackageDownloader::isActive() const
{
    return !d->downloads.isEmpty() && !d->downloads.isReady();
}

void PackageDownloader::mountServerRepository(shell::ServerInfo const &info,
                                              MountCallback afterConnected)
{
    // The remote repository feature was added in 2.1. Trying to send a RemoteFeed
    // request to an older server would just result in us getting immediately
    // disconnected.

    if (info.version() > Version(2, 1, 0, 2484))
    {
        auto &relay = filesys::RemoteFeedRelay::get();

        d->fileRepository = filesys::NativeLink::URL_SCHEME + info.address().asText();
        d->isCancelled    = false;
        relay.addRepository(d->fileRepository, PATH_REMOTE_SERVER);

        // Notify after repository is available.
        d->afterConnected = afterConnected;
        relay.audienceForStatus() += d;
    }
    else if (afterConnected)
    {
        afterConnected(nullptr);
    }
}

void PackageDownloader::unmountServerRepository()
{
    d->clearDownloads();
    d->unlinkRemotePackages();
    filesys::RemoteFeedRelay::get().removeRepository(d->fileRepository);
    d->fileRepository.clear();
    d->isCancelled = false;
    
    if (Folder *remoteFiles = FS::tryLocate<Folder>(PATH_REMOTE_SERVER))
    {
        trash(remoteFiles);
    }
}

void PackageDownloader::download(StringList packageIds, std::function<void ()> callback)
{
    d->downloads.clear();

    auto const pkgPaths = filesys::RemoteFeedRelay::get().locatePackages(packageIds);

    // The set of found packages may not contain all the requested packages.
    StringList const foundIds = pkgPaths.keys();
    for (auto found = pkgPaths.begin(); found != pkgPaths.end(); ++found)
    {
        if (File *file = found.value().link->populateRemotePath(found.key(), found.value()))
        {
            d->downloadFile(*file);
        }
    }

    auto finished = [this, pkgPaths, callback] ()
    {
        // Let's finalize the downloads so we can load all the packages.
        d->downloads.audienceForStateChange() -= d;
        d->finishDownloads();
        d->linkRemotePackages(pkgPaths);

        callback();
    };

    // If nothing needs to be downloaded, let's just continue right away.
    if (d->downloads.isReady())
    {
        d->postDownloadCallback = nullptr;
        finished();
    }
    else
    {
        d->postDownloadCallback = finished;
        d->downloads.audienceForStateChange() += d;
    }
}

}} // namespace de::shell
