#pragma once

#include <algorithm> // std::reverse
#include <atomic>
#include <chrono>
#include <map>       // std::map
#include <random>
#include <vector>       // std::vector
#include <cassert> // assert
#include <string>    // std::string
#include <utility>   // std::pair
#include <functional> // std::function
#include <tuple> // std::ignore, std::tie
#include "random_gen.h"

namespace u128 {

using ULOW = uint64_t; // Тип половинок: старшей и младшей частей составного числа.

static constexpr auto INF = "inf";

static_assert(CHAR_BIT == 8);

struct Quadrupole { // Структура для задания дроби (A*M + B) / (C*M + D).
    // M - множитель системы счисления, 2^W, W = 64 - битовая ширина половинок.
    ULOW A;
    ULOW B;
    ULOW C;
    ULOW D;
};

struct Signess { // Структура для задания знаков двух чисел.
    bool s1;
    bool s2;
};

struct Dipole { // Структура для задания числа (A*M + B).
    // M - множитель системы счисления, 2^W, W = 64 - битовая ширина половинок.
    ULOW A;
    ULOW B;
};

static constexpr char DIGITS[10]{'0', '1', '2', '3', '4',
                                 '5', '6', '7', '8', '9'};


struct Singular {
    int mOverflow = 0;
    int mNaN = 0;
    constexpr explicit Singular() = default;
    constexpr Singular(bool is_overflow, bool is_nan):
        mOverflow{is_overflow}
        , mNaN{is_nan} {};
    constexpr Singular(const Singular& other) = default;
    constexpr Singular(Singular&& other) = default;
    constexpr Singular& operator=(const Singular& other) = default;
    constexpr Singular& operator=(Singular&& other) = default;
    bool operator()() const {return mNaN != 0 || mOverflow != 0;}
    bool operator==(const Singular& other) const {
        return !(mOverflow || other.mOverflow || mNaN || other.mNaN);
    }
    auto operator<=>(const Singular& other) const = default;
    bool IsOverflow() const {return mOverflow != 0;}
    bool IsNaN() const {return mNaN != 0;}
};

struct Sign {
    int mSign = 0;
    constexpr explicit Sign() = default;
    constexpr Sign(const Sign& other) = default;
    constexpr Sign(Sign&& other) = default;
    constexpr Sign(bool value): mSign {value}{};
    constexpr Sign& operator=(const Sign& other) = default;
    constexpr Sign& operator=(Sign&& other) = default;
    Sign& operator^(const Sign& other) {
        this->mSign = operator()() ^ other.operator()();
        return *this;
    }
    bool operator()() const {return mSign != 0;}
    void operator-() {
        mSign = 1 - operator()();
    }
    bool operator==(const Sign& other) const {
        return mSign == other.mSign ? true : ((operator()() && other.operator()()) || (!operator()() && !other.operator()()));
    }
    auto operator<=>(const Sign& other) = delete;
};

class Globals {
    static struct _gu128 {
        /**
    * @brief Признак остановки расчетов. Для потенциально долгих операций.
    */
        std::atomic<bool> is_stop = false;
    } global_u128;
public:
    explicit Globals() = default;
    static void SetStop(bool value) {
        global_u128.is_stop.store(value);
    }
    static bool LoadStop() {
        return global_u128.is_stop.load(std::memory_order::relaxed);
    }
};

static auto get_random_u32x4(int64_t offset) {
    const int64_t since_epoch_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>
        (std::chrono::system_clock::now().time_since_epoch()).count();
    std::seed_seq g_rnd_sequence{since_epoch_ms & 255, (since_epoch_ms >> 8) & 255,
                                 (since_epoch_ms >> 16) & 255, (since_epoch_ms >> 24) & 255, offset};
    lfsr8::u32x4 st;
    g_rnd_sequence.generate(st.begin(), st.end());
    return st;
}

struct RandomGenerator {
    explicit RandomGenerator() {
        lfsr8::u64 tmp;
        mGenerator.seed(get_random_u32x4((lfsr8::u64)&tmp));
    }
    lfsr_rng_2::gens mGenerator;
};

struct U128;

U128 shl64(U128 x);
U128 get_zero();
U128 get_unit();
U128 get_unit_neg();

// High/Low структура 128-битного числа со знаком и флагом переполнения.
// Для иллюстрации алгоритма деления двух U128 чисел реализованы основные
// арифметические операторы, кроме умножения двух U128 чисел.
struct U128 {
    // Битовая полуширина половинок.
    static constexpr int mHalfWidth = (sizeof(ULOW) * CHAR_BIT) / 2;
    // Наибольшее значение половинок, M-1.
    static constexpr ULOW mMaxULOW = ULOW(-1);
    ULOW mLow = 0;
    ULOW mHigh = 0;
    Sign mSign{};
    Singular mSingular{};

