/** @file savelistwidget.h  List showing the available saves of a game.
 *
 * @authors Copyright (c) 2016-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#ifndef DE_CLIENT_UI_HOME_SAVELISTWIDGET_H
#define DE_CLIENT_UI_HOME_SAVELISTWIDGET_H

#include <de/MenuWidget>
#include <de/ui/Data>

class GamePanelButtonWidget;

class SaveListWidget : public de::MenuWidget
{
    Q_OBJECT

public:
    SaveListWidget(GamePanelButtonWidget &owner);

    de::ui::DataPos selectedPos() const;
    void setSelectedPos(de::ui::DataPos pos);
    void clearSelection();

signals:
    /**
     * Emitted when the selected item changes.
     *
     * @param pos  Position of the selected item in the shared saved sessions
     *             list data model.
     */
    void selectionChanged(de::ui::DataPos pos);

    void doubleClicked(de::ui::DataPos pos);

private:
    DE_PRIVATE(d)
};

#endif // DE_CLIENT_UI_HOME_SAVELISTWIDGET_H
