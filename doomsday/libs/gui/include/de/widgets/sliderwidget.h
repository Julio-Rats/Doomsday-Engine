/** @file sliderwidget.h  Slider to pick a value within a range.
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

#ifndef LIBAPPFW_SLIDERWIDGET_H
#define LIBAPPFW_SLIDERWIDGET_H

#include "../GuiWidget"

#include <de/Range>

namespace de {

/**
 * Slider to pick a value within a range.
 *
 * The value can also be entered as text by right clicking on the slider.
 *
 * @ingroup guiWidgets
 */
class LIBGUI_PUBLIC SliderWidget : public GuiWidget
{
public:
    DE_DEFINE_AUDIENCE2(Value,     void sliderValueChanged(SliderWidget &, double value))
    DE_DEFINE_AUDIENCE2(UserValue, void sliderValueChangedByUser(SliderWidget &, double value))

public:
    SliderWidget(String const &name = {});

    void setRange(Rangei const &intRange, int step = 1);
    void setRange(Rangef const &floatRange, float step = 0);
    void setRange(Ranged const &doubleRange, ddouble step = 0);
    void setPrecision(int precisionDecimals);
    void setStep(double step);
    void setValue(ddouble value);

    void setMinLabel(String const &labelText);
    void setMaxLabel(String const &labelText);

    /**
     * Displayed values are multiplied by this factor when displayed.
     * Does not affect the real value of the slider.
     *
     * @param factor  Display multiplier.
     */
    void setDisplayFactor(ddouble factor);

    Ranged range() const;
    ddouble value() const;
    int precision() const;
    ddouble displayFactor() const;

    // Events.
//    void viewResized();
    void update();
    void drawContent();
    bool handleEvent(Event const &event);
    void setValueFromText(const String &text);


protected:
    void glInit();
    void glDeinit();
    void updateStyle();

private:
    DE_PRIVATE(d)
};

} // namespace de

#endif // LIBAPPFW_SLIDERWIDGET_H
