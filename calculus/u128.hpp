/**
 * @author nawww83@gmail.com
 * @brief Класс для арифметики 128-битных беззнаковых целых чисел.
 */

#pragma once

#include <cstdint>
#include <cassert>
#include <string>
#include <string_view>
#include <utility>
#include <algorithm>
#include <bit>
#include <compare>
#include <type_traits>

#if defined(_MSC_VER) && (_MSC_VER >= 1920) && defined(_M_X64)
#define USE_MSVC_INTRINSICS_DIVISION
#define USE_MSVC_INTRINSICS
#include <immintrin.h>
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
    constexpr bool operator==(const U128 &other) const noexcept
    {
        return mLow == other.mLow && mHigh == other.mHigh;
    }
    constexpr std::strong_ordering operator<=>(const U128 &other) const noexcept
    {
        if (auto cmp = mHigh <=> other.mHigh; cmp != 0)
            return cmp;
        return mLow <=> other.mLow;
    }

    // --- Логические проверки ---
    [[nodiscard]] constexpr explicit operator bool() const noexcept { return mLow != 0 || mHigh != 0; }
    [[nodiscard]] constexpr bool operator!() const noexcept { return mLow == 0 && mHigh == 0; }

    // --- Сложение ---
    constexpr U128 &operator+=(const U128 &other) noexcept;
    constexpr U128 operator+(const U128 &other) const noexcept { return U128(*this) += other; }

    // --- Вычитание ---
    constexpr U128 &operator-=(const U128 &other) noexcept;
    constexpr U128 operator-(const U128 &other) const noexcept { return U128(*this) -= other; }

    [[nodiscard]] constexpr U128 operator-() const noexcept
    {
        U128 res = ~(*this);
        return ++res;
    }

    // --- Умножение ---
    static constexpr U128 mult_ext_manual(u64 x, u64 y) noexcept;
    static constexpr U128 mult_ext(u64 x, u64 y) noexcept;
    static constexpr U128 square_ext_manual(u64 x) noexcept;
    static constexpr U128 square_ext(u64 x) noexcept;

    constexpr U128 operator*(const U128 &other) const noexcept;
    constexpr U128 &operator*=(const U128 &other) noexcept { return *this = (*this * other); }

    // --- Деление и остаток ---
    // --- Деление и остаток (Объявления) ---
    template <bool Q, bool R>
    static constexpr U128 divide_manual(const U128 &dividend, const U128 &divisor, U128 *rem_out) noexcept;
    template <bool Q, bool R>
    static constexpr U128 divide(const U128 &dividend, const U128 &divisor, U128 *rem_out) noexcept;

    constexpr U128 operator/(const U128 &other) const noexcept { return divide<true, false>(*this, other, nullptr); }
    constexpr U128 operator%(const U128 &other) const noexcept
    {
        U128 rem;
        divide<false, true>(*this, other, &rem);
        return rem;
    }
    constexpr U128 &operator/=(const U128 &other) noexcept { return *this = (*this / other); }
    constexpr U128 &operator%=(const U128 &other) noexcept { return *this = (*this % other); }

    // --- Сдвиги ---
    constexpr U128 &operator<<=(u32 s) noexcept;
    constexpr U128 &operator>>=(u32 s) noexcept;
    constexpr U128 operator<<(u32 s) const noexcept { return U128(*this) <<= s; }
    constexpr U128 operator>>(u32 s) const noexcept { return U128(*this) >>= s; }

    // --- Инкремент / Декремент ---
    constexpr U128 &operator++() noexcept
    {
        if (++mLow == 0)
            ++mHigh;
        return *this;
    }
    constexpr U128 operator++(int) noexcept
    {
        U128 tmp(*this);
        ++(*this);
        return tmp;
    }
    constexpr U128 &operator--() noexcept
    {
        if (mLow-- == 0)
            --mHigh;
        return *this;
    }
    constexpr U128 operator--(int) noexcept
    {
        U128 tmp(*this);
        --(*this);
        return tmp;
    }

    // --- Логические (Побитовые) ---
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
    constexpr U128 operator&(const U128 &other) const noexcept { return U128(*this) &= other; }
    constexpr U128 operator|(const U128 &other) const noexcept { return U128(*this) |= other; }
    constexpr U128 operator^(const U128 &other) const noexcept { return U128(*this) ^= other; }

    // --- Математические функции ---
    [[nodiscard]] constexpr int countl_zero() const noexcept
    {
        if (mHigh != 0)
            return std::countl_zero(mHigh);
        return 64 + std::countl_zero(mLow);
    }
    [[nodiscard]] constexpr int countr_zero() const noexcept
    {
        if (mLow != 0)
            return std::countr_zero(mLow);
        return 64 + std::countr_zero(mHigh);
    }
    [[nodiscard]] constexpr int popcount() const noexcept
    {
        return std::popcount(mHigh) + std::popcount(mLow);
    }
    [[nodiscard]] constexpr u32 bit_width() const noexcept { return 128 - countl_zero(); }

    static constexpr U128 max() noexcept { return {~0ull, ~0ull}; }
    static constexpr U128 min() noexcept { return {0, 0}; }

    // --- Конвертация ---
    static constexpr U128 fromString(std::string_view s);
    [[nodiscard]] std::string toString() const;