    explicit constexpr U128() = default;

    explicit constexpr U128(ULOW low, ULOW high, Sign sign = false)
        : mLow{low}
        , mHigh {high}
        , mSign {sign} {};

    constexpr U128(const U128& other) = default;

    constexpr U128(U128&& other) = default;

    constexpr U128& operator=(const U128& other) = default;

    bool operator==(const U128& other) const {
        const auto has_singular = mSingular != other.mSingular;
        return has_singular ? false : *this<=>other == 0;
    }

    std::partial_ordering operator<=>(const U128& other) const {
        const auto has_singular = mSingular != other.mSingular;
        if (has_singular) {
            return std::partial_ordering::unordered;
        }
        const bool equal_signs = mSign() == other.mSign();
        if (equal_signs) {
            auto high_cmp = mSign() ? other.mHigh<=>mHigh : mHigh<=>other.mHigh;
            if (high_cmp != 0) {
                return high_cmp;
            }
            return mSign() ? other.mLow<=>mLow : mLow<=>other.mLow;
        } else {
            if (mSign()) {
                auto high_cmp = mHigh<=>other.mHigh;
                if (high_cmp != 0) {
                    return (high_cmp > 0) ? other.mHigh<=>mHigh : high_cmp;
                }
                auto low_cmp = mLow<=>other.mLow;
                if (low_cmp != 0) {
                    return (low_cmp > 0) ? other.mLow<=>mLow : low_cmp;
                } else {
                    return mLow != 0 || mHigh != 0 ? -1<=>1 : 1<=>1;
                }
            } else {
                auto high_cmp = mHigh<=>other.mHigh;
                if (high_cmp != 0) {
                    return (high_cmp < 0) ? other.mHigh<=>mHigh : high_cmp;
                }
                auto low_cmp = mLow<=>other.mLow;
                if (low_cmp != 0) {
                    return (low_cmp < 0) ? other.mLow<=>mLow : low_cmp;
                } else {
                    return mLow != 0 || mHigh != 0 ? 1<=>-1 : 1<=>1;
                }
            }
        }
    }

    bool is_singular() const {
        return mSingular();
    }

    bool is_overflow() const {
        return mSingular.IsOverflow() && !mSingular.IsNaN();
    }

    bool is_nan() const {
        return mSingular.IsNaN() && !mSingular.IsOverflow();
    }

    bool is_zero() const {
        return mLow == 0 && mHigh == 0 && !is_singular();
    }

    bool is_negative() const {
        return !is_zero() && mSign() && !is_singular();
    }

    bool is_positive() const {
        return !is_zero() && !mSign() && !is_singular();
    }

    bool is_nonegative() const {
        return is_positive() || is_zero();
    }

    void set_overflow() {
        mSingular.mOverflow = 1;
        mSingular.mNaN = 0;
    }

    void set_nan() {
        mSingular.mOverflow = 0;
        mSingular.mNaN = 1;
    }

    U128 operator-() const {
        U128 result = *this;
        -result.mSign;
        return result;
    }

    U128 abs() const {
        U128 result = *this;
        result.mSign = false;
        return result;
    }

