/** @file vrwindowtransform.h  Window content transformation for virtual reality.
 *
 * @authors Copyright (c) 2013 Christopher Bruns <cmbruns@rotatingpenguin.com>
 * @authors Copyright (c) 2013-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBAPPFW_VRWINDOWTRANSFORM_H
#define LIBAPPFW_VRWINDOWTRANSFORM_H

#include "../libappfw.h"
#include "../WindowTransform"

namespace de {

class GLTextureFramebuffer;

/**
 * Window content transformation for virtual reality.
 *
 * @ingroup appfw
 */
class LIBAPPFW_PUBLIC VRWindowTransform : public WindowTransform
{
public:
    VRWindowTransform(BaseWindow &window);

    void glInit() override;
    void glDeinit() override;

    Vector2ui logicalRootSize(Vector2ui const &physicalWindowSize) const override;
    Vector2f windowToLogicalCoords(Vector2i const &pos) const override;
    Vector2f logicalToWindowCoords(Vector2i const &pos) const override;

    void drawTransformed() override;

    GLTextureFramebuffer &unwarpedFramebuffer();

private:
    DENG2_PRIVATE(d)
};

} // namespace de

#endif // LIBAPPFW_VRWINDOWTRANSFORM_H
