/*
 * The Doomsday Engine Project
 *
 * Copyright © 2011-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBDENG2_ANIMATIONRULE_H
#define LIBDENG2_ANIMATIONRULE_H

#include "rule.h"

#include "../Animation"
#include "../Time"

#include <QFlags>

namespace de {

/**
 * Rule with an animated value. The value is animated over time using de::Animation.
 * @ingroup widgets
 */
class DENG2_PUBLIC AnimationRule : public Rule, DENG2_OBSERVES(Clock, TimeChange)
{
public:
    explicit AnimationRule(float initialValue, Animation::Style style = Animation::EaseOut);

    /**
     * Constructs an AnimationRule whose value will animate to the specified target
     * rule. If the animation finishes and the target changes, a new animation begins
     * using the same transition time.
     *
     * @param target      Target value.
     * @param transition  Transition time to reach the current or future target values.
     * @param style       Animation style.
     */
    explicit AnimationRule(Rule const &target, TimeSpan transition, Animation::Style style = Animation::EaseOut);

    void set(float target, TimeSpan transition = 0, TimeSpan delay = 0);

    void set(Rule const &target, TimeSpan transition = 0, TimeSpan delay = 0);

    /**
     * Sets the animation style of the rule.
     *
     * @param style  Animation style.
     */
    void setStyle(Animation::Style style);

    void setStyle(Animation::Style style, float bounceSpring);

    enum Behavior {
        Singleshot               = 0x1,
        RestartWhenTargetChanges = 0x2,
        DontAnimateFromZero      = 0x4,
    };
    Q_DECLARE_FLAGS(Behaviors, Behavior)

    /**
     * Sets the behavior for updating the animation target. The default is Singleshot,
     * which means that if the target rule changes, the animation of the AnimationRule
     * will adjust its target accordingly without altering the ongoing or finished
     * animation.
     *
     * @param behavior  Target update behavior.
     */
    void setBehavior(Behaviors behavior);

    Behaviors behavior() const;

    /**
     * Read-only access to the scalar animation.
     */
    Animation const &animation() const {
        return _animation;
    }

    /**
     * Shifts the scalar rule's animation target and current value without
     * affecting any ongoing animation.
     *
     * @param delta  Value delta for the shift.
     */
    void shift(float delta);

    void finish();

    void pause();

    void resume();

    String description() const;

protected:
    ~AnimationRule();
    void update();

    void timeChanged(Clock const &);

private:
    Animation _animation;
    Rule const *_targetRule;
    Behaviors _behavior;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(AnimationRule::Behaviors)

} // namespace de

#endif // LIBDENG2_ANIMATIONRULE_H
