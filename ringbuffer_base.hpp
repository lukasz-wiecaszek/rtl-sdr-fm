/**
 * @file ringbuffer_base.hpp
 *
 * Definition of generic ringbuffer design pattern.
 * Its non-blocking path is completely lockless.
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

#ifndef _RINGBUFFER_BASE_HPP_
#define _RINGBUFFER_BASE_HPP_

/*===========================================================================*\
 * system header files
\*===========================================================================*/
#include <atomic>
#include <string>
#include <sstream>
#include <functional>
#include <bitset>

#include <cassert>
#include <cstring>
#include <climits>

#if defined(DEBUG_RINGBUFFER)
#include <iostream>
#endif

#if !defined(CACHELINE_SIZE)
#define CACHELINE_SIZE 64
#endif

/*===========================================================================*\
 * project header files
\*===========================================================================*/
#include "utilities.hpp"
#include "binary_semaphore.hpp"

/*===========================================================================*\
 * preprocessor #define constants and macros
\*===========================================================================*/
#define RINGBUFFER_NONBLOCKING_WRITE_SHIFT  0
#define RINGBUFFER_NONBLOCKING_READ_SHIFT   1
#define RINGBUFFER_NONBLOCKING_FLAGS_MAX    2

#define RINGBUFFER_RD_BLOCKING_WR_BLOCKING \
    std::bitset<RINGBUFFER_NONBLOCKING_FLAGS_MAX>{ \
        0U << RINGBUFFER_NONBLOCKING_READ_SHIFT | 0U << RINGBUFFER_NONBLOCKING_WRITE_SHIFT \
    }

#define RINGBUFFER_RD_BLOCKING_WR_NONBLOCKING \
    std::bitset<RINGBUFFER_NONBLOCKING_FLAGS_MAX>{ \
        0U << RINGBUFFER_NONBLOCKING_READ_SHIFT | 1U << RINGBUFFER_NONBLOCKING_WRITE_SHIFT \
    }

#define RINGBUFFER_RD_NONBLOCKING_WR_BLOCKING \
    std::bitset<RINGBUFFER_NONBLOCKING_FLAGS_MAX>{ \
        1U << RINGBUFFER_NONBLOCKING_READ_SHIFT | 0U << RINGBUFFER_NONBLOCKING_WRITE_SHIFT \
    }

#define RINGBUFFER_RD_NONBLOCKING_WR_NONBLOCKING \
    std::bitset<RINGBUFFER_NONBLOCKING_FLAGS_MAX>{ \
        1U << RINGBUFFER_NONBLOCKING_READ_SHIFT | 1U << RINGBUFFER_NONBLOCKING_WRITE_SHIFT \
    }

/*===========================================================================*\
 * global type definitions
\*===========================================================================*/
namespace ymn
{

enum class ringbuffer_status
{
    OK = 0,
    INTERNAL_ERROR = -1,
    WOULD_BLOCK = -2,
    OPERATION_CANCELLED = -3,
};

enum class ringbuffer_role
{
    NONE,
    PRODUCER,
    CONSUMER,
};

enum class ringbuffer_xfer_semantic
{
    COPY,
    MOVE,
};

template<typename T>
struct ringbuffer_functor
{
    ringbuffer_functor(std::function<bool(T*)> f) :
        m_function{f}
    {
    }

    bool operator()(T* arg)
    {
        return m_function(arg);
    }

    /* This operator does nothing but is needed to fullfil contract
    needed in template write and read functions. */
    void operator += (std::size_t count)
    {
        UNUSED(count);
    }

    std::function<bool(T*)> m_function;
};

template<typename T>
class ringbuffer_base
{
protected:
    explicit ringbuffer_base(std::size_t capacity = 0,
                             std::bitset<RINGBUFFER_NONBLOCKING_FLAGS_MAX> flags = RINGBUFFER_RD_NONBLOCKING_WR_NONBLOCKING) :
        m_capacity{capacity},
        m_flags{flags},
        m_counters{},
        m_buffer{nullptr},
        m_writing_semaphore{true},
        m_reading_semaphore{false},
        m_is_writing_cancelled{false},
        m_is_reading_cancelled{false}
    {
        assert(capacity > 0);
        assert(capacity < LONG_MAX);

        m_buffer = new T[capacity];

#if defined(DEBUG_RINGBUFFER)
        std::cout << __PRETTY_FUNCTION__ << std::endl;
        std::cout << to_string() << std::endl;
#endif
    }

    virtual ~ringbuffer_base()
    {
#if defined(DEBUG_RINGBUFFER)
        std::cout << __PRETTY_FUNCTION__ << std::endl;
#endif

        delete [] m_buffer;
    }

    ringbuffer_base(const ringbuffer_base&) = delete;
    ringbuffer_base(ringbuffer_base&&) = delete;
    ringbuffer_base& operator = (const ringbuffer_base&) = delete;
    ringbuffer_base& operator = (ringbuffer_base&&) = delete;

public:
    std::size_t capacity() const
    {
        return m_capacity;
    }

