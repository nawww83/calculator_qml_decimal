/**
 * @author nawww83@gmail.com
 * @brief Класс для арифметики N-битных беззнаковых целых чисел.
 */

#pragma once

#include <cstdint>   // uint64_t
#include <cassert>   // assert
#include <string>    // std::string
#include <utility>   // std::exchange
#include <algorithm> // std::min, std::max
#include <tuple>     // std::pair, std::tie
#include "defines.h" // DIGITS
#include "u128.hpp" // bignum::generic::

namespace bignum::ubig
{
using u64 = uint64_t;

/**
     * @brief Класс для арифметики N-битных беззнаковых целых чисел, основанный на половинчатом представлении числа.
     * @details WIDTH - ширина числа, в битах. Числа беззнаковые, без переполнения: работают в арифметике по модулю 2^N.
     */
template <class ULOW, unsigned WIDTH>
class UBig
{
public:
    /**
         * @brief Конструктор по умолчанию.
         */
    explicit constexpr UBig() = default;

    /**
         * @brief Конструктор с параметром.
         */
    constexpr UBig(u64 low) : mLow{low}
    {
        ;
    }

    /**
         * @brief Конструктор с параметром.
         */
    constexpr UBig(ULOW low) : mLow{low}
    {
        ;
    }

    /**
         * @brief Конструктор с параметрами.
         */
    constexpr UBig(u64 low, u64 high) : mLow{low}, mHigh{high}
    {
        ;
    }

    /**
         * @brief Конструктор с параметрами.
         */
    constexpr UBig(ULOW low, ULOW high) : mLow{low}, mHigh{high}
    {
        ;
    }

    constexpr UBig(const UBig &other) = default;

    constexpr UBig(UBig &&other) = default;

    constexpr UBig &operator=(const UBig &other) = default;

    /**
         * @brief Оператор равно.
         */
    bool operator==(const UBig &other) const
    {
        return mLow == other.mLow && mHigh == other.mHigh;
    }

    /**
         * @brief Оператор сравнения.
         */
    std::partial_ordering operator<=>(const UBig &other) const
    {
        auto high_cmp = mHigh <=> other.mHigh;
        return high_cmp != 0 ? high_cmp : mLow <=> other.mLow;
    }

    /**
         * @brief Оператор сдвига влево. При больших сдвигах дает ноль.
         */
    UBig operator<<(uint32_t shift) const
    {
        if (shift >= WIDTH)
        {
            return UBig{0};
        }
        UBig result = *this;
        int ishift = shift;
        ULOW L{0};
        if (ishift < WIDTH/2)
        {
            L = ishift == 0 ? L : result.mLow >> (WIDTH/2 - ishift);
        }
        else
        {
            result.mHigh = std::exchange(result.mLow, ULOW{0});
            ishift -= WIDTH/2;
        }
        result.mLow <<= ishift;
        result.mHigh <<= ishift;
        result.mHigh |= L;
        return result;
    }

    /**
         * @brief Оператор сдвига влево.
         */
    UBig &operator<<=(uint32_t shift)
    {
        *this = *this << shift;
        return *this;
    }

    /**
         * @brief Оператор сдвига вправо. При больших сдвигах дает ноль.
         */
    UBig operator>>(uint32_t shift) const
    {
        if (shift >= WIDTH)
        {
            return UBig{0};
        }
        UBig result = *this;
        int ishift = shift;
        if (ishift < WIDTH/2)
        {
            ULOW mask = ULOW::get_max_value();
            mask <<= ishift;
            mask = ~mask;
            const auto &H = result.mHigh & mask;
            result.mLow >>= ishift;
            result.mHigh >>= ishift;
            result.mLow |= ishift == 0 ? H : H << (WIDTH/2 - ishift);
        }
        else
        {
            result.mLow = std::exchange(result.mHigh, ULOW{0});
            result.mLow >>= (ishift - WIDTH/2);
        }
        return result;
    }

    /**
         * @brief Оператор сдвига вправо.
         */
    UBig &operator>>=(uint32_t shift)
    {
        *this = *this >> shift;
        return *this;
    }

    /**
         * @brief Оператор побитового И.
         */
    UBig operator&(const UBig &mask) const
    {
        UBig result = *this;
        result.mLow &= mask.mLow;
        result.mHigh &= mask.mHigh;
        return result;
    }

    /**
         * @brief Оператор побитового И.
         */
    UBig &operator&=(const UBig &mask)
    {
        *this = *this & mask;
        return *this;
    }

    /**
         * @brief Оператор побитового ИЛИ.
         */
    UBig operator|(const UBig &mask) const
    {
        UBig result = *this;
        result.mLow |= mask.mLow;
        result.mHigh |= mask.mHigh;
        return result;
    }

    /**
         * @brief Оператор побитового ИЛИ.
         */
    UBig &operator|=(const UBig &mask)
    {
        *this = *this | mask;
        return *this;
    }

    /**
         * @brief Оператор исключающего ИЛИ.
         */
    UBig operator^(const UBig &mask) const
    {
        UBig result = *this;
        result.mLow ^= mask.mLow;
        result.mHigh ^= mask.mHigh;
        return result;
    }

