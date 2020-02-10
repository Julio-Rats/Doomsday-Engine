/** @file environment.h
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

#ifndef ENVIRONMENT_H
#define ENVIRONMENT_H

#include <de/Time>

namespace gloom {

class IWorld;

class Environment
{
public:
    Environment();

    void setWorld(IWorld *world);

    void enable(bool enabled = true);
    void disable() { enable(false); }
    void advanceTime(de::TimeSpan elapsed);

private:
    DE_PRIVATE(d)
};

} // namespace gloom

#endif // ENVIRONMENT_H
