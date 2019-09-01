/** @file gltimer.cpp  GL timer.
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

#include "de/GLTimer"
#include "de/graphics/opengl.h"
#include "de/GLInfo"

#include <QHash>

namespace de {

DE_PIMPL_NOREF(GLTimer)
{
    static const int BUF_COUNT = 2;

    struct Query {
        struct Measurement {
            GLuint id;
        } measurements[BUF_COUNT] {};
        int head = 0;
        int tail = 0;

        bool isEmpty() const { return head == tail; }

        Measurement &front() { return measurements[head]; }
        Measurement &back()  { return measurements[tail]; }
        const Measurement &front() const { return measurements[head]; }
        const Measurement &back() const  { return measurements[tail]; }

        bool pushFront()
        {
            int h = (head + 1) % BUF_COUNT;
            if (h == tail) return false; // Full.
            head = h;
            return true;
        }

        bool popBack()
        {
            if (head == tail) return false;
            tail = (tail + 1) % BUF_COUNT;
            return true;
        }
    };
    QHash<Id::Type, Query> queries;
    Id measuring{Id::None};

    ~Impl()
    {
        auto &GL = LIBGUI_GL;
        for (auto i = queries.begin(), end = queries.end(); i != end; ++i)
        {
            for (auto &ms : i.value().measurements)
            {
                GL.glDeleteQueries(1, &ms.id);
            }
        }
    }
};

GLTimer::GLTimer()
    : d(new Impl)
{}

void GLTimer::beginTimer(const Id &id)
{
    DE_ASSERT(!d->measuring);
    if (d->measuring) return;

    auto &GL = LIBGUI_GL;
    auto &query = d->queries[id];
    if (query.pushFront())
    {
        auto &ms = query.front();
        if (!ms.id)
        {
            GL.glGenQueries(1, &ms.id);
        }
        GL.glBeginQuery(GL_TIME_ELAPSED, ms.id);
        d->measuring = id;
    }
    LIBGUI_ASSERT_GL_OK();
}

void GLTimer::endTimer(const Id &id)
{
    if (d->measuring != id) return;

    auto found = d->queries.constFind(id);
    if (found != d->queries.constEnd())
    {
        auto &query = found.value();
        if (!query.isEmpty())
        {
            LIBGUI_GL.glEndQuery(GL_TIME_ELAPSED);
            d->measuring = Id::None;
        }
    }
    LIBGUI_ASSERT_GL_OK();
}

TimeSpan GLTimer::elapsedTime(const Id &id) const
{
    auto &GL = LIBGUI_GL;
    auto found = d->queries.find(id);
    if (found != d->queries.end())
    {
        auto &query = found.value();
        if (query.isEmpty()) return 0.0;

        const auto &ms = query.back();
        if (!ms.id)
        {
            query.popBack();
            return 0.0;
        }

        GLint isAvailable;
        GL.glGetQueryObjectiv(ms.id, GL_QUERY_RESULT_AVAILABLE, &isAvailable);
        LIBGUI_ASSERT_GL_OK();

        if (isAvailable)
        {
            GLuint64 nanosecs = 0;
            GL.glGetQueryObjectui64v(ms.id, GL_QUERY_RESULT, &nanosecs);
            LIBGUI_ASSERT_GL_OK();
            query.popBack();
            return double(nanosecs) / 1.0e9;
        }
    }
    return 0.0;
}

} // namespace de
