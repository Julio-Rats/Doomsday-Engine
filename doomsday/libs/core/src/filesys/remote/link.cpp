/** @file remote/link.cpp
 *
 * @authors Copyright (c) 2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * LGPL: http://www.gnu.org/licenses/lgpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser
 * General Public License for more details. You should have received a copy of
 * the GNU Lesser General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#include "de/filesys/Link"
#include "de/RemoteFeedRelay"
#include "de/FileSystem"

#include <de/App>
#include <de/Async>
#include <de/Garbage>
#include <de/charsymbols.h>

namespace de {
namespace filesys {

DENG2_PIMPL(Link), public AsyncScope
{
    String localRootPath;
    State state = Initializing;
    String address;
    QueryId nextQueryId = 1;
    QList<Query> deferredQueries;
    QHash<QueryId, Query> pendingQueries;

    Impl(Public *i) : Base(i)
    {}

    ~Impl()
    {
        cancelAllQueries();
    }

    void cancelAllQueries()
    {
        for (auto i = deferredQueries.begin(); i != deferredQueries.end(); ++i)
        {
            i->cancel();
        }
        for (auto i = pendingQueries.begin(); i != pendingQueries.end(); ++i)
        {
            i.value().cancel();
        }
    }

    /// Remove all invalid/cancelled queries.
    void cleanup()
    {
        QMutableHashIterator<QueryId, Query> iter(pendingQueries);
        while (iter.hasNext())
        {
            iter.next();
            if (!iter.value().isValid())
            {
                iter.remove();
            }
        }
    }

    void sendQuery(Query query)
    {
        try
        {
            query.id = nextQueryId++;
            if (state == Ready)
            {
                self().transmit(query);
                pendingQueries.insert(query.id, query);
                cleanup();
            }
            else
            {
                deferredQueries.append(query);
            }
        }
        catch (Error const &er)
        {
            LOG_NET_ERROR("Error sending file repository query: %s")
                    << er.asText();
        }
    }

    void sendDeferredQueries()
    {
        foreach (Query const &query, deferredQueries)
        {
            if (query.isValid())
            {
                pendingQueries.insert(query.id, query);
                self().transmit(query);
            }
        }
        deferredQueries.clear();
    }

    void notifyStatus(RemoteFeedRelay::Status status)
    {
        using Relay = RemoteFeedRelay;
        DENG2_FOR_EACH_OBSERVER(Relay::StatusAudience, i,
                                Relay::get().audienceForStatus())
        {
            i->remoteRepositoryStatusChanged(address, status);
        }
    }
};

Link::Link(String const &address)
    : d(new Impl(this))
{
    d->address = address;
}

Link::~Link()
{
    d->cancelAllQueries();

    if (d->state != Deinitialized)
    {
        // Normally, the local folder will be deleted when the link is disconnected.
        if (auto *folder = FS::tryLocate<Folder>(d->localRootPath))
        {
            delete folder;
        }
    }
}

void Link::setLocalRoot(String const &rootPath)
{
    d->localRootPath = rootPath;
    // Create the folder right away.
    localRoot();
}

Folder &Link::localRoot() const
{
    return FS::get().makeFolder(d->localRootPath, FS::DontInheritFeeds);
}

String Link::address() const
{
    return d->address;
}

Link::State Link::state() const
{
    return d->state;
}

StringList Link::categoryTags() const
{
    return StringList();
}

void Link::wasConnected()
{
    DENG2_ASSERT_IN_MAIN_THREAD();
    d->state = Ready;
    d->sendDeferredQueries();
    d->notifyStatus(RemoteFeedRelay::Connected);
}

void Link::wasDisconnected()
{
    DENG2_ASSERT_IN_MAIN_THREAD();
    d->state = Deinitialized;
    d->cancelAllQueries();
    d->cleanup();
    d->notifyStatus(RemoteFeedRelay::Disconnected);

    // Remove the local root folder.
    if (Folder *root = FS::tryLocate<Folder>(d->localRootPath))
    {
        trash(root);
    }
}

void Link::handleError(QString errorMessage)
{
    LOG_NET_ERROR("Error accessing remote file repository \"%s\": %s " DENG2_CHAR_MDASH
                  " files from repository may not be available")
            << d->address
            << errorMessage;
}

AsyncScope &Link::scope()
{
    return *d;
}

void Link::cancelAllQueries()
{
    d->cancelAllQueries();
}

void Link::cleanupQueries()
{
    d->cleanup();
}

Query *Link::findQuery(QueryId id)
{
    auto found = d->pendingQueries.find(id);
    if (found != d->pendingQueries.end())
    {
        return &found.value();
    }
    return nullptr;
}

QueryId Link::sendQuery(Query query)
{
    try
    {
        query.id = d->nextQueryId++;
        if (d->state == Ready)
        {
            d->pendingQueries.insert(query.id, query);
            transmit(query);
            d->cleanup();
        }
        else
        {
            d->deferredQueries.append(query);
        }
        return query.id;
    }
    catch (Error const &er)
    {
        LOG_NET_ERROR("Error sending file repository query: %s") << er.asText();
        return 0;
    }
}

File *Link::populateRemotePath(String const &, RepositoryPath const &path) const
{
    // By default we assume the remote files are all populated while connecting.
    return FS::tryLocate<File>(path.localPath);
}

//void Link::packagePathsReceived(QueryId id, PackagePaths const &remotePaths)
//{
//    if (auto *query = findQuery(id))
//    {
//        if (query->remotePaths)
//        {
//            query->remotePaths->call(id, remotePaths);
//        }
//        d->pendingQueries.remove(id);
//    }
//}

void Link::metadataReceived(QueryId id, DictionaryValue const &metadata)
{
    if (auto *query = findQuery(id))
    {
        if (query->fileMetadata)
        {
            query->fileMetadata->call(metadata);
        }
        d->pendingQueries.remove(id);
    }
}

void Link::chunkReceived(QueryId id, duint64 startOffset, Block const &chunk, duint64 fileSize)
{
    if (auto *query = findQuery(id))
    {
        // Get rid of cancelled queries.
        if (!query->isValid())
        {
            d->pendingQueries.remove(id);
            return;
        }

        // Before the first chunk, notify about the total size.
        if (!query->fileSize)
        {
            query->fileContents->call(0, Block(), fileSize);
        }

        query->fileSize = fileSize;
        query->receivedBytes += chunk.size();

        // Notify about progress and provide the data chunk to the requestor.
        query->fileContents->call(startOffset, chunk,
                                  fileSize - query->receivedBytes);

        if (fileSize == query->receivedBytes)
        {
            // Transfer complete.
            d->pendingQueries.remove(id);
        }
    }
}

} // namespace filesys
} // namespace de
