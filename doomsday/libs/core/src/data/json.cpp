/** @file json.cpp  JSON parser and composer.
 *
 * Parses JSON and outputs a QVariant with the data.
 *
 * @authors Copyright © 2012-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/data/json.h"
#include <QVarLengthArray>
#include <de/Block>
#include <de/Error>
#include <de/Log>
#include <de/Record>
#include <de/NoneValue>
#include <de/NumberValue>
#include <de/RecordValue>
#include <de/DictionaryValue>
#include <de/ArrayValue>
#include <QTextStream>
#include <QDebug>

namespace de {

namespace internal {

/**
 * @internal Not exposed outside this source file; use parseJSON() instead.
 */
class JSONParser
{
    QString const &source;
    int pos;

public:
    JSONParser(QString const &s) : source(s), pos(0)
    {
        skipWhite();
    }

    void advance()
    {
        pos++;
        skipWhite();
    }

    void skipWhite()
    {
        while (!atEnd() && source[pos].isSpace()) pos++;
    }

    bool atEnd() const
    {
        return pos >= source.size();
    }

    QChar peek() const
    {
        if (atEnd()) return 0;
        return source[pos];
    }

    QChar next()
    {
        if (atEnd()) return 0;
        QChar c = source[pos];
        advance();
        return c;
    }

    QChar nextNoSkip()
    {
        if (atEnd()) return 0;
        return source[pos++];
    }

    DENG2_NORETURN void error(QString const &message)
    {
        throw de::Error("JSONParser", de::String("Error at position %1 (%2^%3): %4")
                        .arg(pos).arg(source.mid(pos - 4, 4)).arg(source.mid(pos, 4)).arg(message));
    }

    QVariant parse()
    {
        LOG_AS("JSONParser");
        if (atEnd()) return QVariant();
        QChar c = peek();
        if (c == '{')
        {
            return parseObject();
        }
        else if (c == '[')
        {
            return parseArray();
        }
        else if (c == '\"')
        {
            return parseString();
        }
        else if (c == '-' || c.isDigit())
        {
            return parseNumber();
        }
        else
        {
            return parseKeyword();
        }
    }

    QVariant parseObject()
    {
        QVariantMap result;
        QChar c = next();
        DENG2_ASSERT(c == '{');
        forever
        {
            if (peek() == '}')
            {
                // Totally empty.
                break;
            }
            QString name = parseString().toString();
            c = next();
            if (c != ':') error("object keys and values must be separated by a colon");
            QVariant value = parse();
            // Add to the result.
            result.insert(name, value);
            // Move forward.
            skipWhite();
            c = next();
            if (c == '}')
            {
                // End of object.
                break;
            }
            else if (c != ',')
            {
                LOG_DEBUG(de::String("got %1 instead of ,").arg(c));
                error("key/value pairs must be separated by comma");
            }
        }
        return result;
    }

    QVariant parseArray()
    {
        QVariantList result;
        QChar c = next();
        DENG2_ASSERT(c == '[');
        if (peek() == ']')
        {
            // Empty list.
            next();
            return result;
        }
        forever
        {
            result << parse();
            c = next();
            if (c == ']')
            {
                // End of array.
                break;
            }
            else if (c != ',')
            {
                // What?
                error("array items must be separated by comma");
            }
        }
        return result;
    }

    QVariant parseString()
    {
        QVarLengthArray<QChar, 1024> result;
        QChar c = next();
        DENG2_ASSERT(c == '\"');
        forever
        {
            c = nextNoSkip();
            if (c == '\\')
            {
                // Escape.
                c = nextNoSkip();
                if (c == '\"' || c == '\\' || c == '/')
                    result.append(c);
                else if (c == 'b')
                    result.append('\b');
                else if (c == 'f')
                    result.append('\f');
                else if (c == 'n')
                    result.append('\n');
                else if (c == 'r')
                    result.append('\r');
                else if (c == 't')
                    result.append('\t');
                else if (c == 'u')
                {
                    QString code = source.mid(pos, 4);
                    pos += 4;
                    result.append(QChar(ushort(code.toLong(0, 16))));
                }
                else error("unknown escape sequence in string");
            }
            else if (c == '\"')
            {
                // End of string.
                break;
            }
            else
            {
                result.append(c);
            }
        }
        skipWhite();
        return QString(result.constData(), result.size());
    }

