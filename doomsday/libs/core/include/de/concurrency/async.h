/** @file asynctask.h  Asynchoronous task with a completion callback.
 *
 * @authors Copyright (c) 2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBDENG2_ASYNCTASK_H
#define LIBDENG2_ASYNCTASK_H

#include "../App"
#include "../Loop"
#include "../String"

#include <QThread>
#include <atomic>
#include <utility>

namespace de {

struct DENG2_PUBLIC AsyncTask : public QThread
{
    virtual ~AsyncTask() {}
    virtual void abort() = 0;
    virtual void invalidate() = 0;
};

namespace internal {

template <typename Task, typename Completion>
class AsyncTaskThread : public AsyncTask
{
    Task task;
    decltype(task()) result {}; // can't be void
    Completion completion;
    bool valid;

    void run() override
    {
        try
        {
            result = task();
        }
        catch (...)
        {}
        notifyCompletion();
    }

    void notifyCompletion()
    {
        Loop::mainCall([this] ()
        {
            if (valid) completion(result);
            deleteLater();
        });
    }

    void invalidate() override
    {
        valid = false;
    }

public:
    AsyncTaskThread(Task task, Completion completion)
        : task(std::move(task))
        , completion(std::move(completion))
        , valid(true)
    {}

    AsyncTaskThread(Task const &task)
        : task(task)
        , valid(false)
    {}

    void abort() override
    {
        terminate();
        notifyCompletion();
    }
};

} // namespace internal

/**
 * Executes an asynchronous callback in a background thread.
 *
 * After the background thread finishes, the result from the callback is passed to
 * another callback that is called in the main thread.
 *
 * Must be called from the main thread.
 *
 * If it is possible that the completion becomes invalid (e.g., the object that
 * started the operation is destroyed), you should use AsyncScope to automatically
 * invalidate the completion callbacks of the started tasks.
 *
 * @param task        Task callback. If an exception is thrown here, it will be
 *                    quietly caught, and the completion callback will be called with
 *                    a default-constructed result value. Note that if you return a
 *                    pointer to an object and intend to pass ownership to the
 *                    completion callback, the object will leak if the completion has
 *                    been invalidated. Therefore, you should always pass ownership via
 *                    std::shared_ptr or other reference-counted type.
 *
 * @param completion  Completion callback to be called in the main thread. Takes one
 *                    argument matching the type of the return value from @a task.
 *
 * @return Background thread object. The thread will delete itself after the completion
 * callback has been called. You can pass this to AsyncScope for keeping track of.
 */
template <typename Task, typename Completion>
AsyncTask *async(Task task, Completion completion)
{
    DENG2_ASSERT_IN_MAIN_THREAD();
    auto *t = new internal::AsyncTaskThread<Task, Completion>(std::move(task), std::move(completion));
    t->start();
    // Note: The thread will delete itself when finished.
    return t;
}

/*template <typename Task>
AsyncTask *async(Task const &task)
{
    auto *t = new internal::AsyncTaskThread<Task, void *>(task);
    t->start();
    // Note: The thread will delete itself when finished.
    return t;
}*/

/**
 * Utility for invalidating the completion callbacks of async tasks whose initiator
 * has gone out of scope.
 */
class DENG2_PUBLIC AsyncScope
{
public:
    AsyncScope() = default;
    ~AsyncScope();

    AsyncScope &operator += (AsyncTask *task);
    bool isAsyncFinished() const;
    void waitForFinished(TimeSpan timeout = 0.0);

private:
    LockableT<QSet<AsyncTask *>> _tasks;
};

} // namespace de

#endif // LIBDENG2_ASYNCTASK_H
