/*
 * The Doomsday Engine Project -- libcore
 *
 * @authors Copyright © 2004-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2014 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG2_LOCKABLE_H
#define LIBDENG2_LOCKABLE_H

#include "../libcore.h"

#include <mutex>

namespace de {

/**
 * A mutex that can be used to synchronize access to a resource.  All classes
 * of lockable resources should be derived from this class. The mutex works
 * in a recursive way: if lock() is called multiple times, unlock() must be
 * called as many times.
 *
 * @ingroup concurrency
 */
class DENG2_PUBLIC Lockable
{
public:
    /// Acquire the lock.  Blocks until the operation succeeds.
    inline void lock() const {
        _mutex.lock();
    }

    /// Release the lock.
    inline void unlock() const {
        _mutex.unlock();
    }

private:
    mutable std::recursive_mutex _mutex;
};

template <typename Type>
struct LockableT : public Lockable
{
    typedef Type ValueType;
    Type value;

    LockableT() {}
    LockableT(Type const &initial) : value(initial) {}
    LockableT(Type &&initial) : value(initial) {}

    operator Type &() { return value; }
    operator Type const &() const { return value; }
};

} // namespace de

#endif // LIBDENG2_LOCKABLE_H
