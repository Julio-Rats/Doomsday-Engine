/** @file render.cpp
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

#include "gloom/render/render.h"

namespace gloom {

DENG2_PIMPL_NOREF(Render)
{
    const Context *context = nullptr;
};

Render::Render()
    : d(new Impl)
{}

Render::~Render()
{
    DENG2_ASSERT(d->context == nullptr);
}

const Context &Render::context() const
{
    DENG2_ASSERT(d->context);
    return *d->context;
}

void Render::glInit(const Context &context)
{
    DENG2_ASSERT(d->context == nullptr);
    d->context = &context;
}

void Render::glDeinit()
{
    DENG2_ASSERT(d->context != nullptr);
    d->context = nullptr;
}

void Render::advanceTime(de::TimeSpan)
{}

} // namespace gloom
