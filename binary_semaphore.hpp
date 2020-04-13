/**
 * @file binary_semaphore.hpp
 *
 * Class representing/implementing a binary_semaphore design pattern.
 *
 * @author Lukasz Wiecaszek <lukasz.wiecaszek@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 */

#ifndef _BINARY_SEMAPHORE_HPP_
#define _BINARY_SEMAPHORE_HPP_

/*===========================================================================*\
 * system header files
\*===========================================================================*/
#include <type_traits>
#include <mutex>
#include <condition_variable>
#include <chrono>

/*===========================================================================*\
 * project header files
\*===========================================================================*/

/*===========================================================================*\
 * preprocessor #define constants and macros
\*===========================================================================*/

/*===========================================================================*\
 * global type definitions
\*===========================================================================*/
namespace ymn
{

class binary_semaphore
{
public:
    explicit binary_semaphore(bool ready = false) :
       m_mutex{},
       m_condvar{},
       m_ready{ready}
    {
    }

    ~binary_semaphore() = default;

    binary_semaphore(const binary_semaphore&) = delete;
    binary_semaphore(binary_semaphore&&) = delete;

    binary_semaphore& operator = (const binary_semaphore&) = delete;
    binary_semaphore& operator = (binary_semaphore&&) = delete;

    bool get_value() const
    {
        bool ready;

        do {
            std::lock_guard<decltype(m_mutex)> lock(m_mutex);
            ready = m_ready;
        } while (0);

        return ready;
    }

    /**
     * Unlocks the semaphore.
     *
     * If some of the threads were waiting for being notified by a call
     * to this function then any (but only one) of them will be chosen and woken up.
     *
     * @return none
     */
    void post()
    {
        do {
            std::lock_guard<decltype(m_mutex)> lock(m_mutex);
            m_ready = true;
        } while (0);

        /* the lock does not need to be held for notification */
        m_condvar.notify_one();
    }

    /**
     * Locks the semaphore.
     *
     * If the semaphore is unlocked, then locking proceeds,
     * and the function returns immediately.
     * If the semaphore is currently locked, then the call blocks
     * until the semaphore is unlocked (post() is issued).
     *
     * @return none
     */
    void wait()
    {
        std::unique_lock<decltype(m_mutex)> lock(m_mutex);

        while (!m_ready)
            m_condvar.wait(lock);

        m_ready = false;
    }

    /**
     * Locks the semaphore with timeout semantics.
     *
     * If the semaphore is unlocked, then locking proceeds,
     * and the function returns immediately.
     * If the semaphore is currently locked, then the call blocks
     * until the semaphore is unlocked (post() is issued)
     * or limit on the amount of time that the call should block expires.
     *
     * @param[in] milliseconds Represents the maximum time to spend waiting.
     *
     * @return false when the function returns because timeout has passed,
     *         true otherwise.
     */
    bool wait_timeout(unsigned int milliseconds)
    {
        const std::chrono::steady_clock::time_point t1(std::chrono::steady_clock::now());
        const std::chrono::milliseconds timeout(milliseconds);
        std::chrono::milliseconds elapsed(0);

        std::unique_lock<decltype(m_mutex)> lock(m_mutex);

        while (!m_ready) {
            if (elapsed >= timeout)
                return false;

            if (std::cv_status::timeout == m_condvar.wait_for(lock, timeout - elapsed))
                return false;

            const std::chrono::steady_clock::time_point t2(std::chrono::steady_clock::now());
            elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1);
        }

        m_ready = false;
        return true;
    }

private:
    mutable std::mutex m_mutex;
    std::condition_variable m_condvar;
    bool m_ready;
};

} /* end of namespace ymn */

/*===========================================================================*\
 * inline function/variable definitions
\*===========================================================================*/
namespace ymn
{

} /* end of namespace ymn */

/*===========================================================================*\
 * global object declarations
\*===========================================================================*/
namespace ymn
{

} /* end of namespace ymn */

/*===========================================================================*\
 * function forward declarations
\*===========================================================================*/
namespace ymn
{

} /* end of namespace ymn */

#endif /* _BINARY_SEMAPHORE_HPP_ */
