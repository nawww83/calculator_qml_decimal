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
#include "defines.h"

#include <bit>
#include <compare>

#if defined(_MSC_VER) && (_MSC_VER >= 1920) && defined(_M_X64)
#define USE_MSVC_INTRINSICS_DIVISION
#endif

#if defined(_MSC_VER) && defined(_M_X64)
#define USE_MSVC_INTRINSICS
#endif

#ifdef USE_MSVC_INTRINSICS_DIVISION
#include <immintrin.h>
#endif

#ifdef USE_MSVC_INTRINSICS
#include <intrin.h>
#pragma intrinsic(_umul128)
#endif

namespace bignum::u128
{
using u64 = uint64_t;
using u32 = uint32_t;
class U128
{
    u64 mLow{0};
    u64 mHigh{0};

public:
    // --- Конструкторы ---
    constexpr U128() noexcept = default;
    constexpr U128(u64 low, u64 high) noexcept : mLow{low}, mHigh{high} {}
    constexpr U128(u64 low) noexcept : mLow{low}, mHigh{0} {}

    // --- Доступ к данным ---
    [[nodiscard]] constexpr u64 low() const noexcept { return mLow; }
    [[nodiscard]] constexpr u64 high() const noexcept { return mHigh; }
    [[nodiscard]] constexpr u64 &low() noexcept { return mLow; }
    [[nodiscard]] constexpr u64 &high() noexcept { return mHigh; }

    // --- Сравнение ---
    bool operator==(const U128 &other) const noexcept
    {
        return mLow == other.mLow && mHigh == other.mHigh;
    }
    std::strong_ordering operator<=>(const U128 &other) const noexcept
    {
        if (auto cmp = mHigh <=> other.mHigh; cmp != 0)
            return cmp;
        return mLow <=> other.mLow;
    }

    // --- Сложение ---
    constexpr U128 &operator+=(const U128 &other) noexcept
    {
#if defined(__SIZEOF_INT128__)
        unsigned __int128 val = to_u128() + other.to_u128();
        mLow = static_cast<u64>(val);
        mHigh = static_cast<u64>(val >> 64);
#else
        u64 old_low = mLow;
        mLow += other.mLow;

        // Сначала определяем наличие переноса из младшей части
        u64 carry = (mLow < old_low) ? 1ull : 0ull;

        // Затем прибавляем всё к старшей части
        mHigh += other.mHigh + carry;
#endif
        return *this;
    }
    U128 operator+(const U128 &other) const noexcept { return U128(*this) += other; }

    // --- Вычитание ---
    constexpr U128 &operator-=(const U128 &other) noexcept
    {
#if defined(__SIZEOF_INT128__)
        auto val = to_u128() - other.to_u128();
        mLow = static_cast<u64>(val);
        mHigh = static_cast<u64>(val >> 64);
#else
        u64 old_low = mLow;
        mLow -= other.mLow;
        mHigh -= other.mHigh + (old_low < other.mLow ? 1ull : 0ull);
#endif
        return *this;
    }
    U128 operator-(const U128 &other) const noexcept { return U128(*this) -= other; }

    /**
         * @brief Унарный минус (отрицание).
         * @details В беззнаковой арифметике возвращает (2^128 - *this).
         */
    [[nodiscard]] constexpr U128 operator-() const noexcept
    {
#if defined(__SIZEOF_INT128__)
        unsigned __int128 val = to_u128();
        val = -val; // Нативная поддержка дополнения до двух
        return {static_cast<u64>(val), static_cast<u64>(val >> 64)};
#else
        // Побитовая инверсия + 1 (стандартный алгоритм для дополнения до двух)
        U128 res = ~(*this);
        return ++res;
#endif
    }

