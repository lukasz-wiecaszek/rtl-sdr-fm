/**
 * @file compelx.hpp
 *
 * Class representing complex numbers.
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

#ifndef _COMPLEX_
#define _COMPLEX_

/*===========================================================================*\
 * system header files
\*===========================================================================*/
#include <string>
#include <sstream>

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

template<typename T>
class complex
{
public:
    typedef T value_type;

    constexpr complex(T re = T(), T im = T()) :
        m_re{re},
        m_im{im}
    {
    }

    constexpr complex(const complex& other) :
        m_re{other.m_re},
        m_im{other.m_im}
    {
    }

    template<typename U>
    constexpr complex(const complex<U>& other) :
        m_re{other.m_re},
        m_im{other.m_im}
    {
    }

    constexpr complex& operator = (T re)
    {
        m_re = re;
        m_im = T();
        return *this;
    }

    constexpr complex& operator = (const complex& other)
    {
        m_re = other.m_re;
        m_im = other.m_im;
        return *this;
    }

    template<typename U>
    constexpr complex& operator = (const complex<U>& other)
    {
        m_re = other.m_re;
        m_im = other.m_im;
        return *this;
    }

    constexpr void real(T value)
    {
        m_re = value;
    }

    constexpr T real() const
    {
        return m_re;
    }

    constexpr void imag(T value)
    {
        m_im = value;
    }

    constexpr T imag() const
    {
        return m_im;
    }

    constexpr complex<T> conj() const
    {
        return complex<T>(m_re, -m_im);
    }

    constexpr T norm() const
    {
        return m_re * m_re + m_im * m_im;
    }

    constexpr complex& operator += (T other)
    {
        m_re = m_re + other;
        return *this;
    }

    constexpr complex& operator += (const complex& other)
    {
        m_re = m_re + other.m_re;
        m_im = m_im + other.m_im;
        return *this;
    }

    constexpr complex& operator -= (T other)
    {
        m_re = m_re - other;
        return *this;
    }

    constexpr complex& operator -= (const complex& other)
    {
        m_re = m_re - other.m_re;
        m_im = m_im - other.m_im;
        return *this;
    }

    constexpr complex& operator *= (T other)
    {
        m_re = m_re * other;
        m_im = m_im * other;
        return *this;
    }

    constexpr complex& operator *= (const complex& other)
    {
        m_re = m_re * other.m_re - m_im * other.m_im;
        m_im = m_re * other.m_im + m_im * other.m_re;
        return *this;
    }

    constexpr complex& operator /= (T other)
    {
        m_re = m_re / other;
        m_im = m_im / other;
        return *this;
    }

    constexpr complex& operator /= (const complex& other)
    {
        T re = m_re * other.m_re + m_im * other.m_im;
        T im = m_re * other.m_im - m_im * other.m_re;
        T divisor = other.m_re * other.m_re + other.m_im * other.m_im;

        m_re = re / divisor;
        m_im = im / divisor;
        return *this;
    }

    std::string to_string() const
    {
        std::ostringstream stream;

        stream << "(";
        stream << std::to_string(m_re);
        stream << ", ";
        stream << std::to_string(m_im);
        stream << ")";

        return stream.str();
    }

    operator std::string() const
    {
        return to_string();
    }

private:
    T m_re;
    T m_im;
};

} /* end of namespace ymn */

/*===========================================================================*\
 * inline function/variable definitions
\*===========================================================================*/
namespace ymn
{

template<typename T>
constexpr complex<T> operator + (const complex<T>& c)
{
    return complex<T>(c.real(), c.imag());
}

template<typename T>
constexpr complex<T> operator - (const complex<T>& c)
{
    return complex<T>(-c.real(), -c.imag());
}

template<typename T>
constexpr complex<T> operator + (const complex<T>& lhs, const complex<T>& rhs)
{
    T re = lhs.real() + rhs.real();
    T im = lhs.imag() + rhs.imag();

    return complex<T>(re, im);
}

template<typename T>
constexpr complex<T> operator + (const complex<T>& lhs, T rhs)
{
    T re = lhs.real() + rhs;
    T im = lhs.imag();

    return complex<T>(re, im);
}

template<typename T>
constexpr complex<T> operator + (T lhs, const complex<T>& rhs)
{
    T re = lhs + rhs.real();
    T im = rhs.imag();

    return complex<T>(re, im);
}

template<typename T>
constexpr complex<T> operator - (const complex<T>& lhs, const complex<T>& rhs)
{
    T re = lhs.real() - rhs.real();
    T im = lhs.imag() - rhs.imag();

    return complex<T>(re, im);
}

template<typename T>
constexpr complex<T> operator - (const complex<T>& lhs, T rhs)
{
    T re = lhs.real() - rhs;
    T im = lhs.imag();

    return complex<T>(re, im);
}

template<typename T>
constexpr complex<T> operator - (T lhs, const complex<T>& rhs)
{
    T re = lhs - rhs.real();
    T im = rhs.imag();

    return complex<T>(re, im);
}

template<typename T>
constexpr complex<T> operator * (const complex<T>& lhs, const complex<T>& rhs)
{
    T re = lhs.real() * rhs.real() - lhs.imag() * rhs.imag();
    T im = lhs.real() * rhs.imag() + lhs.imag() * rhs.real();

    return complex<T>(re, im);
}

template<typename T>
constexpr complex<T> operator * (const complex<T>& lhs, T rhs)
{
    T re = lhs.real() * rhs;
    T im = lhs.imag() * rhs;

    return complex<T>(re, im);
}

template<typename T>
constexpr complex<T> operator * (T lhs, const complex<T>& rhs)
{
    T re = lhs * rhs.real();
    T im = lhs * rhs.imag();

    return complex<T>(re, im);
}

template<typename T>
constexpr complex<T> operator / (const complex<T>& lhs, const complex<T>& rhs)
{
    T re = lhs.real() * rhs.real() + lhs.imag() * rhs.imag();
    T im = lhs.real() * rhs.imag() - lhs.imag() * rhs.real();
    T divisor = rhs.real() * rhs.real() + rhs.imag() * rhs.imag();

    return complex<T>(re / divisor, im / divisor);
}

template<typename T>
constexpr complex<T> operator / (const complex<T>& lhs, T rhs)
{
    T re = lhs.real() / rhs;
    T im = lhs.imag() / rhs;

    return complex<T>(re, im);
}

template<typename T>
constexpr complex<T> operator / (T lhs, const complex<T>& rhs)
{
    T re = lhs * rhs.real();
    T im = lhs * rhs.imag();
    T divisor = rhs.real() * rhs.real() + rhs.imag() * rhs.imag();

    return complex<T>(re / divisor, im / divisor);
}

template<typename T>
constexpr bool operator == (const complex<T>& lhs, const complex<T>& rhs)
{
    return (lhs.real() == rhs.real()) && (lhs.imag() == rhs.imag());
}

template<typename T>
constexpr bool operator == (const complex<T>& lhs, T rhs)
{
    return (lhs.real() == rhs) && (lhs.imag() == T());
}

template<typename T>
constexpr bool operator == (T lhs, const complex<T>& rhs)
{
    return (lhs == rhs.real()) && (T() == rhs.imag());
}

template<typename T>
constexpr bool operator != (const complex<T>& lhs, const complex<T>& rhs)
{
    return !(lhs == rhs);
}

template<typename T>
constexpr bool operator != (const complex<T>& lhs, T rhs)
{
    return !(lhs == rhs);
}

template<typename T>
constexpr bool operator != (T lhs, const complex<T>& rhs)
{
    return !(lhs == rhs);
}

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

#endif /* _COMPLEX_ */
