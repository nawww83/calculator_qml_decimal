/**
 * @author nawww83@gmail.com
 * @brief Класс для арифметики 128-битных знаковых целых чисел с переполнением.
 */

#include <cstdint>      // uint64_t, uint32_t, uint16_t, uint8_t
#include "u128.hpp"     // U128
#include "sign.hpp"     // Sign
#include "singular.hpp" // Singular
#include "ulow.hpp"     // low64::ULOW
#include "defines.h"

#pragma once

namespace bignum::i128
{
    using U128 = bignum::u128::U128;
    using Sign = sign::Sign<uint32_t>;
    using Singular = singular::Singular<uint16_t>;

    using u64 = uint64_t;

    /**
     * @brief Тип половинки числа.
     */
    using ULOW = low64::ULOW;

    /**
     * Класс для арифметики 128-битных знаковых целых чисел с переполнением, основанный на классе беззнаковых чисел U128 и знаковом манипуляторе.
     */
    class I128
    {
    public:
        /**
         * @brief Конструктор по умолчанию.
         */
        explicit constexpr I128() = default;

        /**
         * @brief Конструктор с параметром.
         */
        constexpr I128(u64 low) : mUnsigned{low}
        {
            ;
        }

        /**
         * @brief Конструктор с параметрами.
         */
        constexpr I128(U128 x, Sign sign = false) : mUnsigned{x}, mSign{sign}
        {
            ;
        }

        /**
         * @brief Конструктор с параметрами.
         */
        constexpr I128(U128 x, Sign sign, Singular singularity) : mUnsigned{x}, mSign{sign}, mSingular{singularity}
        {
            ;
        }

        constexpr I128(const I128 &other) = default;

        constexpr I128(I128 &&other) = default;

        constexpr I128 &operator=(const I128 &other) = default;

        /**
         * @brief Оператор сравнения.
         */
        bool operator==(const I128 &other) const
        {
            const auto has_a_singularity = mSingular != other.mSingular;
            return has_a_singularity ? false : *this <=> other == 0;
        }

        /**
         * @brief Остальные операторы, кроме оператора сравнения.
         */
        std::partial_ordering operator<=>(const I128 &other) const
        {
            const auto has_a_singularity = mSingular != other.mSingular;
            if (has_a_singularity)
            {
                return std::partial_ordering::unordered;
            }
            const bool signs_are_equal = mSign() == other.mSign();
            if (signs_are_equal)
            {
                return mSign() ? other.mUnsigned <=> mUnsigned : mUnsigned <=> other.mUnsigned;
            }
            else
            {
                if (mSign())
                {
                    return mUnsigned != U128{0} ? -1 <=> 1 : 1 <=> 1;
                }
                else
                {
                    return mUnsigned != U128{0} ? 1 <=> -1 : 1 <=> 1;
                }
            }
        }

        bool is_singular() const
        {
            return mSingular();
        }

        bool is_overflow() const
        {
            return mSingular.is_overflow() && !mSingular.is_nan();
        }

        bool is_nan() const
        {
            return mSingular.is_nan() && !mSingular.is_overflow();
        }

        bool is_zero() const
        {
            return mUnsigned == U128{0} && !is_singular();
        }

        /**
         * @brief x < 0
         */
        bool is_negative() const
        {
            return !is_zero() && mSign() && !is_singular();
        }

        /**
         * @brief x > 0
         */
        bool is_positive() const
        {
            return !is_zero() && !mSign() && !is_singular();
        }

        /**
         * @brief x >= 0
         */
        bool is_nonegative() const
        {
            return is_positive() || is_zero();
        }

        void set_overflow()
        {
            mSingular.set_overflow();
        }

        void set_nan()
        {
            mSingular.set_nan();
        }

        /**
         * @brief Оператор сдвига влево. При больших сдвигах дает ноль. Не меняет знак.
         */
        I128 operator<<(uint32_t shift) const
        {
            I128 result = *this;
            result.mUnsigned <<= shift;
            return result;
        }

        /**
         * @brief Оператор сдвига влево.
         */
        I128 &operator<<=(uint32_t shift)
        {
            *this = *this << shift;
            return *this;
        }

        /**
         * @brief Оператор сдвига вправо. При больших сдвигах дает ноль. Не меняет знак.
         */
        I128 operator>>(uint32_t shift) const
        {
            I128 result = *this;
            result.mUnsigned <<= shift;
            return result;
        }

