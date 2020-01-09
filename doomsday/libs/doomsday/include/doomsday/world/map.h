/** @file map.h  Base for world maps.
 * @ingroup world
 *
 * @authors Copyright © 2014-2016 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDOOMSDAY_WORLD_MAP_H
#define LIBDOOMSDAY_WORLD_MAP_H

#include "../libdoomsday.h"
#include "../resource/mapmanifest.h"
#include <de/Observers>
#include <de/Reader>
#include <de/Writer>

class EntityDatabase;

namespace world {

class IThinkerMapping;

/**
 * Base class for world maps.
 */
class LIBDOOMSDAY_PUBLIC Map
{
public:
    /// No resource manifest is associated with the map. @ingroup errors
    DE_ERROR(MissingResourceManifestError);

    /// Required map object is missing. @ingroup errors
    DE_ERROR(MissingObjectError);

public:
    /**
     * @param manifest  Resource manifest for the map (Can be set later, @ref setDef).
     */
    explicit Map(res::MapManifest *manifest = nullptr);
    virtual ~Map();

    de::String id() const;

    /**
     * Returns @c true if a resource manifest is associated with the map.
     *
     * @see manifest(), setManifest()
     */
    bool hasManifest() const;

    /**
     * Returns the resource manifest for the map.
     *
     * @see hasManifest(), setManifest()
     */
    res::MapManifest &manifest() const;

    /**
     * Change the associated resource manifest to @a newManifest.
     *
     * @see hasManifest(), manifest()
     */
    void setManifest(res::MapManifest *newManifest);

    /**
     * Provides access to the entity database.
     */
    EntityDatabase &entityDatabase() const;

    virtual void serializeInternalState(de::Writer &to) const;
    virtual void deserializeInternalState(de::Reader &from, const IThinkerMapping &);

    DE_CAST_METHODS()

public:
    /// Notified when the map is about to be deleted.
    DE_AUDIENCE(Deletion, void mapBeingDeleted(const Map &map))

private:
    DE_PRIVATE(d)
};

typedef de::duint16 InternalSerialId;

// Identifiers for serialized internal state.
enum InternalSerialIds
{
    THINKER_DATA                = 0x0001,
    MOBJ_THINKER_DATA           = 0x0002,
    CLIENT_MOBJ_THINKER_DATA    = 0x0003,
    STATE_ANIMATOR              = 0x0004,
};

}  // namespace world

#endif  // LIBDOOMSDAY_WORLD_MAP_H
