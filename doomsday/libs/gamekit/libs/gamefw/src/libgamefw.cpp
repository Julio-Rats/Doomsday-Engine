/** @file libgamefw.cpp  Common framework for games.
 *
 * @authors Copyright (c) 2016-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "gamefw/libgamefw.h"
#include <de/Extension>

static gfw_game_id_t theCurrentGame = GFW_GAME_ID_COUNT;

void gfw_SetCurrentGame(gfw_game_id_t game)
{
    DE_ASSERT(game != GFW_STRIFE);
    theCurrentGame = game;
}

gfw_game_id_t gfw_CurrentGame()
{
    return theCurrentGame;
}