    // --- Умножение ---
    U128 &operator*=(const U128 &other) noexcept
    {
        *this = (*this * other);
        return *this;
    }
    U128 operator*(const U128 &other) const noexcept
    {
#if defined(__SIZEOF_INT128__)
        auto res = to_u128() * other.to_u128();
        return {static_cast<u64>(res), static_cast<u64>(res >> 64)};
#else
        U128 res = mult_ext(mLow, other.mLow);
        res.mHigh += (mLow * other.mHigh) + (mHigh * other.mLow);
        return res;
#endif
    }

    static U128 mult_ext(u64 x, u64 y) noexcept
    {
#if defined(__SIZEOF_INT128__)
        unsigned __int128 res = static_cast<unsigned __int128>(x) * y;
        return {static_cast<u64>(res), static_cast<u64>(res >> 64)};
#elif defined(_MSC_VER)
        u64 hi, lo = _umul128(x, y, &hi);
        return {lo, hi};
#else
        return mult_ext_manual(x, y);
#endif
    }

    /**
         * @brief Возведение в квадрат 64-битного числа с расширением до 128-битного числа.
         */
    static U128 square_ext(u64 x) noexcept
    {
#if defined(__SIZEOF_INT128__)
        unsigned __int128 res = static_cast<unsigned __int128>(x) * static_cast<unsigned __int128>(x);
        return {static_cast<u64>(res), static_cast<u64>(res >> 64)};
#elif defined(USE_MSVC_INTRINSICS)
        u64 hi, lo = _umul128(x, x, &hi);
        return {lo, hi};
#else
        return square_ext_manual(x);
#endif
    }

    // --- Деление и остаток ---
    U128 &operator/=(const U128 &other) noexcept
    {
        return *this = divide<true, false>(*this, other, nullptr);
    }

    U128 operator/(const U128 &other) const noexcept
    {
        return divide<true, false>(*this, other, nullptr);
    }

    U128 &operator%=(const U128 &other) noexcept
    {
        U128 rem;
        divide<false, true>(*this, other, &rem);
        return *this = rem;
    }

    U128 operator%(const U128 &other) const noexcept
    {
        U128 rem;
        divide<false, true>(*this, other, &rem);
        return rem;
    }

    // --- Сдвиги ---
    U128 &operator<<=(uint32_t s) noexcept
    {
        if (s >= 128)
        {
            mLow = mHigh = 0;
        }
        else if (s >= 64)
        {
            mHigh = mLow << (s - 64);
            mLow = 0;
        }
        else if (s > 0)
        {
            mHigh = (mHigh << s) | (mLow >> (64 - s));
            mLow <<= s;
        }
        return *this;
    }
    U128 operator<<(uint32_t s) const noexcept { return U128(*this) <<= s; }

    U128 &operator>>=(uint32_t s) noexcept
    {
        if (s >= 128)
        {
            mLow = mHigh = 0;
        }
        else if (s >= 64)
        {
            mLow = mHigh >> (s - 64);
            mHigh = 0;
        }
        else if (s > 0)
        {
            mLow = (mLow >> s) | (mHigh << (64 - s));
            mHigh >>= s;
        }
        return *this;
    }
    U128 operator>>(uint32_t s) const noexcept { return U128(*this) >>= s; }

    // --- Инкремент / Декремент ---
    U128 &operator++() noexcept
    {
        if (++mLow == 0)
            ++mHigh;
        return *this;
    }
    U128 operator++(int) noexcept
    {
        U128 tmp(*this);
        ++(*this);
        return tmp;
    }
    U128 &operator--() noexcept
    {
        if (mLow-- == 0)
            --mHigh;
        return *this;
    }
    U128 operator--(int) noexcept
    {
        U128 tmp(*this);
        --(*this);
        return tmp;
    }

    // --- Логические ---
    constexpr U128 operator~() const noexcept { return {~mLow, ~mHigh}; }
    constexpr U128 &operator&=(const U128 &other) noexcept
    {
        mLow &= other.mLow;
        mHigh &= other.mHigh;
        return *this;
    }
    constexpr U128 &operator|=(const U128 &other) noexcept
    {
        mLow |= other.mLow;
        mHigh |= other.mHigh;
        return *this;
    }
    constexpr U128 &operator^=(const U128 &other) noexcept
    {
        mLow ^= other.mLow;
        mHigh ^= other.mHigh;
        return *this;
    }

