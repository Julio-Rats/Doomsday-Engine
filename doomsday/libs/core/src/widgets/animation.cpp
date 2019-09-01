/** @file animation.h Animation function.
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

#include "de/Animation"
#include "de/Writer"
#include "de/Reader"
#include "de/math.h"

#include <atomic>

namespace de {

static float const DEFAULT_SPRING = 3.f;

namespace internal {

static inline float easeOut(TimeSpan t)
{
    return t * (2 - t);
}

static inline float easeOutSofter(TimeSpan t)
{
    const float a = -std::pow(t - 1.0, 4) + 1;
    return (easeOut(t) + a) / 2.0f;
}

static inline float easeIn(TimeSpan t)
{
    return t * t;
}

static inline float easeBoth(TimeSpan t)
{
    if (t < .5)
    {
        // First half acceleretes.
        return easeIn(t * 2.0) / 2.0;
    }
    else
    {
        // Second half decelerates.
        return .5 + easeOut((t - .5) * 2.0) / 2.0;
    }
}

enum AnimationFlag
{
    Paused = 0x1,
    Finished = 0x2,
};
Q_DECLARE_FLAGS(AnimationFlags, AnimationFlag)
Q_DECLARE_OPERATORS_FOR_FLAGS(AnimationFlags)

/// Thread-safe current time for animations.
struct AnimationTime : DENG2_OBSERVES(Clock, TimeChange) {
    double now;
    void timeChanged(Clock const &clock) override {
        now = clock.time().highPerformanceTime();
    }
};
static AnimationTime theTime;

} // namespace internal
using namespace internal;

/// Global animation time source.
Clock const *Animation::_clock = 0;

DENG2_PIMPL_NOREF(Animation)
{
    float value;
    float target;
    TimeSpan startDelay;
    TimeSpan setTime;
    TimeSpan targetTime;
    TimeSpan pauseTime;
    Style style;
    float spring;
    mutable AnimationFlags flags;

    Impl(float val, Style s)
        : value(val)
        , target(val)
        , startDelay(0)
        , setTime   (theTime.now)
        , targetTime(theTime.now)
        , style(s)
        , spring(DEFAULT_SPRING)
    {}

    /**
     * Calculates the value of the animation.
     * @param now  Point of time at which to evaluate.
     * @return Value of the animation when the frame time is @a now.
     */
    float valueAt(TimeSpan const &now) const
    {
        TimeSpan span = targetTime - setTime;

        float s2 = 0;
        TimeSpan peak = 0;
        TimeSpan peak2 = 0;

        // Spring values.
        if (style == Bounce || style == FixedBounce)
        {
            s2 = spring * spring;
            peak = 1.f/3;
            peak2 = 2.f/3;
        }

        if (now >= targetTime || span <= 0)
        {
            flags |= Finished;
            return target;
        }
        else
        {
            span -= startDelay;
            TimeSpan const elapsed = now - setTime - startDelay;
            TimeSpan const t = clamp(0.0, elapsed/span, 1.0);
            float const delta = target - value;
            switch (style)
            {
            case EaseOut:
                return value + easeOut(t) * delta;

            case EaseOutSofter:
                return value + easeOutSofter(t) * delta;

            case EaseIn:
                return value + easeIn(t) * delta;

            case EaseBoth:
                return value + easeBoth(t) * delta;

            case Bounce:
            case FixedBounce:
            {
                const float bounce1 = (style == Bounce? delta/spring : (delta>=0? spring : -spring));
                const float bounce2 = (style == Bounce? delta/s2 : (delta>=0? spring/2 : -spring/2));
                float peakDelta = (delta + bounce1);

                if (t < peak)
                {
                    return value + easeOut(t/peak) * peakDelta;
                }
                else if (t < peak2)
                {
                    return (value + peakDelta) - easeBoth((t - peak)/(peak2 - peak)) * (bounce1 + bounce2);
                }
                else
                {
                    return (target - bounce2) + easeBoth((t - peak2)/(1 - peak2)) * bounce2;
                }
                break;
            }

            default:
                return value + t * delta;
            }
        }

        return target;
    }

    void checkDone()
    {
        if (!(flags & Finished) && currentTime() >= targetTime)
        {
            flags |= Finished;
        }
    }

    double currentTime() const
    {
        if (flags & Paused) return pauseTime;
        return theTime.now; //Animation::currentTime();
    }
};

Animation::Animation(float val, Style s) : d(new Impl(val, s))
{}

Animation::Animation(Animation const &other)
    : de::ISerializable()
    , d(new Impl(*other.d))
{}

