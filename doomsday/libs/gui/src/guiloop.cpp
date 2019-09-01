/** @file guiloop.cpp
 *
 * @authors Copyright (c) 2016-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/GuiLoop"
#include "de/GLWindow"

namespace de {

DENG2_PIMPL_NOREF(GuiLoop)
{
    GLWindow *window = nullptr;
};

GuiLoop::GuiLoop()
    : d(new Impl)
{}

void GuiLoop::setWindow(GLWindow *window)
{
    d->window = window;
}

GuiLoop &GuiLoop::get() // static
{
    return static_cast<GuiLoop &>(Loop::get());
}

void GuiLoop::nextLoopIteration()
{
    if (d->window)
    {
        d->window->glActivate();
    }

    Loop::nextLoopIteration();

    if (d->window)
    {
        d->window->glDone();
    }
}

} // namespace de