    // --- Побитовые операторы (Binary) ---
    [[nodiscard]] constexpr U128 operator&(const U128 &other) const noexcept { return U128(*this) &= other; }
    [[nodiscard]] constexpr U128 operator|(const U128 &other) const noexcept { return U128(*this) |= other; }
    [[nodiscard]] constexpr U128 operator^(const U128 &other) const noexcept { return U128(*this) ^= other; }

    // --- Логические проверки ---
    // Позволяет писать: if (val), if (!val), val && other, val || other
    [[nodiscard]] constexpr explicit operator bool() const noexcept
    {
        return mLow != 0 || mHigh != 0;
    }

    [[nodiscard]] constexpr bool operator!() const noexcept
    {
        return mLow == 0 && mHigh == 0;
    }

    /**
         * @brief Возвращает максимально возможное значение U128 (2^128 - 1).
         */
    [[nodiscard]] static constexpr U128 max() noexcept
    {
        return U128{0xFFFFFFFFFFFFFFFFULL, 0xFFFFFFFFFFFFFFFFULL};
    }

    /**
         * @brief Возвращает минимальное значение (0).
         */
    [[nodiscard]] static constexpr U128 min() noexcept
    {
        return U128{0, 0};
    }

    /**
         * @brief Подсчет количества ведущих нулей (Count Leading Zeros).
         * @return Количество нулевых бит слева (от 0 до 128).
         */
    [[nodiscard]] constexpr int countl_zero() const noexcept
    {
        if (mHigh != 0)
        {
            return std::countl_zero(mHigh);
        }
        return 64 + std::countl_zero(mLow);
    }

    /**
         * @brief Подсчет количества завершающих нулей (Count Trailing Zeros).
         * @return Количество нулевых бит справа (от 0 до 128).
         */
    [[nodiscard]] constexpr int countr_zero() const noexcept
    {
        if (mLow != 0)
        {
            return std::countr_zero(mLow);
        }
        return 64 + std::countr_zero(mHigh);
    }

    /**
         * @brief Подсчет количества установленных бит (Population Count).
         * @return Общее количество бит, равных 1.
         */
    [[nodiscard]] constexpr int popcount() const noexcept
    {
        return std::popcount(mHigh) + std::popcount(mLow);
    }

    /**
         * @brief Минимальное количество бит для представления числа.
         */
    [[nodiscard]] constexpr uint32_t bit_width() const noexcept
    {
        return 128 - countl_zero();
    }

