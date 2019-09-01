/** @file regexp.cpp
 *
 * @authors Copyright (c) 2018 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/RegExp"

#include <the_Foundation/object.h>

namespace de {

RegExpMatch::RegExpMatch()
{
    clear();
}

const char *RegExpMatch::begin() const
{
    return match.subject + match.range.start;
}

const char *RegExpMatch::end() const
{
    return match.subject + match.range.end;
}

void RegExpMatch::clear()
{
    zap(match);
}

String RegExpMatch::captured(int index) const
{
    return String::take(captured_RegExpMatch(&match, index));
}

CString RegExpMatch::capturedCStr(int index) const
{
    iRangecc range;
    capturedRange_RegExpMatch(&match, index, &range);
    return range;
}

//------------------------------------------------------------------------------------------------

const RegExp RegExp::WHITESPACE{"\\s+"};

RegExp::RegExp(const String &expression, Sensitivity cs)
{
    _d.reset(new_RegExp(expression, cs == CaseSensitive ? caseSensitive_RegExpOption
                                                        : caseInsensitive_RegExpOption));
}

bool RegExp::exactMatch(const String &subject) const
{
    RegExpMatch m;
    return exactMatch(subject, m);
}

bool RegExp::exactMatch(const String &subject, RegExpMatch &match) const
{
    auto &m = match.match;
    if (matchString_RegExp(_d, subject, &m))
    {
        return m.range.start == 0 && m.range.end == subject.sizei();
    }
    return false;
}

bool RegExp::match(const String &subject, RegExpMatch &match) const
{
    return matchString_RegExp(_d, subject, &match.match);
}

bool RegExp::hasMatch(const String &subject) const
{
    iRegExpMatch match{};
    return matchString_RegExp(_d, subject, &match);
}

} // namespace de