        /**
         * @brief Оператор сдвига вправо.
         */
        I128 &operator>>=(uint32_t shift)
        {
            *this = *this >> shift;
            return *this;
        }

        /**
         * @brief Оператор побитового И. Сохраняет знак.
         */
        I128 operator&(const I128 &mask) const
        {
            I128 result = *this;
            result.mUnsigned &= mask.mUnsigned;
            return result;
        }

        /**
         * @brief Оператор побитового И.
         */
        I128 &operator&=(const I128 &mask)
        {
            *this = *this & mask;
            return *this;
        }

        /**
         * @brief Оператор побитового ИЛИ. Сохраняет знак.
         */
        I128 operator|(const I128 &mask) const
        {
            I128 result = *this;
            result.mUnsigned |= mask.mUnsigned;
            return result;
        }

        /**
         * @brief Оператор побитового ИЛИ.
         */
        I128 &operator|=(const I128 &mask)
        {
            *this = *this | mask;
            return *this;
        }

        /**
         * @brief Оператор исключающего ИЛИ. Сохраняет знак.
         */
        I128 operator^(const I128 &mask) const
        {
            I128 result = *this;
            result.mUnsigned ^= mask.mUnsigned;
            return result;
        }

        /**
         * @brief Оператор исключающего ИЛИ.
         */
        I128 &operator^=(const I128 &mask)
        {
            *this = *this ^ mask;
            return *this;
        }

        /**
         * @brief Оператор инверсии битов. Сохраняет знак.
         */
        I128 operator~() const
        {
            I128 result = *this;
            result.mUnsigned = ~result.mUnsigned;
            return result;
        }

        /**
         * @brief Возвращает абсолютное значение числа.
         */
        I128 abs() const
        {
            I128 result = *this;
            result.mSign.set_sign(false);
            return result;
        }

        /**
         * @brief Оператор суммирования.
         */
        I128 operator+(const I128 &rhs) const
        {
            I128 X = *this;
            I128 Y = rhs;
            if (X.is_singular())
            {
                return X;
            }
            if (Y.is_singular())
            {
                X.mSingular = Y.mSingular;
                return X;
            }
            I128 result;
            if (X.is_negative() && !Y.is_negative())
            {
                X.mSign.set_sign(false);
                result = Y - X;
                return result;
            }
            if (!X.is_negative() && Y.is_negative())
            {
                Y.mSign.set_sign(false);
                result = X - Y;
                return result;
            }
            result.mUnsigned = X.mUnsigned + Y.mUnsigned;
            if (X.is_negative() && Y.is_negative())
            {
                result.mSign.set_sign(true);
            }
            if (result.mUnsigned < std::min(X.mUnsigned, Y.mUnsigned))
            {
                result.mSingular.set_overflow();
            }
            return result;
        }

        /**
         * @brief Оператор суммирования.
         */
        I128 &operator+=(const I128 &Y)
        {
            *this = *this + Y;
            return *this;
        }

        /**
         * @brief Оператор вычитания.
         */
        I128 operator-(const I128 &rhs) const
        {
            I128 X = *this;
            I128 Y = rhs;
            if (X.is_singular())
            {
                return X;
            }
            if (Y.is_singular())
            {
                X.mSingular = Y.mSingular;
                return X;
            }
            I128 result;
            if (X.is_negative() && !Y.is_negative())
            {
                Y.mSign.set_sign(true);
                result = Y + X;
                return result;
            }
            if (!X.is_negative() && Y.is_negative())
            {
                Y.mSign.set_sign(false);
                result = X + Y;
                return result;
            }
            if (X.is_negative() && Y.is_negative())
            {
                Y.mSign.set_sign(false);
                X.mSign.set_sign(false);
                result = Y - X;
                return result;
            }
            if (X.is_zero())
            {
                result = Y;
                -result.mSign;
                return result;
            }
            if (X.mUnsigned >= Y.mUnsigned)
            {
                result.mUnsigned = X.mUnsigned - Y.mUnsigned;
            }
            else
            {
                result.mUnsigned = Y.mUnsigned - X.mUnsigned;
                result.mSign.set_sign(true);
            }
            return result;
        }

        /**
         * @brief Оператор вычитания.
         */
        I128 &operator-=(const I128 &other)
        {
            *this = *this - other;
            return *this;
        }

        /**
         * @brief Знак минус.
         */
        I128 operator-() const
        {
            I128 result = *this;
            -result.mSign;
            return result;
        }

