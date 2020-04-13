/**
 * @file ringbuffer.hpp
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

#ifndef _RINGBUFFER_HPP_
#define _RINGBUFFER_HPP_

/*===========================================================================*\
 * system header files
\*===========================================================================*/
#if defined(DEBUG_RINGBUFFER)
#include <iostream>
#endif

/*===========================================================================*\
 * project header files
\*===========================================================================*/
#include "iringbuffer.hpp"
#include "oringbuffer.hpp"

/*===========================================================================*\
 * preprocessor #define constants and macros
\*===========================================================================*/

/*===========================================================================*\
 * global type definitions
\*===========================================================================*/
namespace ymn
{

template<typename T>
class ringbuffer : public iringbuffer<T>, public oringbuffer<T>
{
public:
    typedef T value_type;

    explicit ringbuffer(std::size_t capacity, std::bitset<RINGBUFFER_NONBLOCKING_FLAGS_MAX> flags) :
        ringbuffer_base<T>::ringbuffer_base{capacity, flags}
    {
#if defined(DEBUG_RINGBUFFER)
        std::cout << __PRETTY_FUNCTION__ << std::endl;
        std::cout << ringbuffer_base<T>::to_string() << std::endl;
#endif
    }

    ~ringbuffer() override
    {
#if defined(DEBUG_RINGBUFFER)
        std::cout << __PRETTY_FUNCTION__ << std::endl;
#endif
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

#endif /* _RINGBUFFER_HPP_ */