    U128 operator+(U128 rhs) const {
        U128 result{};
        U128 X = *this;
        if (X.is_singular()) {
            return X;
        }
        if (rhs.is_singular()) {
            X.mSingular = rhs.mSingular;
            return X;
        }
        if (X.is_negative() && !rhs.is_negative()) {
            X.mSign = false;
            result = rhs - X;
            return result;
        }
        if (!X.is_negative() && rhs.is_negative()) {
            rhs.mSign = false;
            result = X - rhs;
            return result;
        }
        result.mLow = X.mLow + rhs.mLow;
        const ULOW c1 = result.mLow < std::min(X.mLow, rhs.mLow);
        result.mHigh = X.mHigh + rhs.mHigh;
        const int c2 = result.mHigh < std::min(X.mHigh, rhs.mHigh);
        ULOW tmp = result.mHigh;
        result.mHigh = tmp + c1;
        const int c3 = result.mHigh < std::min(tmp, c1);
        result.mSingular.mOverflow = c2 || c3;
        if (X.mSign() && rhs.mSign()) {
            result.mSign = true;
        }
        return result;
    }

    U128& operator+=(U128 other) {
        *this = *this + other;
        return *this;
    }

    U128 operator-(U128 rhs) const {
        U128 result{};
        U128 X = *this;
        if (X.is_singular()) {
            return X;
        }
        if (rhs.is_singular()) {
            X.mSingular = rhs.mSingular;
            return X;
        }
        if (X.is_negative() && !rhs.is_negative()) {
            rhs.mSign = 1;
            result = rhs + X;
            return result;
        }
        if (!X.is_negative() && rhs.is_negative()) {
            rhs.mSign = false;
            result = X + rhs;
            return result;
        }
        if (X.is_negative() && rhs.is_negative()) {
            rhs.mSign = false;
            X.mSign = false;
            result = rhs - X;
            return result;
        }
        if (X.is_zero()) {
            result = rhs;
            -result.mSign;
            return result;
        }
        result.mLow = X.mLow - rhs.mLow;
        result.mHigh = X.mHigh - rhs.mHigh;
        const bool borrow = X.mLow < rhs.mLow;
        const bool hasUnit = X.mHigh > rhs.mHigh;
        if (borrow && hasUnit) {
            result.mHigh -= ULOW(1);
        }
        if (borrow && !hasUnit) {
            result = rhs - X;
            -result.mSign;
            return result;
        }
        if (!borrow && X.mHigh < rhs.mHigh) {
            result.mHigh = -result.mHigh - ULOW(result.mLow != 0);
            result.mLow = -result.mLow;
            result.mSign = true;
        }
        return result;
    }

    U128& operator-=(U128 other) {
        *this = *this - other;
        return *this;
    }

    /**
     * @brief Инкремент числа.
     * @return Число + 1.
     */
    U128& inc() {
        *this = *this + get_unit();
        return *this;
    }

    /**
     * @brief Декремент числа.
     * @return Число - 1.
     */
    U128& dec() {
        *this = *this - get_unit();
        return *this;
    }

    U128 mult64(ULOW x, ULOW y) const {
        constexpr ULOW MASK = (ULOW(1) << mHalfWidth) - 1;
        const ULOW x_low = x & MASK;
        const ULOW y_low = y & MASK;
        const ULOW x_high = x >> mHalfWidth;
        const ULOW y_high = y >> mHalfWidth;
        const ULOW t1 = x_low * y_low;
        const ULOW t = t1 >> mHalfWidth;
        const ULOW t21 = x_low * y_high;
        const ULOW q = t21 >> mHalfWidth;
        const ULOW p = t21 & MASK;
        const ULOW t22 = x_high * y_low;
        const ULOW s = t22 >> mHalfWidth;
        const ULOW r = t22 & MASK;
        const ULOW t3 = x_high * y_high;
        U128 result{t1, 0};
        const ULOW div = (q + s) + ((p + r + t) >> mHalfWidth);
        const auto p1 = t21 << mHalfWidth;
        const auto p2 = t22 << mHalfWidth;
        const ULOW mod = p1 + p2;
        result.mLow += mod;
        result.mHigh += div;
        result.mHigh += t3;
        result.mSingular.mOverflow = result.mHigh < t3;
        return result;
    }

