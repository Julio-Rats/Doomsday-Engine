/** @file commandlinewidget.cpp  Widget for command line input.
 *
 * @authors Copyright © 2013-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/term/CommandLineWidget"
#include "de/term/TextRootWidget"
#include "de/term/KeyEvent"
#include "de/EditorHistory"

#include <de/String>

namespace de { namespace term {

DE_PIMPL(CommandLineWidget)
{
    EditorHistory history;

    Impl(Public *i) : Base(i), history(i) {}

    DE_PIMPL_AUDIENCE(Command)
};

DE_AUDIENCE_METHOD(CommandLineWidget, Command)

CommandLineWidget::CommandLineWidget(String const &name)
    : LineEditWidget(name)
    , d(new Impl(this))
{
    setPrompt("> ");
}

bool CommandLineWidget::handleEvent(Event const &event)
{
    // There are only key press events.
    DE_ASSERT(event.type() == Event::KeyPress);
    KeyEvent const &ev = event.as<KeyEvent>();

    // Override the editor's normal Enter handling.
    if (ev.key() == Key::Enter)
    {
        String const entered = d->history.enter();
        DE_NOTIFY(Command, i) i->commandEntered(entered);
        return true;
    }

    if (LineEditWidget::handleEvent(event))
    {
        return true;
    }

    // Final fallback: history navigation.
    return d->history.handleControlKey(ev.key());
}

void CommandLineWidget::autoCompletionBegan(String const &wordBase)
{
    LineEditWidget::autoCompletionBegan(wordBase);

    LOG_MSG("Completions for '%s':") << wordBase;
    LOG_MSG("  %s") << String::join(suggestedCompletions(), ", ");
}

}} // namespace de::term