#if defined(__SIZEOF_INT128__)
    constexpr unsigned __int128 to_u128() const noexcept
    {
        return (static_cast<unsigned __int128>(mHigh) << 64) | mLow;
    }
#endif
};

// --- Реализации вне класса ---

inline constexpr U128 &U128::operator+=(const U128 &other) noexcept
{
#if defined(__SIZEOF_INT128__)
    if (!std::is_constant_evaluated())
    {
        unsigned __int128 val = to_u128() + other.to_u128();
        mLow = (u64)val;
        mHigh = (u64)(val >> 64);
        return *this;
    }
#endif
    u64 old_low = mLow;
    mLow += other.mLow;
    mHigh += other.mHigh + (mLow < old_low ? 1ull : 0ull);
    return *this;
}

inline constexpr U128 &U128::operator-=(const U128 &other) noexcept
{
#if defined(__SIZEOF_INT128__)
    if (!std::is_constant_evaluated())
    {
        unsigned __int128 val = to_u128() - other.to_u128();
        mLow = (u64)val;
        mHigh = (u64)(val >> 64);
        return *this;
    }
#endif
    u64 old_low = mLow;
    mLow -= other.mLow;
    mHigh -= (other.mHigh + (old_low < other.mLow ? 1ull : 0ull));
    return *this;
}

inline constexpr U128 U128::mult_ext_manual(u64 x, u64 y) noexcept
{
    const u64 x_low = x & 0xFFFFFFFF, x_high = x >> 32;
    const u64 y_low = y & 0xFFFFFFFF, y_high = y >> 32;
    const u64 t1 = x_low * y_low, t21 = x_low * y_high, t22 = x_high * y_low, t3 = x_high * y_high;
    const u64 mid = (t1 >> 32) + (t21 & 0xFFFFFFFF) + (t22 & 0xFFFFFFFF);
    return {(t1 & 0xFFFFFFFF) | (mid << 32), t3 + (t21 >> 32) + (t22 >> 32) + (mid >> 32)};
}

inline constexpr U128 U128::mult_ext(u64 x, u64 y) noexcept
{
#if defined(__SIZEOF_INT128__)
    if (!std::is_constant_evaluated())
    {
        unsigned __int128 res = static_cast<unsigned __int128>(x) * y;
        return {(u64)res, (u64)(res >> 64)};
    }
#elif defined(USE_MSVC_INTRINSICS)
    if (!std::is_constant_evaluated())
    {
        u64 hi, lo = _umul128(x, y, &hi);
        return {lo, hi};
    }
#endif
    return mult_ext_manual(x, y);
}

inline constexpr U128 U128::square_ext_manual(u64 x) noexcept
{
    const u64 x_low = x & 0xFFFFFFFF, x_high = x >> 32;
    const u64 t1 = x_low * x_low, t2 = x_low * x_high, t3 = x_high * x_high;
    const u64 t2_x2 = t2 << 1, t2_carry = t2 >> 63;
    const u64 mid = (t1 >> 32) + (t2_x2 & 0xFFFFFFFF);
    return {(t1 & 0xFFFFFFFF) | (mid << 32), t3 + (mid >> 32) + (t2_x2 >> 32) + (t2_carry << 32)};
}

inline constexpr U128 U128::square_ext(u64 x) noexcept
{
#if defined(USE_MSVC_INTRINSICS)
    if (!std::is_constant_evaluated())
    {
        u64 hi, lo = _umul128(x, x, &hi);
        return {lo, hi};
    }
#endif
    return square_ext_manual(x);
}

inline constexpr U128 U128::operator*(const U128 &other) const noexcept
{
#if defined(__SIZEOF_INT128__)
    if (!std::is_constant_evaluated())
    {
        auto res = to_u128() * other.to_u128();
        return {(u64)res, (u64)(res >> 64)};
    }
#endif
    U128 res = mult_ext(mLow, other.mLow);
    res.mHigh += (mLow * other.mHigh) + (mHigh * other.mLow);
    return res;
}

inline constexpr U128 &U128::operator<<=(u32 s) noexcept
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

