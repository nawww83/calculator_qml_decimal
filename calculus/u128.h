#pragma once

#include <algorithm> // std::reverse
#include <cassert>
#include <cstdint>
#include <climits>
#include <string> // std::string

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
        bool sign = operator()() ^ other.operator()();
        this->mSign = sign;
        return *this;
    }
    bool operator()() const {return mSign != 0;}
    void operator-() {
        mSign = 1 - (operator()() ? 1 : 0);
    }
    bool operator==(const Sign& other) const {
        return mSign == other.mSign ? true : ((operator()() && other.operator()()) || (!operator()() && !other.operator()()));
    }
    auto operator<=>(const Sign& other) = delete;
};

struct U128;

U128 shl64(U128 x);

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
    U128 operator/(ULOW y) const {
        assert(y != 0);
        const U128 X = *this;
        ULOW Q = X.mHigh / y;
        ULOW R = X.mHigh % y;
        ULOW N = R * (mMaxULOW / y) + (X.mLow / y);
        U128 result; result.mHigh = Q; result.mLow = N; result.mSign = X.mSign;
        U128 E = X - result * y; // Ошибка от деления: остаток от деления.
        while (1) {
            Q = E.mHigh / y;
            R = E.mHigh % y;
            N = R * (mMaxULOW / y) + (E.mLow / y);
            U128 tmp{N, Q};
            tmp.mSign = E.mSign;
            if (tmp.is_zero()) {
                break;
            }
            result += tmp;
            E -= tmp * y;
        }
        if (E.is_negative()) { // И при этом не равно нулю.
            result -= U128{1, 0};
            U128 tmp {y, 0};
            E += tmp;
        }
        return result;
    }

    U128& operator/=(ULOW y) {
        *this = *this / y;
        return *this;
    }

    // Метод деления двух широких чисел.
    // Отсутствует "раскачка" алгоритма для "плохих" случаев деления: (A*M + B)/(1*M + D).
    // Наиболее вероятное общее количество итераций: 4...6.
    U128 operator/(const U128 other) const {
        U128 X = *this;
        U128 Y = other;
        constexpr U128 ZERO {0, 0};
        constexpr U128 UNIT {1, 0};
        constexpr U128 UNIT_NEG {1, 0, true};
        if (Y.mHigh == 0) {
            X.mSign = X.mSign() ^ Y.mSign();
            U128 result = X / Y.mLow;
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
        U128 Quotient = W1 / W2;
        Quotient = Quotient / C1;
        U128 result = U128{Q, 0} + Quotient;
        if (make_sign_inverse) {-result.mSign;}
        U128 N = Y * result.mLow;
        if (make_sign_inverse) {-N.mSign;}
        assert(!N.is_overflow());
        U128 Error = X - N;
        U128 More = Error - Y;
        bool do_inc = More.is_positive();
        bool do_dec = Error.is_negative();
        while (do_dec || do_inc) {
            result += (do_inc ? UNIT : (do_dec ? UNIT_NEG : ZERO));
            if (do_dec) {
                Error += Y;
            }
            if (do_inc) {
                Error -= Y;
            }
            More = Error - Y;
            do_inc = More.is_positive();
            do_dec = Error.is_negative();
        }
        return result;
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


} // namespace u128
