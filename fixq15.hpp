/**
 * @file fixq15.hpp
 *
 * Fixed point with 15 fractional bits.
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

#ifndef _FIXQ15_HPP_
#define _FIXQ15_HPP_

/*===========================================================================*\
 * system header files
\*===========================================================================*/

/*===========================================================================*\
 * project header files
\*===========================================================================*/

/*===========================================================================*\
 * preprocessor #define constants and macros
\*===========================================================================*/
#define Q15  (1 << 15)

/*===========================================================================*\
 * global type definitions
\*===========================================================================*/
namespace ymn
{

class fixq15 /* fixed point with 15 fractional bits */
{
    using T = int64_t;
public:
    constexpr fixq15() : m_value{} {}
    constexpr fixq15(T v) : m_value{v} {}
    constexpr operator T () const {return m_value;}
    constexpr T value() const {return m_value;}

private:
    T m_value;
};

constexpr fixq15 operator + (fixq15 lhs, fixq15 rhs)
{
    return lhs.value() + rhs.value();
}

constexpr fixq15 operator - (fixq15 lhs, fixq15 rhs)
{
    return lhs.value() - rhs.value();
}

constexpr fixq15 operator * (fixq15 lhs, fixq15 rhs)
{
    return lhs.value() * rhs.value() / Q15;
}

constexpr fixq15 operator / (fixq15 lhs, fixq15 rhs)
{
    return lhs.value() * Q15 / rhs.value();
}

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

#endif /* _FIXQ15_HPP_ */