    QVariant parseNumber()
    {
        QVarLengthArray<QChar> str;
        QChar c = next();
        if (c == '-')
        {
            str.append(c);
            c = nextNoSkip();
        }
        for (; c.isDigit(); c = nextNoSkip())
        {
            str.append(c);
        }
        bool hasDecimal = false;
        if (c == '.')
        {
            str.append(c);
            hasDecimal = true;
            c = nextNoSkip();
            for (; c.isDigit(); c = nextNoSkip())
            {
                str.append(c);
            }
        }
        if (c == 'e' || c == 'E')
        {
            // Exponent.
            str.append(c);
            c = nextNoSkip();
            if (c == '+' || c == '-')
            {
                str.append(c);
                c = nextNoSkip();
            }
            for (; c.isDigit(); c = nextNoSkip())
            {
                str.append(c);
            }
        }
        // Rewind one char (the loop was broken when a non-digit was read).
        pos--;
        skipWhite();
        double value = QString(str.constData(), str.size()).toDouble();
        if (hasDecimal)
        {
            return QVariant(value);
        }
        else
        {
            return QVariant(int(value));
        }
    }

    QVariant parseKeyword()
    {
        if (source.mid(pos, 4) == "true")
        {
            pos += 4;
            skipWhite();
            return QVariant(true);
        }
        else if (source.mid(pos, 5) == "false")
        {
            pos += 5;
            skipWhite();
            return QVariant(false);
        }
        else if (source.mid(pos, 4) == "null")
        {
            pos += 4;
            skipWhite();
            return QVariant();
        }
        else
        {
            error("unknown keyword");
        }
        return QVariant();
    }
};

//---------------------------------------------------------------------------------------

static Block recordToJSON(Record const &rec);
static Block valueToJSON(Value const &value);

static Block valueToJSONWithTabNewlines(Value const &value)
{
    Block json = valueToJSON(value);
    json.replace('\n', "\n\t");
    return json;
}

static Block valueToJSON(Value const &value)
{
    if (is<NoneValue>(value))
    {
        return "null";
    }
    if (auto const *rec = maybeAs<RecordValue>(value))
    {
        return recordToJSON(rec->dereference());
    }
    if (auto const *dict = maybeAs<DictionaryValue>(value))
    {
        Block out = "{";
        auto const &elems = dict->elements();
        for (auto i = elems.begin(); i != elems.end(); ++i)
        {
            if (i != elems.begin())
            {
                out += ",";
            }
            out += "\n\t" + valueToJSON(*i->first.value) + ": " +
                   valueToJSONWithTabNewlines(*i->second);
        }
        return out + "\n}";
    }
    if (auto const *array = maybeAs<ArrayValue>(value))
    {
        Block out = "[";
        auto const &elems = array->elements();
        for (auto i = elems.begin(); i != elems.end(); ++i)
        {
            if (i != elems.begin())
            {
                out += ",";
            }
            out += "\n\t" + valueToJSONWithTabNewlines(**i);
        }
        return out + "\n]";
    }
    if (auto const *num = maybeAs<NumberValue>(value))
    {
        if (num->semanticHints().testFlag(NumberValue::Boolean))
        {
            return num->isTrue()? "true" : "false";
        }
        return num->asText().toUtf8();
    }

    // Text string.
    String text = value.asText();
    text.replace("\\", "\\\\")
        .replace("\"", "\\\"")
        .replace("\b", "\\b")
        .replace("\f", "\\f")
        .replace("\n", "\\n")
        .replace("\r", "\\r")
        .replace("\t", "\\t");
    return "\"" + text.toUtf8() + "\"";
}

static Block recordToJSON(Record const &rec)
{
    Block out = "{\n\t\"__obj__\": \"Record\"";
    rec.forMembers([&out] (String const &name, Variable const &var)
    {
        out += ",\n\t\"" + name.toUtf8() + "\": " + valueToJSONWithTabNewlines(var.value());
        return LoopContinue;
    });
    return out + "\n}";
}

} // internal

QVariant parseJSON(String const &jsonText)
{
    try
    {
        return internal::JSONParser(jsonText).parse();
    }
    catch (de::Error const &er)
    {
        LOG_WARNING(er.asText());
        return QVariant(); // invalid
    }
}

Block composeJSON(Record const &rec)
{
    return internal::recordToJSON(rec) + "\n";
}

} // de
