/** @file staticlibraryfeed.h
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

#ifndef DENG2_STATICLIBRARYFEED_H
#define DENG2_STATICLIBRARYFEED_H

#include "../Feed"

namespace de {

/**
 * Feed for populating LibraryFiles representing imported static libraries.
 */
class StaticLibraryFeed : public Feed
{
public:
    StaticLibraryFeed();

    String description() const;
    PopulatedFiles populate(Folder const &folder);
    bool prune(File &file) const;
};

} // namespace de

#endif // DENG2_STATICLIBRARYFEED_H