    /**
         * @brief Оператор исключающего ИЛИ.
         */
    UBig &operator^=(const UBig &mask)
    {
        *this = *this ^ mask;
        return *this;
    }

    /**
         * @brief Оператор инверсии битов.
         */
    UBig operator~() const
    {
        UBig result = *this;
        result.mLow = ~result.mLow;
        result.mHigh = ~result.mHigh;
        return result;
    }

    /**
         * @brief Оператор суммирования.
         */
    UBig operator+(const UBig &Y) const
    {
        const UBig &X = *this;
        UBig result{X.mLow + Y.mLow, X.mHigh + Y.mHigh};
        const auto &carry = ULOW{result.mLow < std::min(X.mLow, Y.mLow) ? 1ull : 0ull};
        result.mHigh += carry;
        return result;
    }

    /**
         * @brief Оператор суммирования.
         */
    UBig &operator+=(const UBig &Y)
    {
        *this = *this + Y;
        return *this;
    }

    /**
         * @brief Оператор вычитания.
         */
    UBig operator-(const UBig &Y) const
    {
        const UBig &X = *this;
        if (X >= Y)
        {
            const auto &high = (X.mHigh - Y.mHigh) - ULOW{X.mLow < Y.mLow ? 1ull : 0ull};
            return UBig{X.mLow - Y.mLow, high};
        }
        return (X + (UBig::get_max_value() - Y)).inc();
    }

    /**
         * @brief Оператор вычитания.
         */
    UBig &operator-=(const UBig &Y)
    {
        *this = *this - Y;
        return *this;
    }

    /**
         * @brief Оператор минус.
         */
    UBig operator-() const
    {
        return UBig{0} - *this;
    }

    /**
         * @brief Инкремент числа.
         * @return Число + 1.
         */
    UBig &inc()
    {
        *this = *this + UBig{1};
        return *this;
    }

    /**
         * @brief Декремент числа.
         * @return Число - 1.
         */
    UBig &dec()
    {
        *this = *this - UBig{1};
        return *this;
    }

    /**
         * @brief Оператор умножения.
         */
    UBig operator*(const UBig &Y) const
    {
        const UBig &X = *this;
        // x*y = (a + w*b)(c + w*d) = ac + w*(ad + bc) + w*w*bd = (ac + w*(ad + bc)) mod 2^128;
        const UBig ac = mult_ext(X.mLow, Y.mLow);
        const UBig ad = mult_ext(X.mLow, Y.mHigh);
        const UBig bc = mult_ext(X.mHigh, Y.mLow);
        UBig result = ad + bc;
        result <<= WIDTH/2;
        result += ac;
        return result;
    }

    /**
         * @brief Оператор умножения.
         */
    UBig &operator*=(const UBig &Y)
    {
        *this = *this * Y;
        return *this;
    }

    /**
         * @brief Половинчатый оператор умножения.
         */
    UBig operator*(const ULOW &Y) const
    {
        const UBig &X = *this;
        // x*y = (a + w*b)(c + w*0) = ac + w*(0 + bc) = (ac + w*bc) mod 2^128;
        UBig result{mult_ext(X.mHigh, Y)};
        result <<= WIDTH/2;
        result += mult_ext(X.mLow, Y);
        return result;
    }

    /**
         * @brief Половинчатый оператор умножения.
         */
    UBig &operator*=(const ULOW &Y)
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
         * @brief Оператор половинчатого деления.
         * @details Авторский метод итеративного деления "широкого" числа на "узкое".
         * Количество итераций: ~[3, 42], наиболее вероятное количество - 15, среднее - около 21.
         * @return Частное от деления Q и остаток R.
         */
    std::pair<UBig, ULOW> operator/(const ULOW &Y) const
    {
        assert(Y != 0);
        UBig X = *this;
        if (Y == ULOW{1})
        {
            return {X, 0};
        }
        UBig Q{0};
        ULOW R = 0;
        auto rcp = bignum::generic::reciprocal_and_extend(Y);
        const auto &rcp_compl = Y - rcp.second;
        const bool make_inverse = rcp_compl < rcp.second; // Для ускорения сходимости.
        rcp.first += make_inverse ? ULOW{1} : ULOW{0};
        const auto X_old = X;
        for (;;)
        {
            const bool x_has_high = X.high() != 0;
            Q += x_has_high ? UBig::mult_ext(X.high(), rcp.first) : 0ull;
            Q += UBig{(X.low() / Y).first};
            const auto& carry = bignum::generic::smart_remainder_adder(R, X.low(), Y, rcp.second);
            Q += carry;
            X = X.high() != 0ull ? UBig::mult_ext(X.high(), make_inverse ? rcp_compl : rcp.second) : 0ull;
            if (X == UBig{0})
            {
                if (Q > X_old) // Коррекция знака.
                {
                    Q = -Q;
                    R = Y - R; // mod Y
                    if (R == Y)
                    {
                        Q.inc();
                        R = 0;
                    }
                }
                break;
            }
            if (make_inverse)
            {
                Q = -Q;
                R = Y - R; // mod Y
            }
        }
        return {Q, R};
    }