    U128 operator*(ULOW rhs) const {
        U128 result = mult64(mLow, rhs);
        U128 tmp = mult64(mHigh, rhs);
        const bool is_overflow = tmp.mHigh != 0;
        tmp.mHigh = tmp.mLow;
        tmp.mLow = 0;
        result += tmp;
        result.mSign = !result.is_zero() ? this->mSign() : false;
        if (is_overflow)
            result.set_overflow();
        return result;
    }

    U128 operator*(U128 rhs) const {
        const U128 X = *this;
        if (X.is_overflow() || rhs.is_overflow()) {
            U128 result;
            result.set_overflow();
            return result;
        }
        if (X.is_nan() || rhs.is_nan()) {
            U128 result;
            result.set_nan();
            return result;
        }
        U128 result = X * rhs.mLow;
        if (result.is_singular()) {
            return result;
        }
        result.mSign = this->mSign() ^ rhs.mSign();
        const auto tmp = X * rhs.mHigh;
        result = result + shl64(tmp);
        return result;
    }

    U128 div10() const { // Специальный метод деления на 10 для формирования
        // строкового представления числа.
        U128 X = *this;
        if (X.is_singular()) {
            return X;
        }
        const bool sign = X.mSign();
        X.mSign = false;
        ULOW Q = X.mHigh / ULOW(10);
        ULOW R = X.mHigh % ULOW(10);
        ULOW N = R * (mMaxULOW / ULOW(10)) + (X.mLow / ULOW(10));
        U128 result{}; result.mHigh = Q; result.mLow = N;
        const U128 tmp = result * ULOW(10);
        U128 E = X - tmp;
        while (E.mHigh != 0 || E.mLow >= ULOW(10)) {
            Q = E.mHigh / ULOW(10);
            R = E.mHigh % ULOW(10);
            N = R * (mMaxULOW / ULOW(10)) + (E.mLow / ULOW(10));
            U128 tmp {N, Q};
            result += tmp;
            E -= tmp * ULOW(10);
        }
        result.mSign = sign;
        return result;
    }

    int mod10() const { // Специальный метод нахождения остатка от деления на 10 для формирования
        // строкового представления числа.
        if (this->is_singular()) {
            return -1;
        }
        constexpr int multiplier_mod10 = mMaxULOW % 10 + 1;
        return ((mLow % 10) + multiplier_mod10 * (mHigh % 10)) % 10;
    }

    // Метод итеративного деления широкого числа на узкое.
    // Наиболее вероятное количество итераций: ~N/4, где N - количество битов узкого числа.
    // В данном случае имеем ~64/4 = 16 итераций.
    // Максимум до ~(N+1) итерации.
    std::pair<U128, U128> operator/(ULOW y) const {
        assert(y != 0);
        const U128 X = *this;
        if (X.is_singular()) {
            return std::make_pair(X, u128::get_zero());
        }
        ULOW Q = X.mHigh / y;
        ULOW R = X.mHigh % y;
        ULOW N = R * (mMaxULOW / y) + (X.mLow / y);
        U128 result{N, Q, X.mSign};
        U128 E = X - result * y; // Остаток от деления.
        for (;;) {
            Q = E.mHigh / y;
            R = E.mHigh % y;
            N = R * (mMaxULOW / y) + (E.mLow / y);
            U128 tmp{N, Q, E.mSign};
            if (tmp.is_zero()) {
                break;
            }
            result += tmp;
            E -= tmp * y;
        }
        if (E.is_negative()) { // И при этом не равно нулю.
            result -= get_unit();
            U128 tmp {y, 0};
            E += tmp;
        }
        return std::make_pair(result, E);
    }

    std::pair<U128, U128> operator/=(ULOW y) {
        U128 remainder;
        std::tie(*this, remainder) = *this / y;
        return std::make_pair(*this, remainder);
    }

