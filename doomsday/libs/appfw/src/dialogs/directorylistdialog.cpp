/** @file directorylistdialog.cpp  Dialog for editing a list of directories.
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

#include "de/DirectoryListDialog"
#include "de/DirectoryArrayWidget"

#include <de/CallbackAction>

namespace de {

DENG2_PIMPL(DirectoryListDialog)
{
    struct Group
    {
        LabelWidget *title;
        LabelWidget *description;
        Variable array;
        DirectoryArrayWidget *list;
    };
    QHash<Id::Type, Group *> groups;

    Impl(Public *i) : Base(i)
    {}

    ~Impl()
    {
        qDeleteAll(groups);
    }

    Id addGroup(String const &title, String const &description)
    {
        Id groupId;
        std::unique_ptr<Group> group(new Group);

        self().area().add(group->title = new LabelWidget("group-title"));
        group->title->setText(title);
        group->title->setMaximumTextWidth(self().area().rule().width() -
                                          self().margins().width());
        group->title->setTextLineAlignment(ui::AlignLeft);
        group->title->setAlignment(ui::AlignLeft);
        group->title->setFont("separator.label");
        group->title->setTextColor("accent");
        group->title->margins().setTop("gap");

        self().area().add(group->description = new LabelWidget("group-desc"));
        group->description->setText(description);
        group->description->setFont("small");
        group->description->setTextColor("altaccent");
        group->description->margins().setTop("").setBottom("");
        group->description->setMaximumTextWidth(self().area().rule().width() -
                                                self().margins().width());
        group->description->setTextLineAlignment(ui::AlignLeft);
        group->description->setAlignment(ui::AlignLeft);
        group->description->margins().setBottom(ConstantRule::zero());

        group->array.set(new ArrayValue);
        group->list = new DirectoryArrayWidget(group->array, "group-direc-array");
        group->list->margins().setZero();
        self().add(group->list->detachAddButton(self().area().rule().width()));
        group->list->addButton().hide();
        self().area().add(group->list);

        QObject::connect(group->list, SIGNAL(arrayChanged()),
                         thisPublic, SIGNAL(arrayChanged()));

        groups.insert(groupId, group.release());
        return groupId;
    }
};

DirectoryListDialog::DirectoryListDialog(String const &name)
    : MessageDialog(name)
    , d(new Impl(this))
{
    area().enableIndicatorDraw(true);
    buttons() << new DialogButtonItem(Default | Accept)
              << new DialogButtonItem(Reject)
              << new DialogButtonItem(Action, style().images().image("create"),
                                      tr("Add Folder"),
                                      new CallbackAction([this]() {
        (*d->groups.begin())->list->addButton().trigger();
    }));
}

Id DirectoryListDialog::addGroup(String const &title, String const &description)
{
    return d->addGroup(title, description);
}

void DirectoryListDialog::prepare()
{
    MessageDialog::prepare();
    updateLayout();
}

void DirectoryListDialog::setValue(Id const &id, Value const &elements)
{
    DENG2_ASSERT(d->groups.contains(id));
    d->groups[id]->array.set(elements);
}

Value const &DirectoryListDialog::value(Id const &id) const
{
    DENG2_ASSERT(d->groups.contains(id));
    return d->groups[id]->array.value();
}

} // namespace de