    // --- Вспомогательный метод (Ручное деление) ---
    template <bool make_quotient, bool make_remainder>
    static U128 divide_manual(const U128 &dividend, const U128 &divisor, U128 *rem_out)
    {
        auto udiv128_64 = [](u64 high, u64 low, u64 d, u64 *r) -> u64
        {
#if defined(USE_MSVC_INTRINSICS_DIVISION)
            return _udiv128(high, low, d, r);
#else
            // Ваша эмуляция через 32-битные части (из Пути 2)
            u64 rem = high % d;
            u64 p_hi = low >> 32;
            u64 p_lo = low & 0xFFFFFFFF;
            u64 q_hi = ((rem << 32) | p_hi) / d;
            rem = ((rem << 32) | p_hi) % d;
            u64 q_lo = ((rem << 32) | p_lo) / d;
            if (r)
                *r = ((rem << 32) | p_lo) % d;
            return (q_hi << 32) | q_lo;
#endif
        };

        if (divisor.mHigh == 0)
        {
            u64 r0, r1;
            u64 q1 = udiv128_64(0, dividend.mHigh, divisor.mLow, &r0);
            u64 q0 = udiv128_64(r0, dividend.mLow, divisor.mLow, &r1);
            if constexpr (make_remainder)
                if (rem_out)
                    *rem_out = {r1, 0};
            if constexpr (make_quotient)
                return {q0, q1};
            return {0, 0};
        }

        u32 s = std::countl_zero(divisor.mHigh);
        U128 v = divisor << s;
        U128 u = dividend;

        // Исправляем UB при s = 0
        u64 h_in = (s == 0) ? 0 : (u.mHigh >> (64 - s));
        u64 l_in = (u.mHigh << s);
        if (s > 0)
            l_in |= (u.mLow >> (64 - s));

        u64 r_tmp;
        u64 q_h = udiv128_64(h_in, l_in, v.mHigh, &r_tmp);

        U128 q_res{q_h, 0};
        U128 prod = q_res * divisor;

        while (prod > dividend)
        {
            prod -= divisor;
            q_res.mLow--;
        }

        // 2. Если недобрали (q слишком мала) — увеличиваем
        // Важно: остаток (dividend - prod) должен быть меньше делителя
        for (;;)
        {
            U128 current_rem = dividend - prod;
            if (current_rem >= divisor)
            {
                prod += divisor;
                q_res.mLow++;
            }
            else
            {
                if constexpr (make_remainder)
                    if (rem_out)
                        *rem_out = current_rem;
                break;
            }
        }

        if constexpr (make_remainder)
            if (rem_out)
                *rem_out = dividend - prod;
        if constexpr (make_quotient)
            return q_res;
        return {0, 0};
    }

    // --- Основной метод (Диспетчер) ---
    template <bool make_quotient, bool make_remainder>
    static U128 divide(const U128 &dividend, const U128 &divisor, U128 *rem_out)
    {
#if defined(__SIZEOF_INT128__)
        // ПУТЬ 1: Нативное деление
        unsigned __int128 a = dividend.to_u128(), b = divisor.to_u128();
        if constexpr (make_remainder)
            if (rem_out)
            {
                unsigned __int128 r = a % b;
                *rem_out = {static_cast<u64>(r), static_cast<u64>(r >> 64)};
            }
        if constexpr (make_quotient)
        {
            unsigned __int128 q = a / b;
            return {static_cast<u64>(q), static_cast<u64>(q >> 64)};
        }
        return {0, 0};
#else
        // ПУТЬ 2: Вызов ручного метода
        return divide_manual<make_quotient, make_remainder>(dividend, divisor, rem_out);
#endif
    }

    [[nodiscard]] std::string toString() const
    {
        if (mHigh == 0 && mLow == 0)
            return "0";

        U128 copy = *this;
        char buffer[44]; // С запасом
        char *ptr = buffer + 43;
        *ptr = '\0';

        constexpr uint64_t div_val = 10000000000000000000ULL; // 10^19

        while (copy.mHigh > 0 || copy.mLow > 0)
        {
            uint64_t rem = 0;

            // --- ВЕТКА 1: GCC / Clang (__int128) ---
#if defined(__SIZEOF_INT128__)
            unsigned __int128 val = (static_cast<unsigned __int128>(copy.mHigh) << 64) | copy.mLow;
            rem = static_cast<uint64_t>(val % div_val);
            val /= div_val;
            copy.mHigh = static_cast<uint64_t>(val >> 64);
            copy.mLow = static_cast<uint64_t>(val);

            // --- ВЕТКА 2: MSVC (_udiv128) ---
#elif defined(USE_MSVC_INTRINSICS_DIVISION)
            copy.mHigh = _udiv128(0, copy.mHigh, div_val, &rem);
            copy.mLow = _udiv128(rem, copy.mLow, div_val, &rem);

            // --- ВЕТКА 3: Универсальная эмуляция (32-bit parts) ---
#else
            // Делим high
            rem = copy.mHigh % div_val;
            copy.mHigh /= div_val;

            // Делим low как (rem << 64 | copy.mLow) / div_val
            // Используем школьное деление столбиком, разбивая low на две части по 32 бита
            uint64_t parts[2] = {copy.mLow >> 32, copy.mLow & 0xFFFFFFFF};
            uint64_t result_low = 0;

            for (int i = 0; i < 2; ++i)
            {
                uint64_t current = (rem << 32) | parts[i];
                uint64_t q = current / div_val;
                rem = current % div_val;
                result_low = (result_low << 32) | q;
            }
            copy.mLow = result_low;
#endif

            // Преобразование остатка (19 цифр) в символы
            uint64_t temp_rem = rem;

            if (copy.mHigh > 0 || copy.mLow > 0)
            {
                // Промежуточный блок: ОБЯЗАТЕЛЬНО 19 цифр (с ведущими нулями)
                for (int i = 0; i < 19; ++i)
                {
                    *(--ptr) = '0' + (temp_rem % 10);
                    temp_rem /= 10;
                }
            }
            else
            {
                // Последний (самый старший) блок: пишем до последнего значащего разряда
                do
                {
                    *(--ptr) = '0' + (temp_rem % 10);
                    temp_rem /= 10;
                } while (temp_rem > 0);
            }
        }

        return std::string(ptr);
    }