    // Метод деления двух широких чисел.
    // Отсутствует "раскачка" алгоритма для "плохих" случаев деления: (A*M + B)/(1*M + D).
    // Наиболее вероятное общее количество итераций: 4...6.
    std::pair<U128, U128> operator/(const U128 other) const {
        U128 X = *this;
        U128 Y = other;
        if (X.is_overflow() || Y.is_overflow()) {
            U128 result;
            result.set_overflow();
            return std::make_pair(result, u128::get_zero());
        }
        if (X.is_nan() || Y.is_nan()) {
            U128 result;
            result.set_nan();
            return std::make_pair(result, u128::get_zero());
        }
        if (Y.mHigh == 0) {
            X.mSign = X.mSign() ^ Y.mSign();
            auto result = X / Y.mLow;
            return result;
        }
        const bool make_sign_inverse = X.mSign != Y.mSign;
        X.mSign = make_sign_inverse;
        Y.mSign = 0;
        const ULOW Q = X.mHigh / Y.mHigh;
        const ULOW R = X.mHigh % Y.mHigh;
        const ULOW Delta = mMaxULOW - Y.mLow;
        const U128 DeltaQ = mult64(Delta, Q);
        U128 W1 = U128{0, R} - U128{0, Q};
        W1 = W1 + DeltaQ;
        const ULOW C1 = (Y.mHigh < mMaxULOW) ? Y.mHigh + ULOW(1) : mMaxULOW;
        const ULOW W2 = mMaxULOW - Delta / C1;
        auto [Quotient, _] = W1 / W2;
        std::tie(Quotient, std::ignore) = Quotient / C1;
        U128 result = U128{Q, 0} + Quotient;
        if (make_sign_inverse) {result = -result;}
        U128 N = Y * result.mLow;
        if (make_sign_inverse) {N = -N;}
        assert(!N.is_overflow());
        U128 Error = X - N;
        U128 More = Error - Y;
        bool do_inc = More.is_nonegative();
        bool do_dec = Error.is_negative();
        while (do_dec || do_inc) {
            result += (do_inc ? get_unit() : (do_dec ? get_unit_neg() : get_zero()));
            if (do_dec) {
                Error += Y;
            }
            if (do_inc) {
                Error -= Y;
            }
            More = Error - Y;
            do_inc = More.is_nonegative();
            do_dec = Error.is_negative();
        }
        return std::make_pair(result, Error);
    }