inline constexpr U128 &U128::operator>>=(u32 s) noexcept
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

// Низкоуровневое деление 128/64 для эмуляции
inline constexpr u64 div_internal(u64 h, u64 l, u64 d, u64 *r) noexcept
{
#if defined(USE_MSVC_INTRINSICS)
    if (!std::is_constant_evaluated())
        return _udiv128(h, l, d, r);
#endif
    u64 rem = h;
    u64 qh = ((rem << 32) | (l >> 32)) / d;
    rem = ((rem << 32) | (l >> 32)) % d;
    u64 ql = ((rem << 32) | (l & 0xFFFFFFFF)) / d;
    if (r)
        *r = ((rem << 32) | (l & 0xFFFFFFFF)) % d;
    return (qh << 32) | ql;
}

template <bool Q, bool R>
inline constexpr U128 U128::divide_manual(const U128 &dividend, const U128 &divisor, U128 *rem_out) noexcept
{
    if (divisor == U128(0))
        return {0, 0};
    if (divisor.mHigh == 0)
    {
        u64 r0, r1, q1 = div_internal(0, dividend.mHigh, divisor.mLow, &r0), q0 = div_internal(r0, dividend.mLow, divisor.mLow, &r1);
        if constexpr (R)
            if (rem_out)
                *rem_out = {r1, 0};
        return Q ? U128(q0, q1) : U128(0);
    }
    u32 s = std::countl_zero(divisor.mHigh);
    U128 v = divisor << s, u = dividend;
    u64 h_in = (s == 0) ? 0 : (u.mHigh >> (64 - s)), rt;
    u64 qe = div_internal(h_in, (u << s).mHigh, v.mHigh, &rt);
    U128 qr{qe, 0}, prod = qr * divisor;
    while (prod > dividend)
    {
        prod -= divisor;
        qr.mLow--;
    }
    while ((dividend - prod) >= divisor)
    {
        prod += divisor;
        qr.mLow++;
    }
    if constexpr (R)
        if (rem_out)
            *rem_out = dividend - prod;
    return Q ? qr : U128(0);
}

template <bool Q, bool R>
inline constexpr U128 U128::divide(const U128 &dividend, const U128 &divisor, U128 *rem_out) noexcept
{
#if defined(__SIZEOF_INT128__)
    if (!std::is_constant_evaluated())
    {
        auto a = dividend.to_u128(), b = divisor.to_u128();
        if (b == 0)
            return {0, 0};
        if constexpr (R)
            if (rem_out)
            {
                auto r = a % b;
                *rem_out = {(u64)r, (u64)(r >> 64)};
            }
        return Q ? U128((u64)(a / b), (u64)((a / b) >> 64)) : U128(0);
    }
#endif
    return divide_manual<Q, R>(dividend, divisor, rem_out);
}

inline constexpr U128 U128::fromString(std::string_view s)
{
    size_t pos = 0;
    while (pos < s.length() && (s[pos] == ' ' || s[pos] == '+'))
        pos++;
    U128 res{0, 0};
    while (pos < s.length())
    {
        u64 block = 0, mult = 1;
        size_t end = (pos + 18 < s.length()) ? pos + 18 : s.length();
        for (; pos < end; ++pos)
        {
            if (s[pos] < '0' || s[pos] > '9')
                break;
            block = block * 10 + (s[pos] - '0');
            mult *= 10;
        }
        res = (mult_ext(res.mLow, mult) + U128(0, res.mHigh * mult)) + U128(block, 0);
        if (pos < s.length() && (s[pos] < '0' || s[pos] > '9'))
            break;
    }
    return res;
}

inline std::string U128::toString() const
{
    if (!(*this))
        return "0";

    U128 copy = *this;
    std::string s;
    s.reserve(40); // Оптимизация аллокаций

    while (copy > 0)
    {
        U128 next_q, rem_u;
        // Делим на 10^19
        next_q = divide<true, true>(copy, 10000000000000000000ULL, &rem_u);
        uint64_t r = rem_u.low();

        copy = next_q;

        // Если это не последний блок (есть еще цифры выше),
        // мы ОБЯЗАНЫ записать все 19 разрядов, включая нули.
        if (copy > 0)
        {
            for (int i = 0; i < 19; ++i)
            {
                s += (char)('0' + (r % 10));
                r /= 10;
            }
        }
        else
        {
            // Если это самый старший блок, пишем только значащие цифры
            do
            {
                s += (char)('0' + (r % 10));
                r /= 10;
            } while (r > 0);
        }
    }

    std::reverse(s.begin(), s.end());
    return s;
}

