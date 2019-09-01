/*
 * The Doomsday Engine Project -- libcore
 *
 * Copyright © 2004-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/Context"
#include "de/Statement"
#include "de/Process"
#include "de/RecordValue"

namespace de {

DENG2_PIMPL(Context)
{
    /**
     * Information about the control flow is stored within a stack of
     * ControlFlow instances.
     */
    class ControlFlow {
    public:
        /**
         * Constructor.
         *
         * @param current  Current statement being executed.
         * @param f        Statement where normal flow continues.
         * @param c        Statement where to jump on "continue".
         *                 @c NULL if continuing is not allowed.
         * @param b        Statement where to jump to and flow from on "break".
         *                 @c NULL if breaking is not allowed.
         */
        ControlFlow(Statement const *current,
                    Statement const *f = 0,
                    Statement const *c = 0,
                    Statement const *b = 0)
            : flow(f), jumpContinue(c), jumpBreak(b), iteration(0), _current(current) {}

        /// Returns the currently executed statement.
        Statement const *current() const { return _current; }

        /// Sets the currently executed statement. When the statement
        /// changes, the phase is reset back to zero.
        void setCurrent(Statement const *s) { _current = s; }

    public:
        Statement const *flow;
        Statement const *jumpContinue;
        Statement const *jumpBreak;
        Value *iteration; // owned

    private:
        Statement const *_current;
    };

    /// Type of the execution context.
    Type type;

    /// The process that owns this context.
    Process *owner;

    /// Control flow stack.
    typedef std::vector<ControlFlow> FlowStack;
    FlowStack controlFlow;

    /// Expression evaluator.
    Evaluator evaluator;

    /// Determines whether the namespace is owned by the context.
    bool ownsNamespace;

    /// The local namespace of this context.
    Record *names;

    QScopedPointer<Value> nativeSelf;

    Variable throwaway;

    Impl(Public *i, Type type, Process *owner, Record *globals)
        : Base(i)
        , type(type)
        , owner(owner)
        , evaluator(*i)
        , ownsNamespace(false)
        , names(globals)
    {
        if (!names)
        {
            // Create a private empty namespace.
            DENG2_ASSERT(type != GlobalNamespace);
            names = new Record;
            ownsNamespace = true;
        }
    }

    ~Impl()
    {
        if (ownsNamespace)
        {
            delete names;
        }
        self().reset();
        DENG2_ASSERT(controlFlow.empty());
    }

    /// Returns the topmost control flow information.
    ControlFlow &flow()
    {
        return controlFlow.back();
    }

    /// Pops the topmost control flow instance off of the stack. The
    /// iteration value is deleted, if it has been defined.
    void popFlow()
    {
        DENG2_ASSERT(!controlFlow.empty());
        delete flow().iteration;
        controlFlow.pop_back();
    }

    /// Sets the currently executed statement.
    void setCurrent(Statement const *statement)
    {
        if (controlFlow.size())
        {
            evaluator.reset();
            flow().setCurrent(statement);
        }
        else
        {
            DENG2_ASSERT(statement == NULL);
        }
    }
};

Context::Context(Type type, Process *owner, Record *globals)
    : d(new Impl(this, type, owner, globals))
{}

Context::Type Context::type()
{
    return d->type;
}

Process &Context::process()
{
    return *d->owner;
}

Evaluator &Context::evaluator()
{
    return d->evaluator;
}

bool Context::hasExternalGlobalNamespace() const
{
    return !d->ownsNamespace;
}

Record &Context::names()
{
    return *d->names;
}

void Context::start(Statement const *statement,    Statement const *fallback,
                    Statement const *jumpContinue, Statement const *jumpBreak)
{
    d->controlFlow.push_back(Impl::ControlFlow(statement, fallback, jumpContinue, jumpBreak));

    // When the current statement is NULL it means that the sequence of statements
    // has ended, so we shouldn't do that until there really is no more statements.
    if (!current())
    {
        proceed();
    }
}

void Context::reset()
{
    while (!d->controlFlow.empty())
    {
        d->popFlow();
    }
    d->evaluator.reset();
}

bool Context::execute()
{
    if (current() != NULL)
    {
        current()->execute(*this);
        return true;
    }
    return false;
}

void Context::proceed()
{
    Statement const *st = NULL;
    if (current())
    {
        st = current()->next();
    }
    // Should we fall back to a point that was specified earlier?
    while (!st && d->controlFlow.size())
    {
        st = d->controlFlow.back().flow;
        d->popFlow();
    }
    d->setCurrent(st);
}

void Context::jumpContinue()
{
    Statement const *st = NULL;
    while (!st && d->controlFlow.size())
    {
        st = d->controlFlow.back().jumpContinue;
        d->popFlow();
    }
    if (!st)
    {
        throw JumpError("Context::jumpContinue", "No jump targets defined for continue");
    }
    d->setCurrent(st);
}

void Context::jumpBreak(duint count)
{
    if (count < 1)
    {
        throw JumpError("Context::jumpBreak", "Invalid number of nested breaks");
    }

    Statement const *st = NULL;
    while ((!st || count > 0) && d->controlFlow.size())
    {
        st = d->controlFlow.back().jumpBreak;
        if (st)
        {
            --count;
        }
        d->popFlow();
    }
    if (count > 0)
    {
        throw JumpError("Context::jumpBreak", "Too few nested compounds to break out of");
    }
    if (!st)
    {
        throw JumpError("Context::jumpBreak", "No jump targets defined for break");
    }
    d->setCurrent(st);
    proceed();
}

Statement const *Context::current()
{
    if (d->controlFlow.size())
    {
        return d->flow().current();
    }
    return NULL;
}

Value *Context::iterationValue()
{
    DENG2_ASSERT(d->controlFlow.size());
    return d->controlFlow.back().iteration;
}

void Context::setIterationValue(Value *value)
{
    DENG2_ASSERT(d->controlFlow.size());

    Impl::ControlFlow &fl = d->flow();
    if (fl.iteration)
    {
        delete fl.iteration;
    }
    fl.iteration = value;
}

void Context::setNativeSelf(Value *scope)
{
    d->nativeSelf.reset(scope);
}

Value &Context::nativeSelf() const
{
    DENG2_ASSERT(!d->nativeSelf.isNull());
    if (d->nativeSelf.isNull())
    {
        throw UndefinedScopeError("Context::nativeSelf",
                                  "Context is not executing in scope of any instance");
    }
    return *d->nativeSelf;
}

Record &Context::selfInstance() const
{
    Record *obj = nativeSelf().as<RecordValue>().record();
    if (!obj)
    {
        throw UndefinedScopeError("Context::selfInstance", "No \"self\" instance has been set");
    }
    return *obj;
}

Variable &Context::throwaway()
{
    return d->throwaway;
}

} // namespace de
