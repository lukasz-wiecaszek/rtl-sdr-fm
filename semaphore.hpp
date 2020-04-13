/**
 * @file semaphore.hpp
 *
 * Class representing/implementing a semaphore design pattern.
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

#ifndef _SEMAPHORE_HPP_
#define _SEMAPHORE_HPP_

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

class semaphore
{
public:
    explicit semaphore(std::size_t count = 0) :
       m_mutex{},
       m_condvar{},
       m_count{count}
    {
    }

    ~semaphore() = default;

    semaphore(const semaphore&) = delete;
    semaphore(semaphore&&) = delete;

    semaphore& operator = (const semaphore&) = delete;
    semaphore& operator = (semaphore&&) = delete;

    std::size_t get_value() const
    {
        std::size_t count;

        do {
            std::lock_guard<decltype(m_mutex)> lock(m_mutex);
            count = m_count;
        } while (0);

        return count;
    }

    /**
     * Increments the semaphore value.
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
            m_count++;
        } while (0);

        /* the lock does not need to be held for notification */
        m_condvar.notify_one();
    }

    /**
     * Decrements the semaphore value.
     *
     * If the semaphore's value is greater than zero, then the decrement proceeds,
     * and the function returns immediately.
     * If the semaphore currently has the value zero, then the call blocks
     * until the semaphore value rises above zero (post() is issued).
     *
     * @return none
     */
    void wait()
    {
        std::unique_lock<decltype(m_mutex)> lock(m_mutex);

        while (m_count == 0)
            m_condvar.wait(lock);

        m_count--;
    }

    /**
     * Decrements the semaphore value with timeout semantics.
     *
     * If the semaphore's value is greater than zero, then the decrement proceeds,
     * and the function returns immediately.
     * If the semaphore currently has the value zero, then the call blocks
     * until the semaphore value rises above zero (post() is issued)
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

        while (m_count == 0) {
            if (elapsed >= timeout)
                return false;

            if (std::cv_status::timeout == m_condvar.wait_for(lock, timeout - elapsed))
                return false;

            const std::chrono::steady_clock::time_point t2(std::chrono::steady_clock::now());
            elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1);
        }

        m_count--;
        return true;
    }

private:
    mutable std::mutex m_mutex;
    std::condition_variable m_condvar;
    std::size_t m_count;
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

#endif /* _SEMAPHORE_HPP_ */
