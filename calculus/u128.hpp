/**
 * @author nawww83@gmail.com
 * @brief Класс для арифметики 128-битных беззнаковых целых чисел.
 */

#pragma once

#include <cstdint>   // uint64_t
#include <cassert>   // assert
#include <iostream>
#include <string>    // std::string
#include <utility>   // std::exchange
#include <algorithm> // std::min, std::max
#include <tuple>     // std::pair, std::tie
#include "ulow.hpp"  // low64::ULOW
#include "defines.h"

namespace bignum::generic
{
/**
     * @brief Вычисляет 2^W / x: частное Q и остаток R. Здесь W - битовая ширина числа U.
     */
template <class U>
inline std::pair<U, U> reciprocal_and_extend(U x)
{
    assert(x != 0);
    const auto x_old = x;
    const auto i = x.countl_zero();
    x <<= i;
    auto [Q, R] = (-x) / x_old;
    Q += (U{1ull} << i);
    return {Q, R};
}

/**
     * @brief r = (r + delta) mod m
     * Возвращает 1, если остаток при сложении был больше или равен модулю m; иначе возвращает ноль.
     * @param r_rec = 2^W mod m.
     */
template <class U>
inline U smart_remainder_adder(U &r, const U &delta, const U &m, const U &r_rec)
{
    assert(m != 0);
    const auto &delta_m = (delta / m).second;
    const U &summ = r + delta_m;
    const bool overflow = summ < std::min(r, delta_m);
    r = summ + (overflow ? r_rec : 0ull);
    std::tie(std::ignore, r) = r / m;
    return overflow ? 1ull : (summ >= m ? 1ull : 0ull);
}
}

namespace bignum::u128
{
/**
     *
     */
using u64 = uint64_t;

/**
     * @brief Тип половинки числа.
     */
using ULOW = low64::ULOW;

/**
     * Класс для арифметики 128-битных беззнаковых целых чисел, основанный на половинчатом представлении числа.
     */
class U128
{
public:
    /**
         * @brief Конструктор по умолчанию.
         */
    explicit constexpr U128() = default;

    /**
         * @brief Конструктор с параметром.
         */
    constexpr U128(u64 low) : mLow{low}
    {
        ;
    }

    /**
         * @brief Конструктор с параметром.
         */
    constexpr U128(ULOW low) : mLow{low}
    {
        ;
    }

    /**
         * @brief Конструктор с параметрами.
         */
    constexpr U128(u64 low, u64 high) : mLow{low}, mHigh{high}
    {
        ;
    }

    /**
         * @brief Конструктор с параметрами.
         */
    constexpr U128(ULOW low, ULOW high) : mLow{low}, mHigh{high}
    {
        ;
    }

    constexpr U128(const U128 &other) = default;

    constexpr U128(U128 &&other) = default;

    constexpr U128 &operator=(const U128 &other) = default;

    /**
         * @brief Оператор равно.
         */
    bool operator==(const U128 &other) const
    {
        return mLow == other.mLow && mHigh == other.mHigh;
    }

    /**
         * @brief Оператор сравнения.
         */
    std::partial_ordering operator<=>(const U128 &other) const
    {
        auto high_cmp = mHigh <=> other.mHigh;
        return high_cmp != 0 ? high_cmp : mLow <=> other.mLow;
    }

    /**
         * @brief Оператор сдвига влево. При больших сдвигах дает ноль.
         */
    U128 operator<<(uint32_t shift) const
    {
        if (shift >= 128u)
            return 0;
        U128 result = *this;
        int ishift = shift;
        ULOW L{0};
        const bool change_L = ishift < 64 && ishift > 0;
        const bool change_ishift = ishift >= 64;
        L = change_L ? result.mLow >> (64 - ishift) : L;
        result.mHigh = change_ishift ? std::exchange(result.mLow, 0) : result.mHigh;
        ishift -= change_ishift ? 64 : 0;
        result.mLow <<= ishift;
        result.mHigh <<= ishift;
        result.mHigh |= L;
        return result;
    }

