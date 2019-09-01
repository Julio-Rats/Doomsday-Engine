/** @file remote/query.cpp
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

#include "de/filesys/Query"

namespace de {
namespace filesys {

Query::Query(Request<FileMetadata> req, String path)
    : path(path), fileMetadata(req)
{}

Query::Query(Request<FileContents> req, String path)
    : path(path), fileContents(req)
{}

bool Query::isValid() const
{
    if (fileMetadata) return fileMetadata->isValid();
    if (fileContents) return fileContents->isValid();
    return false;
}

void Query::cancel()
{
    if (fileMetadata) fileMetadata->cancel();
    if (fileContents) fileContents->cancel();
}

} // namespace filesys
} // namespace de