        /**
         * @brief Оператор половинчатого умножения.
         */
        I128 operator*(const ULOW &rhs) const
        {
            const I128 &X = *this;
            const ULOW &Y = rhs;
            if (X.is_overflow())
            {
                I128 result;
                result.set_overflow();
                return result;
            }
            if (X.is_nan())
            {
                I128 result;
                result.set_nan();
                return result;
            }
            I128 result{U128::mult64(X.mUnsigned.low(), Y)};
            I128 tmp{U128::mult64(X.mUnsigned.high(), Y)};
            result += shl64(tmp);
            result.mSign = X.mSign();
            return result;
        }

        /**
         * @brief Оператор умножения.
         */
        I128 operator*(const I128 &rhs) const
        {
            const I128 &X = *this;
            const I128 &Y = rhs;
            if (X.is_overflow() || Y.is_overflow())
            {
                I128 result;
                result.set_overflow();
                return result;
            }
            if (X.is_nan() || Y.is_nan())
            {
                I128 result;
                result.set_nan();
                return result;
            }
            I128 result = X * Y.mUnsigned.low();
            if (result.is_singular())
                return result;
            result.mSign = X.mSign ^ Y.mSign;
            result += shl64(X * Y.mUnsigned.high());
            return result;
        }

        /**
         * @brief Оператор половинчатого деления.
         */
        std::pair<I128, ULOW> operator/(const ULOW &rhs) const
        {
            assert(rhs != 0);
            const I128 &X = *this;
            const ULOW &Y = rhs;
            if (X.is_overflow())
            {
                I128 result;
                result.set_overflow();
                return {result, 0};
            }
            if (X.is_nan())
            {
                I128 result;
                result.set_nan();
                return {result, 0};
            }
            const auto& [q, r] = X.mUnsigned / Y;
            auto Q = I128{q, X.mSign};
            auto R = X - Q * Y;
            if (R.is_negative())
            {
                R += I128{Y};
                Q -= I128{1};
            }
            return {Q, R.mUnsigned.low()};
        }

        /**
         * @brief Оператор деления.
         */
        std::pair<I128, I128> operator/(const I128 &rhs) const
        {
            assert(!rhs.is_zero());
            const I128 &X = *this;
            const I128 &Y = rhs;
            if (X.is_overflow() || Y.is_overflow())
            {
                I128 result;
                result.set_overflow();
                return {result, 0};
            }
            if (X.is_nan() || Y.is_nan())
            {
                I128 result;
                result.set_nan();
                return {result, 0};
            }
            const auto& [q, r] = X.mUnsigned / Y.mUnsigned;
            auto Q = I128{q, X.mSign ^ Y.mSign};
            auto R = X - Q * Y;
            if (!R.is_zero() && (R.mSign ^ Y.mSign)())
            {
                R += I128{Y};
                Q -= I128{1};
            }
            return {Q, R};
        }

        /**
         * @brief Количество битов, требуемое для представления числа.
         */
        u64 bit_length() const
        {
            return mUnsigned.bit_length();
        }

        /**
         * @brief Возвращает беззнаковую часть числа.
         */
        U128 unsigned_part() const
        {
            return mUnsigned;
        }

        /**
         * @brief Сдвиг влеово на 64 бита беззнаковой части.
         * Сохраняет знак. С переполнением.
         */
        static I128 shl64(const I128 &x)
        { // x * 2^64
            I128 result{x.mUnsigned << 64, x.mSign, x.mSingular};
            if (x.mUnsigned >= U128{0, 1} && !x.is_singular())
                result.set_overflow();
            return result;
        }

        /**
         * @brief Возвращает строковое представление числа.
         */
        std::string value() const
        {
            std::string result;
            if (this->is_nan())
            {
                return "nan";
            }
            if (this->is_overflow())
            {
                return "inf";
            }
            U128 X = this->mUnsigned;
            while (X != U128{0})
            {
                const int d = X.mod10();
                if (d < 0)
                    return result;
                result.push_back(DIGITS[d]);
                X = X.div10();
            }
            if (!result.empty() && this->mSign())
            {
                result.push_back('-');
            }
            std::reverse(result.begin(), result.end());
            return !result.empty() ? result : "0";
        }

    private:
        /**
         * @brief Беззнаковая часть числа.
         */
        U128 mUnsigned{0};

        /**
         * @brief Знак числа.
         */
        Sign mSign{false};

        /**
         * @brief Признак сингулярности числа: переполнение или "нечисло".
         */
        Singular mSingular{false};
    };
}