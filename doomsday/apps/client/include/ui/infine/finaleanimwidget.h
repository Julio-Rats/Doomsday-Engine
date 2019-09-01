/** @file finaleanimwidget.h  InFine animation system, FinaleAnimWidget.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2014 Daniel Swanson <danij@dengine.net>
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

#ifndef DENG_UI_INFINE_FINALEANIMWIDGET_H
#define DENG_UI_INFINE_FINALEANIMWIDGET_H

#include <QList>
#include <doomsday/world/Material>
#include "finalewidget.h"

/**
 * Finale animation widget. Colored rectangles or image sequence animations.
 *
 * @ingroup infine
 */
class FinaleAnimWidget : public FinaleWidget
{
public:
    /**
     * Describes a frame in the animation sequence.
     */
    struct Frame
    {
        enum Type
        {
            PFT_MATERIAL,
            PFT_PATCH,
            PFT_RAW, /// "Raw" graphic or PCX lump.
            PFT_XIMAGE /// External graphics resource.
        };

        int tics;
        Type type;
        struct Flags {
            char flip:1;
        } flags;
        union {
            world::Material *material;
            patchid_t patch;
            lumpnum_t lumpNum;
            DGLuint tex;
        } texRef;
        short sound;

        Frame();
        ~Frame();
    };
    typedef QList<Frame *> Frames;

public:
    FinaleAnimWidget(de::String const &name);
    virtual ~FinaleAnimWidget();

    /// @todo Observe instead.
    bool animationComplete() const;

    FinaleAnimWidget &setLooping(bool yes = true);

    int newFrame(Frame::Type type, int tics, void *texRef, short sound, bool flagFlipH);

    Frames const &allFrames() const;
    FinaleAnimWidget &clearAllFrames();

    inline int frameCount() const { return allFrames().count(); }

    FinaleAnimWidget &resetAllColors();

    animator_t const *color() const;
    FinaleAnimWidget &setColorAndAlpha(de::Vec4f const &newColorAndAlpha, int steps = 0);
    FinaleAnimWidget &setColor(de::Vec3f const &newColor, int steps = 0);
    FinaleAnimWidget &setAlpha(float newAlpha, int steps = 0);

    animator_t const *edgeColor() const;
    FinaleAnimWidget &setEdgeColorAndAlpha(de::Vec4f const &newColorAndAlpha, int steps = 0);
    FinaleAnimWidget &setEdgeColor(de::Vec3f const &newColor, int steps = 0);
    FinaleAnimWidget &setEdgeAlpha(float newAlpha, int steps = 0);

    animator_t const *otherColor() const;
    FinaleAnimWidget &setOtherColorAndAlpha(de::Vec4f const &newColorAndAlpha, int steps = 0);
    FinaleAnimWidget &setOtherColor(de::Vec3f const &newColor, int steps = 0);
    FinaleAnimWidget &setOtherAlpha(float newAlpha, int steps = 0);

    animator_t const *otherEdgeColor() const;
    FinaleAnimWidget &setOtherEdgeColorAndAlpha(de::Vec4f const &newColorAndAlpha, int steps = 0);
    FinaleAnimWidget &setOtherEdgeColor(de::Vec3f const &newColor, int steps = 0);
    FinaleAnimWidget &setOtherEdgeAlpha(float newAlpha, int steps = 0);

protected:
#ifdef __CLIENT__
    void draw(de::Vec3f const &offset);
#endif
    void runTicks(/*timespan_t timeDelta*/);

private:
    DENG2_PRIVATE(d)
};

typedef FinaleAnimWidget::Frame FinaleAnimWidgetFrame;

#endif // DENG_UI_INFINE_FINALEANIMWIDGET_H
