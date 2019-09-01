/** @file widgets/choicewidget.cpp  Widget for choosing from a set of alternatives.
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

#include "de/ChoiceWidget"
#include "de/PopupMenuWidget"
#include "de/SignalAction"

namespace de {

using namespace ui;

DENG_GUI_PIMPL(ChoiceWidget),
DENG2_OBSERVES(Data, Addition),
DENG2_OBSERVES(Data, Removal),
DENG2_OBSERVES(Data, OrderChange),
DENG2_OBSERVES(ChildWidgetOrganizer, WidgetCreation),
DENG2_OBSERVES(ChildWidgetOrganizer, WidgetUpdate)
{
    /**
     * Items in the choice's popup uses this as action to change the selected
     * item.
     */
    struct SelectAction : public de::Action
    {
        ChoiceWidget::Impl *wd;
        ui::Item const &selItem;

        SelectAction(ChoiceWidget::Impl *inst, ui::Item const &item)
            : wd(inst), selItem(item) {}

        void trigger()
        {
            Action::trigger();
            wd->selected = wd->items().find(selItem);
            wd->updateButtonWithSelection();
            wd->updateItemHighlight();
            wd->choices->dismiss();

            emit wd->self().selectionChangedByUser(wd->selected);
        }
    };

    PopupMenuWidget *choices;
    IndirectRule *   maxWidth;
    Data::Pos        selected; ///< One item is always selected.
    String           noSelectionHint;

    Impl(Public *i) : Base(i), selected(Data::InvalidPos)
    {
        maxWidth = new IndirectRule;

        self().setMaximumTextWidth(rule("choice.item.width.max"));
        self().setTextLineAlignment(ui::AlignLeft);
        self().setFont("choice.selected");

        choices = new PopupMenuWidget;
        choices->items().audienceForAddition() += this;
        choices->items().audienceForRemoval() += this;
        choices->items().audienceForOrderChange() += this;
        choices->menu().organizer().audienceForWidgetCreation() += this;
        choices->menu().organizer().audienceForWidgetUpdate() += this;
        self().add(choices);

        self().setPopup(*choices, ui::Right);

        QObject::connect(choices, &PanelWidget::opened, [this] ()
        {
            // Focus the selected item when the popup menu is opened.
            if (auto *w = choices->menu().itemWidget<GuiWidget>(selected))
            {
                root().setFocus(w);
            }
        });

        updateButtonWithSelection();
        updateStyle();
    }

    ~Impl()
    {
        releaseRef(maxWidth);
    }

    void updateStyle()
    {
        // Popup background color.
        choices->set(choices->background().withSolidFill(style().colors().colorf("choice.popup")));
    }

    void widgetCreatedForItem(GuiWidget &widget, ui::Item const &item)
    {
        if (auto *label = maybeAs<LabelWidget>(widget))
        {
            label->setMaximumTextWidth(rule("choice.item.width.max"));
        }
        if (auto *but = maybeAs<ButtonWidget>(widget))
        {
            // Make sure the created buttons have an action that updates the
            // selected item.
            but->setAction(new SelectAction(this, item));
        }
    }

    void widgetUpdatedForItem(GuiWidget &, ui::Item const &item)
    {
        if (isValidSelection() && &item == &self().selectedItem())
        {
            // Make sure the button is up to date, too.
            updateButtonWithItem(self().selectedItem());
        }
    }

    void updateMaximumWidth()
    {
        // We'll need to calculate this manually because the fonts keep changing due to
        // selection and thus we can't just check the current layout.
        Font const &font = self().font();
        int widest = 0;
        for (uint i = 0; i < items().size(); ++i)
        {
            EscapeParser esc;
            esc.parse(items().at(i).label());
            widest = de::max(widest, font.advanceWidth(esc.plainText()));
        }
        maxWidth->setSource(OperatorRule::minimum(rule("choice.item.width.max"),
                                                  Const(widest) + self().margins().width()));
    }

    Data const &items() const
    {
        return choices->items();
    }

    bool isValidSelection() const
    {
        return selected < items().size();
    }

    void dataItemAdded(Data::Pos id, ui::Item const &)
    {
        updateMaximumWidth();

        if (selected >= items().size())
        {
            // If the previous selection was invalid, make a valid one now.
            selected = 0;

            updateButtonWithSelection();
            return;
        }

        if (id <= selected)
        {
            // New item added before/at the selection.
            selected++;
        }
    }

    void dataItemRemoved(Data::Pos id, ui::Item &)
    {
        if (id <= selected && selected > 0)
        {
            selected--;
        }

        updateButtonWithSelection();
        updateMaximumWidth();
    }

    void dataItemOrderChanged()
    {
        updateButtonWithSelection();
    }

    void updateItemHighlight()
    {
        // Highlight the currently selected item.
        for (Data::Pos i = 0; i < items().size(); ++i)
        {
            if (GuiWidget *w = choices->menu().organizer().itemWidget(i))
            {
                w->setFont(i == selected? "choice.selected" : "default");
            }
        }
    }

    void updateButtonWithItem(ui::Item const &item)
    {
        self().setText(item.label());

        ActionItem const *act = dynamic_cast<ActionItem const *>(&item);
        if (act)
        {
            self().setImage(act->image());
        }
    }

    void updateButtonWithSelection()
    {
        // Update the main button.
        if (isValidSelection())
        {
            updateButtonWithItem(items().at(selected));
        }
        else
        {
            // No valid selection.
            self().setText(noSelectionHint);
            self().setImage(Image());
        }

        emit self().selectionChanged(selected);
    }
};

ChoiceWidget::ChoiceWidget(String const &name)
    : PopupButtonWidget(name), d(new Impl(this))
{
    setOpeningDirection(ui::Right);
    d->choices->setAllowDirectionFlip(false);
}

PopupMenuWidget &ChoiceWidget::popup()
{
    return *d->choices;
}

void ChoiceWidget::setSelected(Data::Pos pos)
{
    d->selected = pos;
    d->updateButtonWithSelection();
    d->updateItemHighlight();
}

bool ChoiceWidget::isValidSelection() const
{
    return d->isValidSelection();
}

Data::Pos ChoiceWidget::selected() const
{
    return d->selected;
}

Item const &ChoiceWidget::selectedItem() const
{
    DENG2_ASSERT(d->isValidSelection());
    return d->items().at(d->selected);
}

Rule const &ChoiceWidget::maximumWidth() const
{
    return *d->maxWidth;
}

void ChoiceWidget::openPopup()
{
    d->updateItemHighlight();
    d->choices->open();
}

ui::Data &ChoiceWidget::items()
{
    return d->choices->items();
}

void ChoiceWidget::setItems(Data const &items)
{
    popup().menu().setItems(items);
    d->updateMaximumWidth();
}

void ChoiceWidget::setNoSelectionHint(String const &hint)
{
    d->noSelectionHint = hint;
}

void ChoiceWidget::useDefaultItems()
{
    popup().menu().useDefaultItems();
    d->updateMaximumWidth();
}

} // namespace de