    static U128 fromString(const std::string &s)
    {
        if (s.empty() || s == "0")
            return {0, 0};

        U128 result{0, 0};
        size_t pos = 0;

        // Пропускаем возможные пробелы или знак '+'
        while (pos < s.length() && (s[pos] == ' ' || s[pos] == '+'))
            pos++;

        for (; pos < s.length();)
        {
            // Берем блок до 18 цифр
            size_t len = std::min<size_t>(18, s.length() - pos);
            uint64_t block_val = 0;
            uint64_t multiplier = 1;

            for (size_t i = 0; i < len; ++i)
            {
                if (s[pos] < '0' || s[pos] > '9')
                    break; // Ошибка формата
                block_val = block_val * 10 + (s[pos] - '0');
                multiplier *= 10;
                pos++;
            }

            // result = result * multiplier + block_val
            // Используем __int128 для скорости на GCC, либо ручное умножение
#if defined(__SIZEOF_INT128__)
            unsigned __int128 temp = result.to_u128();
            temp = temp * multiplier + block_val;
            result.mLow = static_cast<uint64_t>(temp);
            result.mHigh = static_cast<uint64_t>(temp >> 64);
#else
            // Ручное 128-битное умножение на 64-битное число
            U128 res_mul_high = {0, 0};
// Используем ваш макрос USE_MSVC_INTRINSICS, если он есть
#ifdef USE_MSVC_INTRINSICS
            unsigned __int64 high;
            result.mLow = _umul128(result.mLow, multiplier, &high);
            result.mHigh = result.mHigh * multiplier + high;
#else
            // Базовое умножение через 32-битные части (аналог "в столбик")
            // ... (для краткости можно вызвать оператор * если он определен)
            result = result * multiplier;
#endif
            // Прибавляем блок
            result.mLow += block_val;
            if (result.mLow < block_val)
                result.mHigh++; // Carry
#endif
        }
        return result;
    }

#if defined(__SIZEOF_INT128__)
    constexpr unsigned __int128 to_u128() const noexcept { return (static_cast<unsigned __int128>(mHigh) << 64) | mLow; }
#endif

}; // class U128

static inline U128 mult_ext_manual(u64 x, u64 y) noexcept
{
    const u64 x_low = x & 0xFFFFFFFF;
    const u64 x_high = x >> 32;
    const u64 y_low = y & 0xFFFFFFFF;
    const u64 y_high = y >> 32;
    const u64 t1 = x_low * y_low;
    const u64 t21 = x_low * y_high;
    const u64 t22 = x_high * y_low;
    const u64 t3 = x_high * y_high;
    const u64 mid = (t1 >> 32) + (t21 & 0xFFFFFFFF) + (t22 & 0xFFFFFFFF);
    return {(t1 & 0xFFFFFFFF) | (mid << 32),
            t3 + (t21 >> 32) + (t22 >> 32) + (mid >> 32)};
}

static inline U128 square_ext_manual(u64 x) noexcept
{
    const u64 x_low = x & 0xFFFFFFFF;
    const u64 x_high = x >> 32;
    const u64 t1 = x_low * x_low;
    const u64 t2 = x_low * x_high;
    const u64 t3 = x_high * x_high;
    const u64 t2_x2 = t2 << 1;
    const u64 t2_carry = t2 >> 63;
    const u64 mid = (t1 >> 32) + (t2_x2 & 0xFFFFFFFF);
    return {(t1 & 0xFFFFFFFF) | (mid << 32),
            t3 + (mid >> 32) + (t2_x2 >> 32) + (t2_carry << 32)};
}

}