    /**
         * @brief Оператор сдвига влево.
         */
    U128 &operator<<=(uint32_t shift)
    {
        *this = *this << shift;
        return *this;
    }

    /**
         * @brief Оператор сдвига вправо. При больших сдвигах дает ноль.
         */
    U128 operator>>(uint32_t shift) const
    {
        if (shift >= 128u)
            return 0;
        U128 result = *this;
        int ishift = shift;
        if (ishift < 64)
        {
            ULOW mask{ULOW::get_max_value()};
            mask <<= ishift;
            mask = ~mask;
            const auto &H = result.mHigh & mask;
            result.mLow >>= ishift;
            result.mHigh >>= ishift;
            result.mLow |= ishift == 0 ? H : H << (64 - ishift);
        }
        else
        {
            result.mLow = std::exchange(result.mHigh, 0);
            result.mLow >>= (ishift - 64);
        }
        return result;
    }

    /**
         * @brief Оператор сдвига вправо.
         */
    U128 &operator>>=(uint32_t shift)
    {
        *this = *this >> shift;
        return *this;
    }

    /**
         * @brief Оператор побитового И.
         */
    U128 operator&(const U128 &mask) const
    {
        U128 result = *this;
        result.mLow &= mask.mLow;
        result.mHigh &= mask.mHigh;
        return result;
    }

    /**
         * @brief Оператор побитового И.
         */
    U128 &operator&=(const U128 &mask)
    {
        *this = *this & mask;
        return *this;
    }

    /**
         * @brief Оператор побитового ИЛИ.
         */
    U128 operator|(const U128 &mask) const
    {
        U128 result = *this;
        result.mLow |= mask.mLow;
        result.mHigh |= mask.mHigh;
        return result;
    }

    /**
         * @brief Оператор побитового ИЛИ.
         */
    U128 &operator|=(const U128 &mask)
    {
        *this = *this | mask;
        return *this;
    }

    /**
         * @brief Оператор исключающего ИЛИ.
         */
    U128 operator^(const U128 &mask) const
    {
        U128 result = *this;
        result.mLow ^= mask.mLow;
        result.mHigh ^= mask.mHigh;
        return result;
    }

    /**
         * @brief Оператор исключающего ИЛИ.
         */
    U128 &operator^=(const U128 &mask)
    {
        *this = *this ^ mask;
        return *this;
    }

    /**
         * @brief Оператор инверсии битов.
         */
    U128 operator~() const
    {
        U128 result = *this;
        result.mLow = ~result.mLow;
        result.mHigh = ~result.mHigh;
        return result;
    }

    /**
         * @brief Оператор суммирования.
         */
    U128 operator+(const U128 &Y) const
    {
        const U128 &X = *this;
        U128 result{X.mLow + Y.mLow, X.mHigh + Y.mHigh};
        const auto &carry = ULOW{result.mLow < std::min(X.mLow, Y.mLow) ? 1ull : 0ull};
        result.mHigh += carry;
        return result;
    }

    /**
         * @brief Оператор суммирования.
         */
    U128 &operator+=(const U128 &Y)
    {
        *this = *this + Y;
        return *this;
    }

    /**
         * @brief Оператор вычитания.
         */
    U128 operator-(const U128 &Y) const
    {
        const U128 &X = *this;
        if (X >= Y) return subtract_if_lhs_more(X, Y);
        return subtract_if_lhs_more(U128::get_max_value(), Y) + 1 + X;
    }

    /**
         * @brief Оператор вычитания.
         */
    U128 &operator-=(const U128 &Y)
    {
        *this = *this - Y;
        return *this;
    }

    /**
         * @brief Оператор минус.
         */
    U128 operator-() const
    {
        return U128{0} - *this;
    }

    /**
         * @brief Инкремент числа.
         * @return Число + 1.
         */
    U128 &inc()
    {
        *this = *this + U128{1};
        return *this;
    }

    /**
         * @brief Декремент числа.
         * @return Число - 1.
         */
    U128 &dec()
    {
        *this = *this - U128{1};
        return *this;
    }