    std::bitset<RINGBUFFER_NONBLOCKING_FLAGS_MAX> flags() const
    {
        return m_flags;
    }

    ringbuffer_status get_counters(std::size_t* produced, std::size_t* consumed, std::size_t* dropped) const
    {
        std::size_t l_produced = m_counters.m_produced.load(std::memory_order_relaxed);
        std::size_t l_consumed = m_counters.m_consumed.load(std::memory_order_relaxed);

        if (l_produced < l_consumed)
            return ringbuffer_status::INTERNAL_ERROR;

        if ((l_produced - l_consumed) > m_capacity) /* LONG_MAX is the max capacity */
            return ringbuffer_status::INTERNAL_ERROR;

        if (produced) *produced = l_produced;
        if (consumed) *consumed = l_consumed;

        if (dropped)
            *dropped = m_counters.m_dropped.load(std::memory_order_relaxed);

        return ringbuffer_status::OK;
    }

    void reset(ringbuffer_role role)
    {
        if (role == ringbuffer_role::PRODUCER) {
            std::size_t consumed = m_counters.m_consumed.load(std::memory_order_relaxed);
            m_counters.m_produced.store(consumed, std::memory_order_relaxed);
            m_counters.m_dropped.store(0U, std::memory_order_relaxed);
        }
        else
        if (role == ringbuffer_role::CONSUMER) {
            std::size_t produced = m_counters.m_produced.load(std::memory_order_relaxed);
            m_counters.m_consumed.store(produced, std::memory_order_relaxed);
        }
        else {
            m_counters.reset();
        }
    }

    void cancel(ringbuffer_role role)
    {
        if (role == ringbuffer_role::PRODUCER) {
            if (!m_flags.test(RINGBUFFER_NONBLOCKING_WRITE_SHIFT)) {
                m_is_writing_cancelled = true;
                m_writing_semaphore.post();
            }
        }
        else
        if (role == ringbuffer_role::CONSUMER) {
            if (!m_flags.test(RINGBUFFER_NONBLOCKING_READ_SHIFT)) {
                m_is_reading_cancelled = true;
                m_reading_semaphore.post();
            }
        }
        else {
            /* do noting */
        }
    }

    std::string to_string() const
    {
        std::ostringstream stream;

        stream << "ringbuffer@";
        stream << std::hex << this;
        stream << " [capacity: ";
        stream << std::dec << m_capacity;
        stream << ", ";
        stream << "write policy: ";
        stream << (m_flags.test(RINGBUFFER_NONBLOCKING_WRITE_SHIFT) ? "non_blocking" : "blocking");
        stream << ", ";
        stream << "read policy: ";
        stream << (m_flags.test(RINGBUFFER_NONBLOCKING_READ_SHIFT) ? "non_blocking" : "blocking");
        stream << " ";
        stream << static_cast<std::string>(m_counters);
        stream << "]";

        return stream.str();
    }

    operator std::string () const
    {
        return to_string();
    }

protected:
    template<typename U>
    static bool copy(U* dst, const U* src, std::size_t count)
    {
        while (count-- > 0)
            *dst++ = *src++;

        return true;
    }

    template<typename U>
    static bool move(U* dst, U* src, std::size_t count)
    {
        while (count-- > 0)
            *dst++ = std::move(*src++);

        return true;
    }

    struct alignas(CACHELINE_SIZE) counters
    {
        explicit counters()
        {
            reset();
        }

        void reset()
        {
            m_produced.store(0U, std::memory_order_relaxed);
            m_consumed.store(0U, std::memory_order_relaxed);
            m_dropped.store(0U, std::memory_order_relaxed);
        }

        std::string to_string() const
        {
            std::ostringstream stream;

            stream << "[";
            stream << "produced: ";
            stream << std::dec << m_produced.load(std::memory_order_relaxed);
            stream << ", ";
            stream << "consumed: ";
            stream << std::dec << m_consumed.load(std::memory_order_relaxed);
            stream << ", ";
            stream << "dropped: ";
            stream << std::dec << m_dropped.load(std::memory_order_relaxed);
            stream << "]";

            return stream.str();
        }

        operator std::string () const
        {
            return to_string();
        }

        std::atomic<std::size_t> m_produced;
        std::atomic<std::size_t> m_consumed;
        std::atomic<std::size_t> m_dropped;
    };

    std::size_t m_capacity;
    std::bitset<RINGBUFFER_NONBLOCKING_FLAGS_MAX> m_flags;
    counters m_counters;
    T* m_buffer;
    binary_semaphore m_writing_semaphore;
    binary_semaphore m_reading_semaphore;
    std::atomic<bool> m_is_writing_cancelled;
    std::atomic<bool> m_is_reading_cancelled;
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

#endif /* _RINGBUFFER_BASE_HPP_ */
