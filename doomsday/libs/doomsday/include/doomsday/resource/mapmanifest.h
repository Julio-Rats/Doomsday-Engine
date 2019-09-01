/** @file mapmanifest.h  Manifest for a map resource.
 *
 * @authors Copyright © 2014-2015 Daniel Swanson <danij@dengine.net>
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
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#ifndef LIBDOOMSDAY_RESOURCE_MAPMANIFEST_H
#define LIBDOOMSDAY_RESOURCE_MAPMANIFEST_H

#include "../filesys/file.h"
#include "../filesys/lumpindex.h"
#include "../Game"
#include "../uri.h"
#include <de/PathTree>
#include <de/Record>
#include <de/String>
#include <memory.h>

namespace res {

/**
 * Resource manifest for a map.
 *
 * @ingroup resource
 */
class LIBDOOMSDAY_PUBLIC MapManifest : public de::PathTree::Node, public de::Record
{
public:
    MapManifest(de::PathTree::NodeArgs const &args);

    /**
     * Returns a textual description of the manifest.
     *
     * @return Human-friendly description the manifest.
     */
    de::String description(de::Uri::ComposeAsTextFlags uriCompositionFlags = de::Uri::DefaultComposeAsTextFlags) const;

    /**
     * Returns the URI this resource will be known by.
     */
    inline de::Uri composeUri() const { return de::Uri("Maps", gets("id")); }

    /**
     * Returns the id used to uniquely reference the map in some (old) definitions.
     */
    de::String composeUniqueId(Game const &currentGame) const;

    MapManifest &setSourceFile(de::File1 *newSourceFile);
    de::File1 *sourceFile() const;

    MapManifest &setRecognizer(de::Id1MapRecognizer *newRecognizer);
    de::Id1MapRecognizer const &recognizer() const;

private:
    //de::String cachePath;
    //bool lastLoadAttemptFailed;
    de::File1 *_sourceFile;
    std::unique_ptr<de::Id1MapRecognizer> _recognized;
};

}  // namespace res

#endif  // LIBDOOMSDAY_RESOURCE_MAPMANIFEST_H