    /**
    * Возвращает строковое представление числа.
    */
    std::string value() const {
        std::string result{};
        if (this->is_overflow()) {
            result = INF;
            return result;
        }
        if (this->is_nan()) {
            result = "";
            return result;
        }
        U128 X = *this;
        while (!X.is_zero()) {
            const int d = X.mod10();
            if (d < 0) {
                return result;
            }
            result.push_back(DIGITS[d]);
            X = X.div10();
        }
        if (this->is_negative() && !this->is_zero()) {
            result.push_back('-');
        }
        std::reverse(result.begin(), result.end());
        return result.length() != 0 ? result : "0";
    }

}; // struct U128

inline U128 get_zero() {
    return U128 {0, 0};
}

inline U128 get_unit() {
    U128 result{};
    result.mLow = 1;
    result.mHigh = 0;
    return result;
}

inline U128 get_unit_neg() {
    U128 result{};
    result.mLow = 1;
    result.mHigh = 0;
    result.mSign = true;
    return result;
}

inline U128 get_max_value() {
    U128 result{};
    result.mLow = -1;
    result.mHigh = -1;
    return result;
}

inline U128 get_random_value() {
    U128 result;
    static RandomGenerator g_prng2;
    result.mLow = g_prng2.mGenerator.next_u64();
    result.mHigh = g_prng2.mGenerator.next_u64();
    g_prng2.mGenerator.next_u64();
    g_prng2.mGenerator.next_u64();
    return result;
}

inline U128 get_random_half_value() {
    U128 result;
    static RandomGenerator g_prng1;
    result.mLow = g_prng1.mGenerator.next_u64();
    result.mHigh = 0;
    g_prng1.mGenerator.next_u64();
    return result;
}

inline U128 int_power(ULOW x, int y) {
    u128::U128 result = get_unit();
    for (int i = 1; i <= y; ++i) {
        result = result * x;
    }
    return result;
}

inline U128 shl64(U128 x) { // x * 2^64
    U128 result {0, x.mLow, x.mSign};
    result.mSingular = x.mSingular;
    if (x.mHigh != 0 && !x.is_singular()) {
        result.set_overflow();
    }
    return result;
}

/**
 * @brief Количество цифр числа.
 * @param x Число.
 * @return Количество цифр, минимум 1.
 */
inline int num_of_digits(U128 x) {
    int i = 0;
    while (!x.is_zero()) {
        x = x.div10();
        i++;
    }
    return i + (i == 0);
}

/**
 * Целочисленный квадратный корень.
 * @param exact Точно ли прошло извлечение корня.
 */
inline U128 isqrt(U128 x, bool& exact) {
    exact = false;
    if (x.is_singular()) {
        return x;
    }
    const U128 c {ULOW(1) << U128::mHalfWidth, 0};
    U128 result;
    x = x.abs();
    if (x >= U128{0, 1}) {
        result = c;
    } else {
        result = U128 {ULOW(1) << (U128::mHalfWidth / 2), 0};
    }
    U128 prevprev = get_unit_neg();
    U128 prev = x;
    for (;;) {
        prevprev = prev;
        prev = result;
        const auto [tmp, remainder] = x / result;
        std::tie(result, std::ignore) = (result + tmp) / 2;
        if (result.is_zero()) {
            exact = true;
            return result;
        }
        if (result == prev) {
            exact = (tmp == prev) && remainder.is_zero(); // Нет остатка от деления.
            return result;
        }
        if (result == prevprev) {
            return prev;
        }
    }
}

/**
 * @brief Является ли заданное число квадратичным вычетом.
 * @param x Тестируемое число.
 * @param p Простой модуль.
 * @return Да/нет.
 */
inline bool is_quadratiq_residue(U128 x, U128 p) {
    // y^2 = x mod p
    auto [_, r1] = x / p;
    for(U128 y = u128::get_zero(); y < p ; y.inc()) {
        U128 sq = y*y;
        auto [_, r2] = sq / p;
        if (r2 == r1)
            return true;
    }
    return false;
}

/**
 * @brief Возвращает корень квадратный из заданного числа
 * по заданному модулю.
 * @param x Число.
 * @param p Простой модуль.
 * @return Два значения корня.
 */
inline std::pair<U128, U128> sqrt_mod(U128 x, U128 p) {
    // return  sqrt(x) mod p
    U128 result[2];
    int idx = 0;
    const auto [_, r1] = x / p;
    for(U128 y = u128::get_zero(); y < p ; y.inc()) {
        U128 sq = y*y;
        auto [_, r2] = sq / p;
        if (r2 == r1)
            result[idx++] = y;
    }
    return std::make_pair(result[0], result[1]);
}

inline bool is_prime(U128 x) {
    [[maybe_unused]] bool exact;
    const auto x_sqrt = isqrt(x, exact) + u128::get_unit();
    U128 d {2, 0};
    bool is_ok = true;
    while (d < x_sqrt) {
        auto [_, remainder] = x / d;
        is_ok &= !remainder.is_zero();
        d.inc();
    }
    return is_ok;
}

/**
 * @brief Делит первое число на второе до "упора".
 * @param x Делимое.
 * @param q Делитель.
 * @return Пара {Делитель, количество успешных делений}
 */
inline std::pair<U128, int> div_by_q(U128& x, ULOW q) {
    auto [tmp, remainder] = x / q;
    int i = 0;
    while (remainder.is_zero()) {
        i++;
        x = tmp;
        std::tie(tmp, remainder) = x / q;
    }
    return std::make_pair(U128{q, 0}, i);
}

inline std::pair<U128, U128> ferma_method(U128 x) {
    U128 x_sqrt;
    {
        bool is_exact;
        x_sqrt = isqrt(x, is_exact);
        if (is_exact)
            return std::make_pair(x_sqrt, x_sqrt);
    }
    const auto error = x - x_sqrt * x_sqrt;
    auto y = U128{2, 0} * x_sqrt + u128::get_unit() - error;
    {
        bool is_exact;
        auto y_sqrt = isqrt(y, is_exact);
        const auto delta = x_sqrt + x_sqrt + U128{3, 0};
        y = y + delta;
        if (is_exact)
            return std::make_pair(x_sqrt + u128::get_unit() - y_sqrt, x_sqrt + u128::get_unit() + y_sqrt);
    }
    auto [k_upper, _] = x_sqrt/2; // Точный коэффициент sqrt(2) - 1 = 0,414... < 0.5.
    for ( auto k = U128{2, 0};; k.inc()) {
        if (!(k.mLow & 65535) && Globals::LoadStop() ) // Проверка на стоп через каждые 65536 отсчетов.
            break;
        if (k > k_upper) {
            return std::make_pair(x, u128::get_unit()); // x - простое число.
        }
        if (k.mLow % 2) { // Проверка с другой стороны: ускоряет поиск.
            // Основано на равенстве, следующем из метода Ферма: индекс k = (F^2 + x) / (2F) - floor(sqrt(x)).
            // Здесь F - кандидат в множители, x - раскладываемое число.
            const auto N1 = k * k + x;
            if ((N1.mLow % 2) == 0) {
                auto [q1, remainder] = N1 / (k + k); // Здесь k как некоторый множитель F.
                if (remainder.is_zero() && (q1 > x_sqrt)) {
                    auto [q2, remainder] = x / k;
                    if (remainder.is_zero()) // На всякий случай оставим.
                        return std::make_pair(k, q2);
                }
            }
        }
        if (auto r = y.mod10(); (r != 1 && r != 9)) // Просеиваем заведомо лишние.
            continue;
        bool is_exact;
        const auto y_sqrt = isqrt(y, is_exact);
        const auto delta = (x_sqrt + x_sqrt) + (k + k) + u128::get_unit();
        y = y + delta;
        if (!is_exact)
            continue;
        const auto first_multiplier = x_sqrt + k - y_sqrt;
        return std::make_pair(first_multiplier, x_sqrt + k + y_sqrt);
    }
    return std::make_pair(x, u128::get_unit()); // По какой-то причине не раскладывается.
};

inline std::map<U128, int> factor(U128 x) {
    Globals::SetStop(false);
    if (x.is_zero()) {
        return {{x, 1}};
    }
    if (x == u128::get_unit()) {
        return {{x, 1}};
    }
    if (x.is_singular()) {
        return {{x, 1}};
    }
    x = x.abs();
    std::map<U128, int> result{};
    { // Обязательное для метода Ферма деление на 2.
        auto [p, i] = div_by_q(x, 2);
        if (i > 0)
            result[p] = i;
        if (x < U128{2, 0}) {
            return result;
        }
    }
    // Делим на простые из списка: опционально.
    for (const auto& el : std::vector{3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37, 41,
                                      43, 47, 53, 59, 61, 67, 71, 73, 79, 83, 89, 97, 101, 103, 107, 109, 113, 127,
                                      131, 137, 139, 149, 151, 157, 163, 167, 173, 179, 181, 191, 193, 197, 199, 211,
                                      223, 227, 229, 233, 239, 241, 251, 257, 263, 269, 271, 277, 281, 283, 293})
    {
        auto [p, i] = div_by_q(x, el);
        if (i > 0)
            result[p] = i;
        if (x < U128{2, 0}) {
            return result;
        }
    }
    // Применяем метод Ферма рекурсивно.
    std::function<void(U128)> ferma_recursive;
    ferma_recursive = [&ferma_recursive, &result](U128 x) -> void {
        auto [a, b] = ferma_method(x);
        if (a == U128{1, 0}) {
            result[b]++;
            return;
        }
        else if (b == U128{1, 0}) {
            result[a]++;
            return;
        }
        ferma_recursive(a);
        ferma_recursive(b);
    };
    ferma_recursive(x);
    Globals::SetStop(false);
    return result;
}

inline U128 get_by_digit(int digit) {
    return U128{static_cast<u128::ULOW>(digit), 0};
}

inline Globals::_gu128 Globals::global_u128;

} // namespace u128
