/** @file imageitem.h  Data item with an image.
 *
 * @authors Copyright (c) 2014-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBAPPFW_UI_IMAGEITEM_H
#define LIBAPPFW_UI_IMAGEITEM_H

#include "item.h"
#include <de/Image>
#include <de/DotPath>

namespace de {
namespace ui {

/**
 * UI context item that represents a user action.
 *
 * @ingroup uidata
 */
class LIBAPPFW_PUBLIC ImageItem : public Item
{
public:
    ImageItem(Semantics semantics, String const &label = "")
        : Item(semantics, label) {}

    ImageItem(Semantics semantics, Image const &image, String const &label = "")
        : Item(semantics, label), _image(image) {}

    ImageItem(Semantics semantics, DotPath const &styleImageId, String const &label = "")
        : Item(semantics, label), _styleId(styleImageId) {}

    Image const &image() const;
    DotPath const &styleImageId() const;

    void setImage(Image const &image);
    void setImage(DotPath const &styleImageId);

private:
    Image _image;
    DotPath _styleId;
};

} // namespace ui
} // namespace de

#endif // LIBAPPFW_UI_IMAGEITEM_H