    /**
         * @brief Оператор умножения.
         */
    U128 operator*(const U128 &Y) const
    {
        const U128 &X = *this;
        // x*y = (a + w*b)(c + w*d) = ac + w*(ad + bc) + w*w*bd = (ac + w*(ad + bc)) mod 2^128;
        const U128 &ac = mult_ext(X.low(), Y.low());
        const U128 &ad = mult_ext(X.low(), Y.high());
        const U128 &bc = (X != Y) ? mult_ext(X.high(), Y.low()) : ad;
        U128 result{ad + bc};
        result <<= 64;
        result += ac;
        return result;
    }

    /**
         * @brief Оператор умножения.
         */
    U128 &operator*=(const U128 &Y)
    {
        *this = *this * Y;
        return *this;
    }

    /**
         * @brief Половинчатый оператор умножения.
         */
    U128 operator*(const ULOW &Y) const
    {
        const U128 &X = *this;
        // x*y = (a + w*b)(c + w*0) = ac + w*(0 + bc) = (ac + w*bc) mod 2^128;
        return (U128{mult_ext(X.high(), Y)} << 64) + mult_ext(X.low(), Y);
    }

    /**
         * @brief Половинчатый оператор умножения.
         */
    U128 &operator*=(const ULOW &Y)
    {
        *this = *this * Y;
        return *this;
    }

    /**
         * @brief Оператор умножения. Позволяет перемножать "узкие" числа, расположенные слева от "широкого" числа.
         */
    template <typename T>
    T operator*(const T &rhs) const
    {
        T result = rhs * *this;
        return result;
    }

    /**
         * @brief
         */
    template <typename T>
    T &operator*=(const T &) = delete;

    /**
         * @brief Оператор деления.
         * @return Частное от деления и остаток.
         */
    std::pair<U128, U128> operator/(const U128 &other) const
    {
        assert(other != 0);
        U128 X{*this};
        const auto &Y = other;
        U128 Q{0};
        auto div_helper = [&X, &Y, &Q]() -> void
        {
            if (X < Y)
                return;
            U128 Q_sc{1};
            auto Y_sc{Y};
            const int n_bits = X.bit_length() - Y.bit_length() - 1;
            if (n_bits > 0)
            {
                Y_sc <<= n_bits;
                Q_sc <<= n_bits;
            }
            if (Y_sc <= (X - Y_sc)) // use the subtraction due to possible overflow.
            {
                Y_sc <<= 1;
                Q_sc <<= 1;
            }
            Q += Q_sc;
            X -= Y_sc;
            return;
        };
        for (; X >= Y;)
        {
            div_helper();
        }
        return {Q, X};
    }

    /**
         * @brief
         */
    std::pair<U128, U128> operator/=(const U128 &Y)
    {
        U128 remainder;
        std::tie(*this, remainder) = *this / Y;
        return std::make_pair(*this, remainder);
    }

    /**
         * @brief Нижняя половина числа.
         */
    ULOW low() const
    {
        return mLow;
    }

    /**
         * @brief Верхняя половина числа.
         */
    ULOW high() const
    {
        return mHigh;
    }

    /**
         * @brief Количество битов, требуемое для представления числа.
         */
    int bit_length() const
    {
        return 128 - countl_zero();
    }

    /**
         * @brief Количество непрерывно идущих нулей битового представления числа, начиная с самого старшего бита.
         */
    int countl_zero() const
    {
        return mHigh == 0 ? 64 + mLow.countl_zero() : mHigh.countl_zero();
    }

    /**
         * @brief Получить максимальное значение 128-битного числа.
         */
    static constexpr U128 get_max_value()
    {
        return U128{ULOW::get_max_value(), ULOW::get_max_value()};
    }

