/**
 * @file oringbuffer.hpp
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
#ifndef _ORINGBUFFER_HPP_
#define _ORINGBUFFER_HPP_

/*===========================================================================*\
 * system header files
\*===========================================================================*/
#if defined(DEBUG_RINGBUFFER)
#include <iostream>
#endif

/*===========================================================================*\
 * project header files
\*===========================================================================*/
#include "ringbuffer_base.hpp"

/*===========================================================================*\
 * preprocessor #define constants and macros
\*===========================================================================*/

/*===========================================================================*\
 * global type definitions
\*===========================================================================*/
namespace ymn
{

template<typename T>
class oringbuffer : public virtual ringbuffer_base<T>
{
public:
    explicit oringbuffer() :
        ringbuffer_base<T>::ringbuffer_base{}
    {
#if defined(DEBUG_RINGBUFFER)
        std::cout << __PRETTY_FUNCTION__ << std::endl;
        std::cout << ringbuffer_base<T>::to_string() << std::endl;
#endif
    }

    ~oringbuffer() override
    {
#if defined(DEBUG_RINGBUFFER)
        std::cout << __PRETTY_FUNCTION__ << std::endl;
#endif
    }

    long write(const T& data)
    {
        return write(&data, 1, ringbuffer_base<T>::template copy<T>);
    }

    long write(T&& data)
    {
        return write(&data, 1, ringbuffer_base<T>::template move<T>);
    }

    template<std::size_t N>
    long write(const T (&data)[N])
    {
        return write(data, N, ringbuffer_base<T>::template copy<T>);
    }

    template<std::size_t N>
    long write(T (&&data)[N])
    {
        return write(data, N, ringbuffer_base<T>::template move<T>);
    }

    long write(std::function<bool(T*)> producer, std::size_t count)
    {
        return write(ringbuffer_functor<T>(producer), count, xfer_producer<T>);
    }

private:
    template<typename U>
    static bool xfer_producer(U* dst, ringbuffer_functor<U> src, std::size_t count)
    {
        bool status = true;

        while (count-- > 0)
            if (false == (status = src(dst++)))
                break;

        return status;
    }

    template<typename DST, typename SRC>
    long write(SRC data, std::size_t count, bool (*xfer)(DST, SRC, std::size_t))
    {
        std::size_t produced;
        std::size_t consumed;
        std::size_t free_elements;
        std::size_t write_idx;
        std::size_t split;
        std::size_t remaining;
        ringbuffer_status rbs;

        if (0 == count)
            return 0;

        if (ringbuffer_base<T>::m_flags.test(RINGBUFFER_NONBLOCKING_WRITE_SHIFT)) {
            rbs = ringbuffer_base<T>::get_counters(&produced, &consumed, nullptr);
            if (rbs != ringbuffer_status::OK)
                return static_cast<long>(rbs);

            free_elements = ringbuffer_base<T>::m_capacity - (produced - consumed);
            if (0 == free_elements) {
                ringbuffer_base<T>::m_counters.m_dropped++;
                return static_cast<long>(ringbuffer_status::WOULD_BLOCK);
            }
        } else {
            for (;;) {
                rbs = ringbuffer_base<T>::get_counters(&produced, &consumed, nullptr);
                if (rbs != ringbuffer_status::OK)
                    return static_cast<long>(rbs);

                free_elements = ringbuffer_base<T>::m_capacity - (produced - consumed);
                if (free_elements > 0)
                    break; /* leave the loop if we have room for new data */

                ringbuffer_base<T>::m_writing_semaphore.wait(); /* let's wait until consumer will read some data */
                if (ringbuffer_base<T>::m_is_writing_cancelled) {
                    ringbuffer_base<T>::m_is_writing_cancelled = false;
                    return static_cast<long>(ringbuffer_status::OPERATION_CANCELLED);
                }
            }
        }

        if (count > free_elements)
            count = free_elements;

        write_idx = produced % ringbuffer_base<T>::m_capacity;

        split = ((write_idx + count) > ringbuffer_base<T>::m_capacity) ? (ringbuffer_base<T>::m_capacity - write_idx) : 0;
        remaining = count;

        if (split > 0) {
            if (false == xfer(ringbuffer_base<T>::m_buffer + write_idx, data, split))
                return static_cast<long>(ringbuffer_status::INTERNAL_ERROR);

            data += split;
            remaining -= split;
            write_idx = 0;
        }

        if (false == xfer(ringbuffer_base<T>::m_buffer + write_idx, data, remaining))
            return static_cast<long>(ringbuffer_status::INTERNAL_ERROR);

        ringbuffer_base<T>::m_counters.m_produced.store(produced + count, std::memory_order_relaxed);

        if (!ringbuffer_base<T>::m_flags.test(RINGBUFFER_NONBLOCKING_READ_SHIFT))
            ringbuffer_base<T>::m_reading_semaphore.post(); /* wake up one thread waiting for new data (if any) */

        return count;
    }
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

#endif /* _ORINGBUFFER_HPP_ */