inline constexpr U128 operator""_u128(const char *str, std::size_t len) { return U128::fromString({str, len}); }
inline constexpr U128 operator""_u128(unsigned long long val) noexcept { return U128{val, 0}; }

inline std::ostream &operator<<(std::ostream &os, const bignum::u128::U128 &val)
{
    return os << val.toString();
}

} // namespace bignum::u128

namespace std {
template<>
struct hash<bignum::u128::U128> {
    size_t operator()(const bignum::u128::U128& v) const noexcept {
        // Комбинируем хеши двух 64-битных половин (алгоритм из Boost)
        size_t h1 = std::hash<uint64_t>{}(v.low());
        size_t h2 = std::hash<uint64_t>{}(v.high());
        return h1 ^ (h2 + 0x9e3779b9 + (h1 << 6) + (h1 >> 2));
    }
};

template<>
class numeric_limits<bignum::u128::U128> {
public:
    static constexpr bool is_specialized = true;
    static constexpr bool is_signed = false;
    static constexpr bool is_integer = true;
    static constexpr bool is_exact = true;
    static constexpr bool has_infinity = false;

    static constexpr bignum::u128::U128 min() noexcept { return {0, 0}; }
    static constexpr bignum::u128::U128 max() noexcept { return bignum::u128::U128::max(); }
    static constexpr bignum::u128::U128 lowest() noexcept { return min(); }

    static constexpr int digits = 128;
    static constexpr int digits10 = 38; // 10^38 < 2^128
};
}

namespace bignum::generic
{
template<typename T>
concept IsBigInt = std::is_integral_v<T> || requires(T a) {
    { a.low() };
    { a.high() };
};

template<typename T>
inline constexpr size_t bit_size() {
    if constexpr (std::is_integral_v<T>) return sizeof(T) * 8;
    if constexpr (requires(T a) { a.low(); }) {
        using LowT = std::decay_t<decltype(std::declval<T>().low())>;
        return bit_size<LowT>() * 2;
    }
    return 0;
}

template <typename T>
inline constexpr int countl_zero_generic(const T &val) {
    if constexpr (std::is_integral_v<T>) {
        return std::countl_zero(val);
    }
    // Добавляем проверку на наличие метода .countl_zero() (для U128)
    else if constexpr (requires { val.countl_zero(); }) {
        return val.countl_zero();
    }
    // Рекурсивный обход иерархии (для UBig)
    else if constexpr (requires { val.high(); val.low(); }) {
        using HighT = std::decay_t<decltype(val.high())>;
        int high_bits = (int)bit_size<HighT>();
        int hz = countl_zero_generic(val.high());
        if (hz < high_bits) return hz;
        return high_bits + countl_zero_generic(val.low());
    }
    return 0; // Заглушка для constexpr
}

template <typename T>
inline constexpr std::pair<T, T> div_rem(const T& a, const T& b) {
    // Случай А: Возвращает пару (UBig)
    if constexpr (requires { { a / b } -> std::same_as<std::pair<T, T>>; }) {
        return a / b;
    }
    // Случай Б: Статический метод (U128)
    // Используем явную проверку существования статического метода
    else if constexpr (requires { T::template divide<true, true>(a, b, (T*)nullptr); }) {
        T r;
        T q = T::template divide<true, true>(a, b, &r);
        return {q, r};
    }
    // Случай В: Примитивы (u64)
    else {
        return { a / b, a % b };
    }
}

template <typename T>
inline constexpr T get_rem_generic(const T &a, const T &b) {
    if constexpr (std::is_integral_v<T>) return a % b;
    else if constexpr (requires { a % b; }) return a % b;
    else return div_rem(a, b).second;
}

template <IsBigInt U>
inline std::pair<U, U> reciprocal_and_extend(U x) {
    if (x == U{0}) return {U{0}, U{0}};
    const auto x_old = x;
    const int i = countl_zero_generic(x);
    x <<= i;
    x = -x;

    U Q, R;
    if (i > 0) {
        auto [q_res, r_res] = div_rem(x, x_old);
        Q = q_res; R = r_res;
    } else {
        bool less = (x < x_old);
        Q = less ? U{0} : U{1};
        R = less ? x : U{0};
    }
    Q += (U{1} << i);
    return {Q, R};
}

template <IsBigInt U>
inline U smart_remainder_adder(U &r, const U &delta, const U &m, const U &r_rec) {
    const U delta_m = get_rem_generic(delta, m);
    const U summ = r + delta_m;
    const bool overflow = (summ < r);
    U next_r = summ + (overflow ? r_rec : U{0});
    r = get_rem_generic(next_r, m);
    return overflow ? U{1} : ((summ >= m) ? U{1} : U{0});
}
} // namespace bignum::generic
