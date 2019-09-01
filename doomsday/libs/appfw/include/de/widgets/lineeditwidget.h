/** @file widgets/lineeditwidget.h
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

#ifndef LIBAPPFW_LINEEDITWIDGET_H
#define LIBAPPFW_LINEEDITWIDGET_H

#include "../GuiWidget"
#include <de/shell/AbstractLineEditor>
#include <de/KeyEvent>

namespace de {

/**
 * Widget showing a lineedit text and/or image.
 *
 * As a graphical widget, widget placement and line wrapping is handled in
 * terms of pixels rather than characters.
 *
 * @ingroup guiWidgets
 */
class LIBAPPFW_PUBLIC LineEditWidget : public GuiWidget, public shell::AbstractLineEditor
{
    Q_OBJECT

public:
    LineEditWidget(String const &name = String());

    void setText(String const &lineText) override;

    /**
     * Sets the text that will be shown in the editor when it is empty.
     *
     * @param hintText  Hint text.
     */
    void setEmptyContentHint(String const &hintText, String const &hintFont = String());

    /**
     * Enables or disables the signal emitted when the edit widget receives an
     * Enter key. By default, no a signal is emitted (and the key is thus not
     * eaten).
     *
     * @param enterSignal  @c true to enable signal and eat event, @c false to
     *                     disable.
     */
    void setSignalOnEnter(bool enterSignal);

    /**
     * Determines where the cursor is currently in view coordinates.
     */
    Rectanglei cursorRect() const;

    void setColorTheme(ColorTheme theme);
    void setUnfocusedBackgroundOpacity(float opacity);

    // Events.
    void viewResized() override;
    void focusGained() override;
    void focusLost() override;
    void update() override;
    void drawContent() override;
    bool handleEvent(Event const &event) override;

public:
    static KeyModifiers modifiersFromKeyEvent(KeyEvent::Modifiers const &keyMods);

#if defined (DENG_MOBILE)
protected slots:
    void userEnteredText(QString);
    void userFinishedTextEntry();
#endif
    
signals:
    void enterPressed(QString text);
    void editorContentChanged();

protected:
    void glInit() override;
    void glDeinit() override;
    void glMakeGeometry(GuiVertexBuilder &verts) override;
    void updateStyle() override;

    int maximumWidth() const override;
    void numberOfLinesChanged(int lineCount) override;
    void cursorMoved() override;
    void contentChanged() override;
    void autoCompletionEnded(bool accepted) override;

private:
    DENG2_PRIVATE(d)
};

} // namespace de

#endif // LIBAPPFW_LINEEDITWIDGET_H
