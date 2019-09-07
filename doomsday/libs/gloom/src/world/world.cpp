/** @file world.cpp
 *
 * @authors Copyright (c) 2018 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "gloom/world/world.h"

using namespace de;

namespace gloom {

World::World()
{}

World::~World()
{}

void World::glInit()
{}

void World::glDeinit()
{}

void World::update(TimeSpan)
{}

void World::render(const ICamera &)
{}

World::POI World::initialViewPosition() const
{
    return Vec3f();
}

List<World::POI> World::pointsOfInterest() const
{
    return List<POI>();
}

double World::groundSurfaceHeight(const Vec3d &) const
{
    return 0.0;
}

double World::ceilingHeight(const Vec3d &) const
{
    return 1000.0;
}

} // namespace gloom
