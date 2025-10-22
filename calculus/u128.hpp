/**
 * @author nawww83@gmail.com
 * @brief Класс для арифметики 128-битных беззнаковых целых чисел.
 */

#pragma once

#include <cstdint>   // uint64_t
#include <cassert>   // assert
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
        assert(x != U{0});
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
    inline U smart_remainder_adder(U &r, const U& delta, const U &m, const U &r_rec)
    {
        assert(m != U{0});
        auto [_, delta_m] = delta / m;
        const U &summ = r + delta_m;
        const bool overflow = summ < std::min(r, delta_m);
        r = summ + (overflow ? r_rec : 0ull);
        auto [__, r_m] = r / m;
        r = r_m;
        return overflow ? 1ull : (summ >= m ? 1ull : 0ull);
    }
}

namespace bignum::u128
{
#ifdef USE_DIV_COUNTERS
    inline double g_min_loops_when_div = 1. / 0.;
    inline double g_max_loops_when_div = 0;
    inline double g_average_loops_when_div = 0;

    inline double g_all_divs = 0;

    inline double g_min_loops_when_half_div = 1. / 0.;
    inline double g_max_loops_when_half_div = 0;
    inline double g_average_loops_when_half_div = 0;

    inline double g_all_half_divs = 0;

    inline uint64_t g_hist[128];
#endif
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
            {
                return U128{0};
            }
            U128 result = *this;
            int ishift = shift;
            ULOW L{0};
            if (ishift < 64)
            {
                L = ishift == 0 ? L : result.mLow >> (64 - ishift);
            }
            else
            {
                result.mHigh = std::exchange(result.mLow, 0);
                ishift -= 64;
            }
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
            {
                return U128{0};
            }
            U128 result = *this;
            int ishift = shift;
            if (ishift < 64)
            {
                ULOW mask = ULOW::get_max_value();
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
            if (X >= Y)
            {
                const auto &high = (X.mHigh - Y.mHigh) - ULOW{X.mLow < Y.mLow ? 1ull : 0ull};
                return U128{X.mLow - Y.mLow, high};
            }
            return (X + (U128::get_max_value() - Y)).inc();
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
            const U128 ac = mult64(X.mLow, Y.mLow);
            const U128 ad = mult64(X.mLow, Y.mHigh);
            const U128 bc = mult64(X.mHigh, Y.mLow);
            U128 result = ad + bc;
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
            U128 result{mult64(X.mHigh, Y)};
            result <<= 64;
            result += mult64(X.mLow, Y);
            return result;
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
         * @brief Оператор половинчатого деления.
         * @details Авторский метод итеративного деления "широкого" числа на "узкое".
         * Количество итераций: ~[3, 42], наиболее вероятное количество - 15, среднее - около 21.
         * @return Частное от деления Q и остаток R.
         */
        std::pair<U128, ULOW> operator/(const ULOW &Y) const
        {
            assert(Y != 0);
            U128 X = *this;
            if (Y == ULOW{1})
            {
                return {X, 0};
            }
#ifdef USE_DIV_COUNTERS
            g_all_half_divs++;
            double loops = 0;
#endif
            U128 Q{0};
            ULOW R = 0;
            auto rcp = generic::reciprocal_and_extend<ULOW>(Y);
            const auto &rcp_compl = Y - rcp.second;
            const bool make_inverse = rcp_compl < rcp.second; // Для ускорения сходимости.
            rcp.first += make_inverse ? ULOW{1} : ULOW{0};
            const auto X_old = X;
            for (;;)
            {
#ifdef USE_DIV_COUNTERS
                loops++;
                assert(loops < 128);
#endif
                const bool x_has_high = X.high() != 0;
                Q += x_has_high ? U128::mult64(X.high(), rcp.first) : 0ull;
                Q += U128{(X.low() / Y).first};
                const auto& carry = generic::smart_remainder_adder(R, X.low(), Y, rcp.second);
                Q += carry;
                X = X.high() != 0ull ? U128::mult64(X.high(), make_inverse ? rcp_compl : rcp.second) : 0ull;
                if (X == U128{0})
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
#ifdef USE_DIV_COUNTERS
            g_hist[static_cast<uint64_t>(loops)]++;
            g_average_loops_when_half_div += (loops - g_average_loops_when_half_div) / g_all_half_divs;
            g_max_loops_when_half_div = std::max(g_max_loops_when_half_div, loops);
            g_min_loops_when_half_div = std::min(g_min_loops_when_half_div, loops);
#endif
            return {Q, R};
        }

        /**
         * @brief
         */
        std::pair<U128, ULOW> operator/=(const ULOW &Y)
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
        std::pair<U128, U128> operator/(const U128 &other) const
        {
            assert(other != U128{0});
            const U128 &X = *this;
            const U128 &Y = other;
            if (Y.mHigh == 0)
            {
                const auto &result = X / Y.low();
                return {result.first, U128{result.second}};
            }
            constexpr auto MAX_ULOW = ULOW::get_max_value();
            const auto &[Q, R] = X.high() / Y.high();
            const auto &Delta = MAX_ULOW - Y.low();
            const U128 &DeltaQ = mult64(Delta, Q);
            const U128 &sum_1 = U128{0, R} + DeltaQ;
            U128 W1{sum_1 - U128{0, Q}};
            const bool make_inverse_1 = sum_1 < U128{0, Q};
            if (make_inverse_1)
                W1 = - W1;
#ifdef USE_DIV_COUNTERS
            g_all_divs++;
            double loops = 0;
#endif
            const auto &C1 = (Y.mHigh < MAX_ULOW) ? Y.mHigh + ULOW{1} : MAX_ULOW;
            const auto &W2 = MAX_ULOW - (Delta / C1).first;
            auto [Quotient, _] = W1 / W2;
            std::tie(Quotient, std::ignore) = Quotient / C1;
            if (make_inverse_1)
                Quotient = - Quotient;
            U128 result = U128{Q} + Quotient - U128{make_inverse_1 ? 1ull : 0ull};
            const U128 &N = Y * result.mLow;
            U128 Error{X - N};
            const bool negative_error = X < N;
            while (Error >= Y)
            {
#ifdef USE_DIV_COUNTERS
                loops++;
                assert(loops < 128);
#endif
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
#ifdef USE_DIV_COUNTERS
            g_hist[static_cast<uint64_t>(loops)]++;
            g_average_loops_when_div += (loops - g_average_loops_when_div) / g_all_divs;
            g_max_loops_when_div = std::max(g_max_loops_when_div, loops);
            g_min_loops_when_div = std::min(g_min_loops_when_div, loops);
#endif
            return std::make_pair(result, Error);
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
        u64 bit_length() const
        {
            U128 X = *this;
            u64 result = 0;
            while (X != U128{0})
            {
                result++;
                X >>= 1;
            }
            return result;
        }

        /**
         * @brief Количество непрерывно идущих нулей битового представления числа, начиная с самого старшего бита.
         */
        int countl_zero() const
        {
            if (mHigh() == 0)
                return 64 + mLow.countl_zero();
            return mHigh.countl_zero();   
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
        static U128 mult64(ULOW x, ULOW y)
        {
            constexpr int QUORTER_WIDTH = 32; // Четверть ширины 128-битного числа.
            constexpr ULOW MASK = (ULOW{1}() << QUORTER_WIDTH) - 1;
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
            U128 result{t1};
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
        U128 div10() const
        {
            const U128 &X = *this;
            constexpr auto TEN = ULOW{10};
            const auto& reciprocal = (ULOW::get_max_value() / TEN).first;
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
    };

}