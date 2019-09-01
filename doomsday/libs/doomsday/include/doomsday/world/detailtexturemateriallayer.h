/** @file materialdetaillayer.h  Logical material, detail-texture layer.
 *
 * @authors Copyright © 2011-2014 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDOOMSDAY_WORLD_DETAILTEXTUREMATERIALLAYER_H
#define LIBDOOMSDAY_WORLD_DETAILTEXTUREMATERIALLAYER_H

#include <de/String>
#include "../defs/dedtypes.h"
#include "texturemateriallayer.h"

namespace world {

/**
 * Specialized TextureMaterialLayer for describing an animated detail texture layer.
 *
 * @ingroup resource
 */
class LIBDOOMSDAY_PUBLIC DetailTextureMaterialLayer : public TextureMaterialLayer
{
public:
    /**
     * Stages describe texture change animations.
     */
    class LIBDOOMSDAY_PUBLIC AnimationStage : public TextureMaterialLayer::AnimationStage
    {
    public:
        AnimationStage(de::Uri const &texture, int tics,
                       float variance    = 0,
                       float scale       = 1,
                       float strength    = 1,
                       float maxDistance = 0);
        AnimationStage(AnimationStage const &other);
        virtual ~AnimationStage();

        /**
         * Construct a new AnimationStage from the given @a definition.
         */
        static AnimationStage *fromDef(ded_detail_stage_t const &definition);

        void resetToDefaults();

    public:
        float scale;
        float strength;
        float maxDistance;
    };

public:
    virtual ~DetailTextureMaterialLayer() {}

    /**
     * Construct a new DetailTextureLayer from the given @a definition.
     */
    static DetailTextureMaterialLayer *fromDef(ded_detailtexture_t const &definition);

    /**
     * Add a new animation stage to the detail texture layer.
     *
     * @param stage  New stage to add (a copy is made).
     *
     * @return  Index of the newly added stage (0 based).
     */
    int addStage(AnimationStage const &stage);

    de::String describe() const;
};

} // namespace world

#endif // LIBDOOMSDAY_WORLD_DETAILTEXTUREMATERIALLAYER_H