    /**
         * @brief
         */
    std::pair<UBig, ULOW> operator/=(const ULOW &Y)
    {
        ULOW remainder;
        std::tie(*this, remainder) = *this / Y;
        return std::make_pair(*this, remainder);
    }

    /**
         * @brief Оператор деления.
         * @details Авторский метод деления двух "широких" чисел, состоящих из двух половинок - "узких" чисел.
         * Отсутствует "раскачка" алгоритма для "плохих" случаев деления: (A*w + B)/(1*w + D).
         * @return Частное от деления и остаток.
         */
    std::pair<UBig, UBig> operator/(const UBig &other) const
    {
        assert(other != UBig{0});
        const UBig &X = *this;
        const UBig &Y = other;
        if (Y.mHigh == 0)
        {
            const auto &result = X / Y.mLow;
            return {result.first, UBig{result.second}};
        }
        constexpr auto MAX_ULOW = ULOW::get_max_value();
        const auto &[Q, R] = X.mHigh / Y.mHigh;
        const ULOW &Delta = MAX_ULOW - Y.mLow;
        const UBig &DeltaQ = mult_ext(Delta, Q);
        const UBig &sum_1 = UBig{0, R} + DeltaQ;
        UBig W1{sum_1 - UBig{0, Q}};
        const bool make_inverse_1 = sum_1 < UBig{0, Q};
        if (make_inverse_1)
            W1 = - W1;
        const ULOW &C1 = (Y.mHigh < MAX_ULOW) ? Y.mHigh + ULOW{1} : MAX_ULOW;
        const ULOW &W2 = MAX_ULOW - (Delta / C1).first;
        auto [Quotient, _] = W1 / W2;
        std::tie(Quotient, std::ignore) = Quotient / C1;
        if (make_inverse_1)
            Quotient = - Quotient;
        UBig result = UBig{Q} + Quotient - UBig{make_inverse_1 ? 1ull : 0ull};
        const UBig &N = Y * result.mLow;
        UBig Error{X - N};
        const bool negative_error = X < N;
        while (Error >= Y)
        {
            if (negative_error)
            {
                result.dec();
                Error += Y;
            }
            else
            {
                result.inc();
                Error -= Y;
            }
        }
        return std::make_pair(result, Error);
    }

    /**
         * @brief
         */
    std::pair<UBig, UBig> operator/=(const UBig &Y)
    {
        UBig remainder;
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
    u64 bit_length() const
    {
        UBig X = *this;
        u64 result = 0;
        while (X != UBig{0})
        {
            result++;
            X >>= 1;
        }
        return result;
    }

    /**
         * @brief Получить максимальное значение числа.
         */
    static constexpr UBig get_max_value()
    {
        return UBig{ULOW::get_max_value(), ULOW::get_max_value()};
    }

    /**
         * @brief Умножение двух N/2-битных чисел с расширением до N-битного числа.
         * @details Авторский алгоритм умножения. Обобщается на любую разрядность.
         */
    static UBig mult_ext(ULOW x, ULOW y)
    {
        constexpr int QUORTER_WIDTH = WIDTH / 4; // Четверть ширины N-битного числа.
        const ULOW& MASK = (ULOW{1} << QUORTER_WIDTH) - ULOW{1};
        const ULOW &x_low = x & MASK;
        const ULOW &y_low = y & MASK;
        const ULOW &x_high = x >> QUORTER_WIDTH;
        const ULOW &y_high = y >> QUORTER_WIDTH;
        const ULOW &t1 = x_low * y_low;
        const ULOW &t = t1 >> QUORTER_WIDTH;
        const ULOW &t21 = x_low * y_high;
        const ULOW &q = t21 >> QUORTER_WIDTH;
        const ULOW &p = t21 & MASK;
        const ULOW &t22 = x_high * y_low;
        const ULOW &s = t22 >> QUORTER_WIDTH;
        const ULOW &r = t22 & MASK;
        const ULOW &t3 = x_high * y_high;
        UBig result{t1};
        const ULOW &div = (q + s) + ((p + r + t) >> QUORTER_WIDTH);
        const auto &p1 = t21 << QUORTER_WIDTH;
        const auto &p2 = t22 << QUORTER_WIDTH;
        const ULOW &mod = p1 + p2;
        result.mLow += mod;
        result.mHigh += div;
        result.mHigh += t3;
        return result;
    }

    /**
         * @brief Специальный метод деления на 10 для формирования строкового представления числа.
         */
    UBig div10() const
    {
        const UBig &X = *this;
        constexpr auto TEN = ULOW{10};
        const auto& [reciprocal, _] = ULOW::get_max_value() / TEN;
        auto [Q, R] = X.mHigh / TEN;
        ULOW N = R * reciprocal + (X.mLow / TEN).first;
        UBig result{N, Q};
        for (UBig E{X - result * TEN}; E >= TEN; E -= TEN * UBig{N, Q})
        {
            std::tie(Q, R) = E.mHigh / TEN;
            N = R * reciprocal + (E.mLow / TEN).first;
            result += UBig{N, Q};
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
        UBig X = *this;
        while (X != UBig{0})
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
};

}
