/** @file loop.cpp
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

#include "de/Loop"
#include "de/App"
#include "de/Time"
#include "de/Timer"
#include "de/Log"
#include "de/math.h"

#include "../src/core/callbacktimer.h"

namespace de {

static Loop *loopSingleton = nullptr;

DE_PIMPL(Loop)
{
    TimeSpan     interval;
    bool         running;
    Timer        timer;
    LoopCallback mainCall;

    Impl(Public *i) : Base(i), running(false)
    {
        DE_ASSERT(!loopSingleton);
        loopSingleton = i;

        audienceForIteration.setAdditionAllowedDuringIteration(true);

        timer.audienceForTrigger() += [this]() { self().nextLoopIteration(); };
    }

    ~Impl()
    {
        loopSingleton = nullptr;
    }

    DE_PIMPL_AUDIENCE(Iteration)
};

DE_AUDIENCE_METHOD(Loop, Iteration)

Loop::Loop() : d(new Impl(this))
{}

Loop::~Loop() {}

void Loop::setRate(double freqHz)
{
    if (fequal(freqHz, 0.0))
    {
        freqHz = 1000.0;
    }
    d->interval = 1.0 / freqHz;
    d->timer.setInterval(de::max(TimeSpan(0.001), d->interval));
}

double Loop::rate() const
{
    return 1.0 / d->interval;
}

void Loop::start()
{
    d->running = true;
    d->timer.start();
}

void Loop::stop()
{
    d->running = false;
    d->timer.stop();
}

void Loop::pause()
{
    d->timer.stop();
}

void Loop::resume()
{
    d->timer.start();
}

void Loop::timer(const TimeSpan &delay, const std::function<void ()> &func)
{
    // The timer will delete itself after it's triggered.
    internal::CallbackTimer *timer = new internal::CallbackTimer(func);
    timer->start(delay);
}

void Loop::mainCall(const std::function<void ()> &func) // static
{
    if (App::inMainThread())
    {
        func();
    }
    else
    {
        Loop::get().d->mainCall.enqueue(func);
    }
}

Loop &Loop::get()
{
    DE_ASSERT(loopSingleton);
    return *loopSingleton;
}

void Loop::nextLoopIteration()
{
    try
    {
        if (d->running)
        {
            DE_FOR_AUDIENCE2(Iteration, i) i->loopIteration();
        }
    }
    catch (Error const &er)
    {
        LOG_AS("Loop");

        // This is called from Qt's event loop, we mustn't let exceptions
        // out of here uncaught.
        App::app().handleUncaughtException("Uncaught exception during loop iteration:\n" + er.asText());
    }
}

// LoopCallback -------------------------------------------------------------------------

LoopCallback::LoopCallback()
{}

LoopCallback::~LoopCallback()
{}

bool LoopCallback::isEmpty() const
{
    DE_GUARD(this);

    return _funcs.isEmpty();
}

void LoopCallback::enqueue(const Callback& func)
{
    DE_GUARD(this);

    _funcs << func;

    Loop::get().audienceForIteration() += this;
}

void LoopCallback::loopIteration()
{
    List<Callback> funcs;

    // Lock while modifying but not during the callbacks themselves.
    {
        DE_GUARD(this);
        Loop::get().audienceForIteration() -= this;

        // Make a copy of the list if new callbacks get enqueued in the callback.
        funcs = std::move(_funcs);
        _funcs.clear();
    }

    for (Callback const &cb : funcs)
    {
        cb();
    }
}

} // namespace de
