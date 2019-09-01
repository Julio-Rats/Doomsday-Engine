/** @file packagedownloader.h  Utility for downloading packages from a remote repository.
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

#ifndef LIBSHELL_PACKAGEDOWNLOADER_H
#define LIBSHELL_PACKAGEDOWNLOADER_H

#include <de/Range>
#include <de/String>
#include <de/filesys/Link>
#include "ServerInfo"

namespace de { namespace shell {

/**
 * Utility for downloading packages from remote repositories.
 * @ingroup network
 */
class LIBSHELL_PUBLIC PackageDownloader
{
public:
    PackageDownloader();

    typedef std::function<void(filesys::Link const *)> MountCallback;

    /**
     * Mount a server's remote file repository.
     *
     * @param serverInfo      Server information.
     * @param afterConnected  Callback to call when the repository is connected and ready for use.
     */
    void mountServerRepository(ServerInfo const &serverInfo, MountCallback afterConnected);

    void unmountServerRepository();

    /**
     * Start downloading files for a set of packages. A notification callback is done
     * after the operation is complete (successfully or not).
     *
     * @param packageIds  Packages to download from the remote repository.
     * @param callback    Called when the downloads are finished or cancelled.
     */
    void download(StringList packageIds, std::function<void()> callback);

    de::String fileRepository() const;

    /**
     * Cancel the ongoing downloads.
     */
    void cancel();

    bool isCancelled() const;

    /**
     * Determines whether downloads are currently active.
     */
    bool isActive() const;

public:
    /**
     * Notified when file downloads are progressing. The ranges describe the remaining
     * and total amounts. For example, `bytes.start` is the number of total bytes
     * remaining to download. `bytes.size()` is the number of bytes downloaded so far.
     * `bytes.end` is the total number of bytes overall.
     */
    DENG2_DEFINE_AUDIENCE2(Status,
                           void downloadStatusUpdate(Rangei64 const &bytes, Rangei const &files))

private:
    DENG2_PRIVATE(d)
};

}} // namespace de::shell

#endif // LIBSHELL_PACKAGEDOWNLOADER_H
