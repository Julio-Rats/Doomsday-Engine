/** @file proceduralimage.h  Procedural image.
 *
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

#ifndef LIBAPPFW_PROCEDURALIMAGE_H
#define LIBAPPFW_PROCEDURALIMAGE_H

#include <de/Vector>
#include <de/GLBuffer>
#include <de/Painter>

#include "../libappfw.h"

namespace de {

/**
 * Base class for procedural images.
 *
 * A procedural image can be used instead of a static one to generate geometry
 * on the fly (see LabelWidget).
 *
 * @ingroup appfw
 */
class LIBAPPFW_PUBLIC ProceduralImage
{
public:
    typedef Vector2f Size;
    typedef Vector4f Color;

public:
    ProceduralImage(Size const &pointSize = Size());
    virtual ~ProceduralImage();

    Size pointSize() const; // in points
    Color color() const;

    void setPointSize(Size const &pointSize);
    void setColor(Color const &color);

    /**
     * Updates the image.
     *
     * @return @c true, if the geometry has changed and it should be remade. Otherwise
     * @c false if nothing has been changed.
     */
    virtual bool update();

    virtual void glInit(); // called repeatedly
    virtual void glDeinit();
    virtual void glMakeGeometry(GuiVertexBuilder &verts, Rectanglef const &rect) = 0;

    DENG2_CAST_METHODS()

private:
    Size _pointSize;
    Color _color;
};

} // namespace de

#endif // LIBAPPFW_PROCEDURALIMAGE_H