Animation &Animation::operator = (Animation const &other)
{
    d.reset(new Impl(*other.d));
    return *this;
}

void Animation::setStyle(Animation::Style s)
{
    d->style = s;
}

void Animation::setStyle(Style style, float bounce)
{
    d->style = style;
    d->spring = bounce;
    if (!d->spring)
    {
        d->spring = DEFAULT_SPRING;
    }
}

Animation::Style Animation::style() const
{
    return d->style;
}

float Animation::bounce() const
{
    return d->spring;
}

void Animation::setValue(float v, TimeSpan transitionSpan, TimeSpan startDelay)
{
    resume();

    TimeSpan const now = d->currentTime();

    if (transitionSpan <= 0)
    {
        d->value = d->target = v;
        d->setTime = d->targetTime = now;
        d->flags |= Finished;
    }
    else
    {
        d->value = d->valueAt(now);
        d->target = v;
        d->setTime = now;
        d->targetTime = d->setTime + transitionSpan;
        d->flags &= ~Finished;
    }
    d->startDelay = startDelay;
}

void Animation::setValue(int v, TimeSpan transitionSpan, TimeSpan startDelay)
{
    setValue(float(v), transitionSpan, startDelay);
}

void Animation::setValueFrom(float fromValue, float toValue, TimeSpan transitionSpan, TimeSpan startDelay)
{
    setValue(fromValue);
    setValue(toValue, transitionSpan, startDelay);
}

float Animation::value() const
{
    if (d->flags & Paused)
    {
        return d->valueAt(d->pauseTime);
    }
    if (d->flags & Finished)
    {
        return d->target;
    }
    return d->valueAt(theTime.now);
}

bool Animation::done() const
{
    d->checkDone();
    if (d->flags & Finished) return true;
    return false;
}

float Animation::target() const
{
    return d->target;
}

void Animation::adjustTarget(float newTarget)
{
    d->target = newTarget;
}

TimeSpan Animation::remainingTime() const
{
    TimeSpan const now = d->currentTime();
    if (now >= d->targetTime)
    {
        return 0;
    }
    return d->targetTime - now;
}

TimeSpan Animation::transitionTime() const
{
    return d->targetTime - d->setTime;
}

void Animation::shift(float valueDelta)
{
    d->value  += valueDelta;
    d->target += valueDelta;
}

void Animation::pause()
{
    if (d->flags.testFlag(Paused) || done()) return;

    d->pauseTime = d->currentTime();
    d->flags |= Paused;
}

void Animation::resume()
{
    if (!d->flags.testFlag(Paused)) return;

    d->flags &= ~Paused;

    TimeSpan const delta = d->currentTime() - d->pauseTime;
    d->setTime    += delta;
    d->targetTime += delta;
}

void Animation::finish()
{
    setValue(d->target);
}

String Animation::asText() const
{
    return String("Animation(%1 -> %2, ETA:%3 s; curr: %4)").arg(d->value).arg(d->target).arg(remainingTime()).arg(value());
}

Clock const &Animation::clock()
{
    DENG2_ASSERT(_clock != 0);
    if (!_clock)
    {
        throw ClockMissingError("Animation::clock", "Animation has no clock");
    }
    return *_clock;
}

void Animation::operator >> (Writer &to) const
{
    TimeSpan const now = currentTime();

    to << d->value << d->target;
    // Write times relative to current frame time.
    to << (d->setTime - now) << (d->targetTime - now);
    to << d->startDelay;
    to << dint32(d->style) << d->spring;
}

void Animation::operator << (Reader &from)
{
    TimeSpan const now = currentTime();

    from >> d->value >> d->target;

    // Times are relative to current frame time.
    TimeSpan relSet, relTarget;
    from >> relSet >> relTarget;

    d->setTime    = now + relSet;
    d->targetTime = now + relTarget;

    from >> d->startDelay;

    dint32 s;
    from >> s;
    d->style = Style(s);

    from >> d->spring;
}

void Animation::setClock(Clock const *clock)
{
    if (_clock) _clock->audienceForPriorityTimeChange -= theTime;
    _clock = clock;
    if (clock) clock->audienceForPriorityTimeChange += theTime;
}
   
TimeSpan Animation::currentTime() // static
{
    DENG2_ASSERT(_clock != 0);
    if (!_clock)
    {
        throw ClockMissingError("Animation::clock", "Animation has no clock");
    }
    return TimeSpan(theTime.now);
}

Animation Animation::range(Style style, float from, float to, TimeSpan span, TimeSpan delay)
{
    Animation anim(from, style);
    anim.setValue(to, span, delay);
    return anim;
}

} // namespace de