    /**
         * @brief Умножение двух 64-битных чисел с расширением до 128-битного числа.
         * @details Авторский алгоритм умножения. Обобщается на любую разрядность.
         */
    static U128 mult_ext(ULOW x, ULOW y)
    {
        constexpr int QUORTER_WIDTH = 32; // Четверть ширины 128-битного числа.
        constexpr ULOW MASK = (ULOW{1}() << QUORTER_WIDTH) - 1;
        const ULOW x_low = x & MASK;
        const ULOW y_low = y & MASK;
        const ULOW x_high = x >> QUORTER_WIDTH;
        const ULOW y_high = y >> QUORTER_WIDTH;
        const ULOW t1 = x_low * y_low;
        const ULOW t = t1 >> QUORTER_WIDTH;
        const ULOW t21 = x_low * y_high;
        const ULOW q = t21 >> QUORTER_WIDTH;
        const ULOW p = t21 & MASK;
        const ULOW t22 = x_high * y_low;
        const ULOW s = t22 >> QUORTER_WIDTH;
        const ULOW r = t22 & MASK;
        const ULOW t3 = x_high * y_high;
        U128 result{t1};
        const ULOW div = (q + s) + ((p + r + t) >> QUORTER_WIDTH);
        const auto p1 = t21 << QUORTER_WIDTH;
        const auto p2 = t22 << QUORTER_WIDTH;
        const ULOW mod = p1 + p2;
        result.mLow += mod;
        result.mHigh += div;
        result.mHigh += t3;
        return result;
    }

    /**
         * @brief Возведение в квадрат 64-битного числа с расширением до 128-битного числа.
         */
    static U128 square_ext(ULOW x)
    {
        constexpr int QUORTER_WIDTH = 32; // Четверть ширины 128-битного числа.
        constexpr ULOW MASK = (ULOW{1}() << QUORTER_WIDTH) - 1;
        const ULOW x_low = x & MASK;
        const ULOW x_high = x >> QUORTER_WIDTH;
        const ULOW t1 = x_low * x_low;
        const ULOW t = t1 >> QUORTER_WIDTH;
        const ULOW t21 = x_low * x_high;
        const ULOW q = t21 >> QUORTER_WIDTH;
        const ULOW p = t21 & MASK;
        const ULOW t3 = x_high * x_high;
        U128 result{t1};
        const ULOW div = (q << 1) + (((p << 1) + t) >> QUORTER_WIDTH);
        const auto p1 = t21 << QUORTER_WIDTH;
        const ULOW mod = p1 << 1;
        result.mLow += mod;
        result.mHigh += div;
        result.mHigh += t3;
        return result;
    }

    /**
         * @brief Специальный метод деления на 10 для формирования строкового представления числа.
         */
    U128 div10() const
    {
        const U128 &X = *this;
        constexpr auto TEN = ULOW{10};
        const auto &reciprocal = (ULOW::get_max_value() / TEN).first;
        auto [Q, R] = X.high() / TEN;
        ULOW N = R * reciprocal + (X.low() / TEN).first;
        U128 result{N, Q};
        for (U128 E{X - result * TEN}; E >= TEN; E -= TEN * U128{N, Q})
        {
            std::tie(Q, R) = E.mHigh / TEN;
            N = R * reciprocal + (E.mLow / TEN).first;
            result += U128{N, Q};
        }
        return result;
    }

    /**
         * @brief Специальный метод нахождения остатка от деления на 10 для формирования строкового представления числа.
         */
    int mod10() const
    {
        const int multiplier_mod10 = ULOW::get_max_value().mod10() + 1;
        return (mLow.mod10() + multiplier_mod10 * mHigh.mod10()) % 10;
    }

    /**
         * @brief Возвращает строковое представление числа.
         */
    std::string value() const
    {
        std::string result;
        U128 X = *this;
        while (X != U128{0})
        {
            const int d = X.mod10();
            if (d < 0)
                return result;
            result.push_back(DIGITS[d]);
            X = X.div10();
        }
        std::reverse(result.begin(), result.end());
        return result.length() != 0 ? result : "0";
    }

private:
    /**
         * @brief Младшая половина числа.
         */
    ULOW mLow{0};

    /**
         * @brief Старшая половина числа.
         */
    ULOW mHigh{0};

    static U128 subtract_if_lhs_more(const U128& X, const U128& Y)
    {
        return U128{X.mLow - Y.mLow, X.mHigh - Y.mHigh - (X.mLow < Y.mLow)};
    }
};

}
