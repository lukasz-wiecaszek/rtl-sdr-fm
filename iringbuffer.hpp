/**
 * @file iringbuffer.hpp
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
#ifndef _IRINGBUFFER_HPP_
#define _IRINGBUFFER_HPP_

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
class iringbuffer : public virtual ringbuffer_base<T>
{
public:
    explicit iringbuffer() :
        ringbuffer_base<T>::ringbuffer_base{}
    {
#if defined(DEBUG_RINGBUFFER)
        std::cout << __PRETTY_FUNCTION__ << std::endl;
        std::cout << ringbuffer_base<T>::to_string() << std::endl;
#endif
    }

    ~iringbuffer() override
    {
#if defined(DEBUG_RINGBUFFER)
        std::cout << __PRETTY_FUNCTION__ << std::endl;
#endif
    }

    long read(T& data)
    {
        return read(&data, 1, ringbuffer_base<T>::template copy<T>);
    }

    long read(T&& data)
    {
        return read(&data, 1, ringbuffer_base<T>::template move<T>);
    }

    template<std::size_t N>
    long read(T (&data)[N])
    {
        return read(data, N, ringbuffer_base<T>::template copy<T>);
    }

    template<std::size_t N>
    long read(T (&&data)[N])
    {
        return read(data, N, ringbuffer_base<T>::template move<T>);
    }

    long read(std::function<bool(T*)> consumer, std::size_t count)
    {
        return read(ringbuffer_functor<T>(consumer), count, xfer_consumer<T>);
    }

private:
    template<typename U>
    static bool xfer_consumer(ringbuffer_functor<U> dst, U* src, std::size_t count)
    {
        bool status = true;

        while (count-- > 0)
            if (false == (status = dst(src++)))
                break;

        return status;
    }

    template<typename DST, typename SRC>
    long read(DST data, std::size_t count, bool (*xfer)(DST, SRC, std::size_t))
    {
        std::size_t produced;
        std::size_t consumed;
        std::size_t available_elements;
        std::size_t read_idx;
        std::size_t split;
        std::size_t remaining;
        ringbuffer_status rbs;

        if (0 == count)
            return 0;

        if (ringbuffer_base<T>::m_flags.test(RINGBUFFER_NONBLOCKING_READ_SHIFT)) {
            rbs = ringbuffer_base<T>::get_counters(&produced, &consumed, nullptr);
            if (rbs != ringbuffer_status::OK)
                return static_cast<long>(rbs);

            available_elements = produced - consumed;
            if (0 == available_elements) {
               return static_cast<long>(ringbuffer_status::WOULD_BLOCK);
            }
        } else {
            for (;;) {
                rbs = ringbuffer_base<T>::get_counters(&produced, &consumed, nullptr);
                if (rbs != ringbuffer_status::OK)
                    return static_cast<long>(rbs);

                available_elements = produced - consumed;
                if (available_elements > 0)
                    break; /* leave the loop if we have elements to be read */

                ringbuffer_base<T>::m_reading_semaphore.wait(); /* let's wait until producer will write some data */
                if (ringbuffer_base<T>::m_is_reading_cancelled) {
                    ringbuffer_base<T>::m_is_reading_cancelled = false;
                    return static_cast<long>(ringbuffer_status::OPERATION_CANCELLED);
                }
            }
        }

        if (count > available_elements)
            count = available_elements;

        read_idx = consumed % ringbuffer_base<T>::m_capacity;

        split = ((read_idx + count) > ringbuffer_base<T>::m_capacity) ? (ringbuffer_base<T>::m_capacity - read_idx) : 0;
        remaining = count;

        if (split > 0) {
            if (false == xfer(data, ringbuffer_base<T>::m_buffer + read_idx, split))
                return static_cast<long>(ringbuffer_status::INTERNAL_ERROR);

            data += split;
            remaining -= split;
            read_idx = 0;
        }

        if (false == xfer(data, ringbuffer_base<T>::m_buffer + read_idx, remaining))
            return static_cast<long>(ringbuffer_status::INTERNAL_ERROR);

        ringbuffer_base<T>::m_counters.m_consumed.store(consumed + count, std::memory_order_relaxed);

        if (!ringbuffer_base<T>::m_flags.test(RINGBUFFER_NONBLOCKING_WRITE_SHIFT))
            ringbuffer_base<T>::m_writing_semaphore.post(); /* wake up one thread waiting for some space in the buffer (if any) */

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

#endif /* _IRINGBUFFER_HPP_ */
