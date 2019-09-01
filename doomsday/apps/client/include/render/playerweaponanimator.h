/** @file playerweaponanimator.h  Player weapon model animator.
 *
 * @authors Copyright (c) 2015-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef CLIENT_RENDER_PLAYERWEAPONANIMATOR_H
#define CLIENT_RENDER_PLAYERWEAPONANIMATOR_H

#include <de/ModelDrawable>
#include "stateanimator.h"

class ClientPlayer;
struct state_s;

namespace render {

/**
 * Animates the player weapon model.
 *
 * @ingroup render
 */
class PlayerWeaponAnimator
{
public:
    PlayerWeaponAnimator(ClientPlayer *plr);

    void setAsset(de::String const &identifier);

    de::String assetId() const;

    void stateChanged(state_s const *state);

    /**
     * Determines if a 3D model has been found and is ready to be rendered.
     */
    bool hasModel() const;

    Model const *model() const;

    StateAnimator &animator();

    void setupVisPSprite(vispsprite_t &spr) const;

    void advanceTime(de::TimeSpan const &elapsed);

private:
    DE_PRIVATE(d)
};

} // namespace render

#endif // CLIENT_RENDER_PLAYERWEAPONANIMATOR_H