namespace bignum::generic
{
template <typename U>
inline constexpr int countl_zero_generic(const U &val)
{
    if constexpr (requires { val.countl_zero(); })
    {
        // Если у типа есть метод (ваш U128/UBig)
        return val.countl_zero();
    }
    else
    {
        // Если это примитив (uint64_t, uint32_t)
        return std::countl_zero(val);
    }
}

/**
     * @brief Универсальное получение остатка от деления.
     */
template <typename T>
inline constexpr T get_rem(const T &a, const T &b)
{
    // 1. Если это наш U128, у которого есть эффективный оператор %
    if constexpr (std::is_same_v<std::decay_t<T>, bignum::u128::U128>)
    {
        return a % b;
    }
    // 2. Если это UBig, который возвращает пару {Q, R}
    else if constexpr (requires { (a / b).second; })
    {
        return (a / b).second;
    }
    // 3. Если это примитив (uint64_t)
    else
    {
        return a % b;
    }
}

/**
     * @brief Вычисляет 2^W / x.
     */
template <class U>
inline std::pair<U, U> reciprocal_and_extend(U x)
{
    // Нам нужно, чтобы U поддерживал countl_zero, <<=, - (унарный), / и %
    if (x == U{0})
        return {U{0}, U{0}}; // Или assert

    const auto x_old = x;
    // Использование универсальной функции
    const int i = countl_zero_generic(x);
    x <<= i;
    x = -x; // Теперь x = 2^W - (x_old << i)

    U Q, R;
    if (i > 0)
    {
        // ОПТИМИЗАЦИЯ: используем наш divide, чтобы не делить дважды
        // std::decay_t<U> сделает из "const U128&" просто "U128"
        if constexpr (std::is_same_v<std::decay_t<U>, bignum::u128::U128>)
        {
            // Здесь мы явно вызываем статический метод
            Q = bignum::u128::U128::divide<true, true>(x, x_old, &R);
        }
        else if constexpr (requires { (x / x_old).first; })
        {
            // Дополнительная ветка для UBig (если он возвращает pair)
            auto res = x / x_old;
            Q = res.first;
            R = res.second;
        }
        else
        {
            // Ветка для uint64_t и других примитивов
            Q = x / x_old;
            R = x % x_old;
        }
    }
    else
    {
        // Если i == 0, число x_old очень большое (старший бит 1)
        // x = -x_old эквивалентно (2^W - x_old)
        bool less = (x < x_old);
        Q = less ? U{0} : U{1};
        R = less ? x : U{0}; // Если x >= x_old при i=0, то x_old точно > 2^(W-1)
    }

    Q += (U{1} << i);
    return {Q, R};
}

/**
     * @brief r = (r + delta) mod m
     */
template <class U>
inline U smart_remainder_adder(U &r, const U &delta, const U &m, const U &r_rec)
{
    // Используем хелпер для безопасного получения остатка
    const U delta_m = get_rem(delta, m);
    const U summ = r + delta_m;

    const bool overflow = (summ < r);

    // Корректируем сумму
    U next_r = summ + (overflow ? r_rec : U{0});

    // Снова используем хелпер вместо r %= m
    r = get_rem(next_r, m);

    if (overflow)
        return U{1};
    return (summ >= m) ? U{1} : U{0};
}
}
