#pragma once

#include <cmath>   // std::pow
#include <cassert> // assert
#include <array>   // std::array
#include <cstring> // std::memcpy
#include <string>  // std::string
#include <climits> // CHAR_BIT
#include <algorithm> // std::clamp

#include "u128.h"


namespace dec_n {

constexpr int undigits(char d) {
    switch (d) {
    case '0':
        return 0;
        break;
    case '1':
        return 1;
        break;
    case '2':
        return 2;
        break;
    case '3':
        return 3;
        break;
    case '4':
        return 4;
        break;
    case '5':
        return 5;
        break;
    case '6':
        return 6;
        break;
    case '7':
        return 7;
        break;
    case '8':
        return 8;
        break;
    case '9':
        return 9;
        break;
    default:
        return 0;
    }
}

inline u128::U128 int_power(u128::ULOW x, int y) {
    u128::U128 result {u128::get_unit()};
    for (int i = 1; i <= y; ++i) {
        result = result * x;
    }
    return result;
}

inline u128::U128 absolute_value(u128::U128 x) {
    u128::U128 result {x};
    result.mSign = false;
    return result;
}

/**
 * @brief Количество цифр числа.
 * @param x Число.
 * @return Количество цифр, минимум 1.
 */
inline int num_of_digits(u128::U128 x) {
    int i = 0;
    while (!x.is_zero()) {
        x = x.div10();
        i++;
    }
    return i + (i == 0);
}

/**
 * @brief Работает ли платформа с дополнительным кодом.
 * @return Да/нет.
 */
inline bool is_two_complement() {
    const unsigned long long w = static_cast<unsigned long long>(-1ll);
    std::byte b[8];
    std::memcpy(b, &w, sizeof(b));
    const auto ref = std::byte(255);
    bool all_is_0xff = true;
    for (int i=0; i<(int)sizeof(b); ++i) {
        all_is_0xff &= b[i] == ref;
    }
    const long long wi = static_cast<long long>(w);
    assert(wi == -1ll);
    return all_is_0xff && wi == -1ll;
}

namespace chars {
    static const char minus_sign = '-';
    static const char separator = ',';
    static const char alternative_separator = '.';
    static const char null = '\0';
    static const char zero = '0';
}

/**
 * @brief Класс для хранения строкового представления Decimal числа,
 * основанного на 128-битных целой и дробной частей.
 */
class Vector128 {
    /**
     * @brief Наибольшее количество хранимых символов.
     */
    static constexpr int MAX_SIZE = 80;

    /**
     * @brief Буфер символов строкового представления числа.
     */
    std::array<char, MAX_SIZE + 1> mBuffer{};

    /**
     * @brief Значимый размер буфера.
     */
    int mRealSize = 0;

    /**
     * @brief Приводит размер к отрезку [0, MAX_SIZE].
     * @param size Входной размер.
     * @return Ограниченный отрезком размер.
     */
    int BoundSize(int size) const {
        return std::clamp(size, 0, MAX_SIZE);
    }

    /**
     * @brief Заполнить входными данными внутренний буфер.
     * @param input Указатель на начало строкового представления числа.
     * @param size Длина строкового представления числа.
     */
    void FillData(const char* input, int size) {
        mRealSize = BoundSize(size);
        for (int i = 0; i < mRealSize; ++i) {
            mBuffer[i] = *input++;
        }
        for (int i = 0; i < MAX_SIZE - mRealSize; ++i) {
            mBuffer[mRealSize + i] = chars::null;
        }
        mBuffer[MAX_SIZE] = chars::null;
    }
public:
    /**
     * @brief Конструктор по умолчанию.
     */
    explicit Vector128() = default;

    /**
     * @brief Конструктор.
     * @param str
     */
    Vector128(const std::string& str) noexcept {
        FillData(str.data(), str.size());
    }

    Vector128(std::string&& str) = delete;

    /**
     * @brief Конструктор копирования.
     * @param other
     */
    Vector128(const Vector128& other) noexcept {
        FillData(other.mBuffer.data(), other.RealSize());
    }

    Vector128(Vector128&& other) = delete;

    Vector128& operator=(const std::string& str) {
        FillData(str.data(), str.size());
        return *this;
    }

    Vector128& operator=(const Vector128& other) {
        FillData(other.mBuffer.data(), other.RealSize());
        return *this;
    }

    char& operator[](int i) noexcept {
        assert(i >= 0);
        assert(i < MAX_SIZE);
        return mBuffer[i];
    }

    char operator[](int i) const noexcept {
        assert(i >= 0);
        assert(i < MAX_SIZE);
        return mBuffer[i];
    }

    int RealSize() const {
        return mRealSize;
    }

    static int MaxSize() {
        return MAX_SIZE;
    }

    void Resize(int new_size) {
        mRealSize = BoundSize(new_size);
        mBuffer[mRealSize] = chars::null;
    }

    /**
     * @brief Получить строковое представления числа.
     */
    auto GetStringView() const noexcept {
        return std::string_view(mBuffer.data(), mRealSize);
    }
};

class Decimal {

    static struct _Static {
        /**
         * @brief Количество цифр после запятой. Не более 19 для 128-битных чисел.
         */
        int mWidth = 3;

        /**
         * @brief Знаменатель дробной части числа.
         */
        u128::U128 mDenominator = int_power(10, mWidth);
    } global;

    /**
     * @brief Целая часть числа.
     */
    u128::U128 mInteger {u128::get_zero()};

    /**
     * @brief Числитель дробной части числа.
     */
    u128::U128 mNominator {u128::get_zero()};

    /**
     * @brief Измененный знаменатель. В процессе операций иногда требуется изменить знаменатель.
     * Если значение -1, то знаменатель не изменился.
     * Значение по умолчанию равно нулю для отработки NaN.
     */
    u128::U128 mChangedDenominator {u128::get_zero()};

    /**
     * @brief Строковое представление числа.
     */
    Vector128 mStringRepresentation;

    /**
     * @brief Преобразовать компоненты Decimal в строковое представление числа.
     */
    void TransformToString() {
        if (IsOverflowed()) {
            mStringRepresentation = "inf";
            return;
        }
        if (IsNotANumber()) {
            mStringRepresentation = "";
            return;
        }
        mChangedDenominator = mChangedDenominator == u128::get_unit_neg() ? global.mDenominator : mChangedDenominator;
        auto r = mInteger;
        const int the_sign = IsNegative();
        if (absolute_value(mNominator) >= mChangedDenominator) {
            auto tmp = mNominator / mChangedDenominator;
            r = the_sign == 0 ? r + tmp : r - tmp;
            if (mNominator.is_nonegative()) {
                auto res = mNominator - mChangedDenominator * tmp;
                mNominator = res;
            } else {
                auto res = mNominator + mChangedDenominator * tmp;
                mNominator = res;
            }
        }
        u128::U128 fraction = mNominator.is_negative() ? -mNominator : mNominator;
        const auto old_denominator = int_power(10, global.mWidth);
        if (old_denominator != mChangedDenominator) {
            fraction = fraction * old_denominator;
            fraction = fraction / mChangedDenominator;
        }
        mChangedDenominator = global.mDenominator;
        const int separator_length = global.mWidth < 1 ? 0 : 1;
        const int required_length = (num_of_digits(r) + separator_length + global.mWidth) + (the_sign == 0 ? 0 : 1);
        // Целая часть, разделитель, дробная часть (precision), знак.
        assert(required_length <= Vector128::MaxSize());
        assert(required_length > 0);
        mStringRepresentation.Resize(required_length);
        if (the_sign != 0) {
            mStringRepresentation[0] = chars::minus_sign;
        }
        if (r.is_zero()) {
            mStringRepresentation[required_length - global.mWidth - 1 - separator_length] = chars::zero;
        }
        r = absolute_value(r);
        if (r.is_overflow()) {
            mInteger = u128::get_unit_neg();
            mNominator = u128::get_unit_neg();
            mStringRepresentation = "inf";
            return;
        }
        for (int i = 0; !r.is_zero() ; i++) {
            mStringRepresentation[required_length - global.mWidth - 1 - separator_length - i] = u128::DIGITS[r.mod10()];
            r = r.div10();
        }
        if (separator_length > 0) {
            mStringRepresentation[required_length - 1 - global.mWidth] = chars::separator;
        }
        for (int i = 0; i < global.mWidth; i++) {
            mStringRepresentation[required_length - 1 - i] = u128::DIGITS[fraction.mod10()];
            fraction = fraction.div10();
        }
    }

    /**
     * @brief Преобразовать строковое представление числа в компоненты Decimal.
     */
    void TransformToDecimal() {
        if (mStringRepresentation.RealSize() < 1) {
            mInteger = u128::get_zero();
            mNominator = u128::get_zero();
            mChangedDenominator = u128::get_zero();
            return;
        }
        if (mStringRepresentation.GetStringView().starts_with("inf")) {
            mInteger = u128::get_unit_neg();
            mNominator = u128::get_unit_neg();
            return;
        }
        mNominator = u128::get_zero();
        mChangedDenominator = global.mDenominator;
        const int the_sign = mStringRepresentation[0] == chars::minus_sign ? 1 : 0;
        int current_index = the_sign == 0 ? 0 : 1;
        char digit = mStringRepresentation[current_index];
        mInteger = u128::get_zero();
        mInteger.mLow = undigits(digit); // В случае некорректного символа возвращается ноль.
        current_index++;
        digit = mStringRepresentation[current_index];
        bool is_overflow = false;
        while ((digit != chars::separator && digit != chars::alternative_separator) && digit != chars::null) {
            if (const auto tmp = mInteger * 10; tmp.is_overflow()) {
                is_overflow = true;
                break;
            }
            mInteger = mInteger * 10;
            if (auto tmp = mInteger + u128::U128{static_cast<u128::ULOW>(undigits(digit)), 0}; tmp.is_overflow()) {
                is_overflow = true;
                break;
            }
            mInteger = mInteger + u128::U128{static_cast<u128::ULOW>(undigits(digit)), 0};
            current_index++;
            digit = mStringRepresentation[current_index];
        } // while
        if (is_overflow) {
            mInteger = u128::get_unit_neg();
            mNominator = u128::get_unit_neg();
            mStringRepresentation = "inf";
            return;
        }
        mInteger = the_sign == 0 ? mInteger : -mInteger;
        if (digit == chars::null) {
            return;
        }
        current_index++;
        digit = mStringRepresentation[current_index];
        mNominator = mNominator + u128::U128{static_cast<u128::ULOW>(undigits(digit)), 0};
        current_index++;
        digit = mStringRepresentation[current_index];
        const int length = mStringRepresentation.RealSize();
        int idx_width = 1;
        while (current_index < length) {
            if (idx_width >= global.mWidth) { // Слишком много цифр после запятой.
                break;
            }
            mNominator = mNominator * 10;
            mNominator = mNominator + u128::U128{static_cast<u128::ULOW>(undigits(digit)), 0};
            current_index++;
            digit = mStringRepresentation[current_index];
            idx_width++;
        }
        while (idx_width < global.mWidth) { // Добавление нулей. Например 4,5 => 4,50 при width = 2.
            mNominator = mNominator * 10;
            mNominator = mNominator + u128::U128{0, 0};
            idx_width++;
        }
        if (mInteger.is_zero() && the_sign != 0) { // Если целая часть равна нулю, то знак храним в числителе.
            mNominator = -mNominator;
        }
    }

public:
    explicit Decimal() {
        TransformToString();
    }

    /**
     * @brief Установить количество знаков после запятой.
     * @param width Количество знаков после запятой.
     * @return Произошло ли изменение количества знаков.
     */
    static bool SetWidth(int width) {
        int old_width = global.mWidth;
        global.mWidth = std::clamp(width, 0, 9);
        global.mDenominator = int_power(10, global.mWidth);
        return global.mWidth != old_width;
    }

    static auto GetWidth() {
        return global.mWidth;
    }

    /**
     * @brief Установить ноль.
     */
    void SetZero() {
        SetDecimal(u128::get_zero(), u128::get_zero(), global.mDenominator);
    }

    /**
     * @brief Установить NaN.
     */
    void SetNotANumber() {
        SetDecimal(u128::get_zero(), u128::get_zero(), u128::get_zero());
    }

    /**
     * @brief Установить Inf.
     */
    void SetInfinity() {
        SetDecimal(u128::get_unit_neg(), u128::get_unit_neg());
    }

    /**
     * @brief Установить Decimal число покомпонентно.
     * @param integer Целая часть
     * @param nominator Числитель дробной части.
     * @param denominator Знаменатель дробной части.
     */
    void SetDecimal(u128::U128 integer, u128::U128 nominator, u128::U128 denominator = u128::get_unit_neg()) {
        mInteger = integer;
        mNominator = nominator;
        mChangedDenominator = denominator;
        // Сделать прямое-обратное преобразования для формирования знаменателя 10^width.
        TransformToString();
        TransformToDecimal();
    }

    /**
     * @brief Число целое.
     * @return Да/нет.
     */
    bool IsInteger() const {
        return mNominator.is_zero() && mChangedDenominator.is_positive();
    }

    /**
     * @brief Произошло переполнение при выполнении операции.
     * @return Да/нет.
     */
    bool IsOverflowed() const {
        return (mInteger.is_negative() && mNominator.is_negative()) ||
               (mInteger.is_overflow() || mNominator.is_overflow());
    }

    /**
     * @brief Не является числом.
     * @return Да/нет.
     */
    bool IsNotANumber() const {
        return (mInteger.is_zero() && mNominator.is_zero() && mChangedDenominator.is_zero()) ||
               (mInteger.is_nan() || mNominator.is_nan());
    }

    /**
     * @brief Число отрицательное в узком (сильном) смысле.
     * Здесь число по модулю не меньше единицы и знак хранится в целой части.
     * @return Да/нет.
     */
    bool IsStrongNegative() const {
        return mInteger.is_negative() && mNominator.is_nonegative() && mChangedDenominator.is_positive();
    }

    /**
     * @brief Число отрицательное в широком (слабом) смысле.
     * Здесь число по модулю меньше единице и знак хранится в числителе.
     * @return Да/нет.
     */
    bool IsWeakNegative() const {
        return mInteger.is_zero() && mNominator.is_negative() && mChangedDenominator.is_positive();
    }

    /**
     * @brief Число отрицательное.
     * @return Да/нет.
     */
    bool IsNegative() const {
        return IsStrongNegative() || IsWeakNegative();
    }

    /**
     * @brief Число есть ноль.
     * @return Да/нет.
     */
    bool IsZero() const {
        return mInteger.is_zero() && mNominator.is_zero() && mChangedDenominator.is_positive();
    }

    auto ValueAsStringView() const {
        return mStringRepresentation.GetStringView();
    }

    auto IntegerPart() const {
        return mInteger;
    }

    auto Nominator() const {
        return mNominator;
    }

    static auto Denominator() {
        return global.mDenominator;
    }

    /**
     * @brief Установить строковое представление числа.
     * @param str Строковое представление числа.
     */
    void SetStringRepresentation(const std::string& str) {
        mStringRepresentation = str;
        TransformToDecimal();
        TransformToString();
    }

    /**
     * @brief Оператор сложения двух чисел.
     * @param other Второй операнд.
     * @return Результат сложения двух чисел.
     */
    Decimal operator+(const Decimal& other) const {
        Decimal result{};
        const int neg1 = IsNegative();
        const int neg2 = other.IsNegative();
        auto tmp_integer_part = mInteger + other.mInteger;
        auto tmp_nominator = mNominator + other.mNominator;
        bool is_overflow1 = tmp_integer_part.is_overflow();
        bool is_overflow2 = tmp_nominator.is_overflow();
        if (is_overflow1 || is_overflow2) {
            result.SetInfinity();
            return result;
        }
        auto sum = tmp_integer_part;
        auto f = absolute_value(mNominator) + absolute_value(other.mNominator);
        const bool have_differ_signs = neg1 ^ neg2;
        if (neg1 && !neg2) {
            f = -absolute_value(mNominator) + absolute_value(other.mNominator);
        }
        if (!neg1 && neg2) {
            f = absolute_value(mNominator) - absolute_value(other.mNominator);
        }
        if (have_differ_signs) {
            if (f.is_negative() && sum.is_negative()) {
                f = -f;
            } else
                if (f.is_negative() && sum.is_positive()) {
                    f += global.mDenominator;
                    sum = sum - u128::get_unit();
                } else
                    if (f.is_positive() && sum.is_negative()) {
                        f -= global.mDenominator;
                        sum = sum + u128::get_unit();
                        if (!sum.is_zero()) {
                            f = absolute_value(f);
                        }
                    }
        }
        if (neg1 && neg2) {
            if (sum.is_zero()) {
                f = -f;
            }
        }
        result.SetDecimal(sum, f);
        return result;
    }

    Decimal operator-(const Decimal& other) const {
        Decimal res{};
        res.SetDecimal(-other.mInteger, other.mInteger.is_zero() ? -other.mNominator : other.mNominator);
        return res + *this;
    }

    /**
     * @brief Оператор умножения двух чисел.
     * @param other Второй операнд.
     * @return Результат умножения двух чисел.
     */
    Decimal operator*(const Decimal& other) const {
        Decimal result{};
        const bool neg1 = IsNegative();
        const bool neg2 = other.IsNegative();
        const bool overflow1 = (mInteger * other.mNominator).is_overflow();
        const bool overflow2 = (other.mInteger * mNominator).is_overflow();
        const bool overflow3 = (mInteger * mNominator).is_overflow();
        const bool overflow4 = (other.mInteger * other.mNominator).is_overflow();
        const bool overflow5 = (mNominator * other.mNominator).is_overflow();
        const bool overflow6 = (mInteger * other.mInteger).is_overflow();
        const bool all_integers = mNominator.is_zero() && other.mNominator.is_zero();
        if (overflow6) {
            result.SetInfinity();
            return result;
        }
        auto integer_part = mInteger * other.mInteger;
        auto fraction_part = integer_part * 0;
        if (all_integers) {
            result.SetDecimal(integer_part, fraction_part);
            return result;
        }
        const bool left_integer = mNominator.is_zero() && !other.mNominator.is_zero();
        if (left_integer) {
            if (overflow1) {
                result.SetInfinity();
                return result;
            }
            const auto A = absolute_value(mInteger) * absolute_value(other.mNominator);
            const auto tmp = A / global.mDenominator;
            integer_part += (neg1 ^ neg2) ? -tmp : tmp;
            fraction_part = A - tmp * global.mDenominator;
            if (neg1 ^ neg2) {
                fraction_part = integer_part.is_zero() ? -fraction_part : fraction_part;
            }
            result.SetDecimal(integer_part, fraction_part);
            return result;
        }
        const bool right_integer = !mNominator.is_zero() && other.mNominator.is_zero();
        if (right_integer) {
            if (overflow2) {
                result.SetInfinity();
                return result;
            }

            const auto A = absolute_value(mNominator) * absolute_value(other.mInteger);
            const auto tmp = A / global.mDenominator;
            integer_part += (neg1 ^ neg2) ? -tmp : tmp;
            fraction_part = A - tmp * global.mDenominator;
            if (neg1 ^ neg2) {
                fraction_part = integer_part.is_zero() ? -fraction_part : fraction_part;
            }
            result.SetDecimal(integer_part, fraction_part);
            return result;
        }
        if (overflow1 || overflow2 || overflow3 || overflow4 || overflow5 || overflow6) {
            result.SetInfinity();
            return result;
        }
        if (!neg1 && !neg2) {
            const auto A = mInteger*other.mNominator + mNominator*other.mInteger + (mNominator*other.mNominator)/global.mDenominator;
            const auto tmp = A / global.mDenominator;
            integer_part += tmp;
            fraction_part = A - tmp * global.mDenominator;
        }
        if (neg1 && neg2) {
            const int neg1_strong = IsStrongNegative();
            const int neg2_strong = other.IsStrongNegative();
            const int neg1_weak = IsWeakNegative();
            const int neg2_weak = other.IsWeakNegative();
            if (neg1_strong && neg2_strong) {
                const auto A = absolute_value(mInteger)*other.mNominator + absolute_value(other.mInteger)*mNominator + (mNominator*other.mNominator)/global.mDenominator;
                const auto tmp = A / global.mDenominator;
                integer_part += tmp;
                fraction_part = A - tmp * global.mDenominator;

            }
            if (neg1_weak && neg2_strong) {
                const auto A = absolute_value(other.mInteger)*absolute_value(mNominator) + (absolute_value(mNominator)*other.mNominator)/global.mDenominator;
                const auto tmp = A / global.mDenominator;
                integer_part += tmp;
                fraction_part = A - tmp * global.mDenominator;
            }
            if (neg1_strong && neg2_weak) {
                const auto A = absolute_value(mInteger)*absolute_value(other.mNominator) + (mNominator*absolute_value(other.mNominator))/global.mDenominator;
                const auto tmp = A / global.mDenominator;
                integer_part += tmp;
                fraction_part = A - tmp * global.mDenominator;
            }
            if (neg1_weak && neg2_weak) {

                const auto A = (absolute_value(mNominator)*absolute_value(other.mNominator))/global.mDenominator;
                const auto tmp = A / global.mDenominator;
                integer_part += tmp;
                fraction_part = A - tmp * global.mDenominator;
            }
        }
        if (neg1 && !neg2) {
            const int neg1_strong = IsStrongNegative();
            const int neg1_weak = IsWeakNegative();
            if (neg1_strong) {
                const auto A = absolute_value(mInteger)*other.mNominator + other.mInteger*mNominator + (mNominator*other.mNominator)/global.mDenominator;
                const auto tmp = A / global.mDenominator;
                integer_part = absolute_value(integer_part) + tmp;
                fraction_part = A - tmp * global.mDenominator;
                integer_part = -integer_part;
                fraction_part = integer_part.is_zero() ? -fraction_part : fraction_part;
            }
            if (neg1_weak) {
                const auto A = other.mInteger*absolute_value(mNominator) + (absolute_value(mNominator)*other.mNominator)/global.mDenominator;
                const auto tmp = A / global.mDenominator;
                integer_part = tmp;
                fraction_part = A - tmp * global.mDenominator;
                integer_part = -integer_part;
                fraction_part = integer_part.is_zero() ? -fraction_part : fraction_part;
            }
        }
        if (!neg1 && neg2) {
            const int neg2_strong = other.IsStrongNegative();
            const int neg2_weak = other.IsWeakNegative();
            if (neg2_strong) {
                const auto A = mInteger*other.mNominator + absolute_value(other.mInteger)*mNominator + (mNominator*other.mNominator)/global.mDenominator;
                const auto tmp = A / global.mDenominator;
                integer_part = absolute_value(integer_part) + tmp;
                fraction_part = A - tmp * global.mDenominator;
                integer_part = -integer_part;
                fraction_part = integer_part.is_zero() ? -fraction_part : fraction_part;
            }
            if (neg2_weak) {
                const auto A = mInteger*absolute_value(other.mNominator) + (mNominator*absolute_value(other.mNominator))/global.mDenominator;
                const auto tmp = A / global.mDenominator;
                integer_part = tmp;
                fraction_part = A - tmp * global.mDenominator;
                integer_part = -integer_part;
                fraction_part = integer_part.is_zero() ? -fraction_part : fraction_part;
            }
        }
        result.SetDecimal(integer_part, fraction_part);
        return result;
    }

    /**
     * @brief Оператор деления двух чисел.
     * @param other Делитель.
     * @return Результат деления двух чисел, this / other, с точностью width.
     */
    Decimal operator/(const Decimal& other) const {
        Decimal result{};
        if (other.IsZero() && !this->IsZero()) {
            result.SetInfinity();
            return result;
        }
        if (other.IsZero() && this->IsZero()) {
            result.SetNotANumber();
            return result;
        }
        const bool neg1 = IsNegative();
        const bool neg2 = other.IsNegative();
        const bool all_integers = mNominator.is_zero() && other.mNominator.is_zero();
        if (all_integers) {
            const auto A = absolute_value(mInteger);
            const auto B = absolute_value(other.mInteger);
            auto integer_part = A / B;
            auto fraction_part = A - integer_part * B;
            if (neg1 ^ neg2) {
                integer_part = integer_part.is_zero() ? integer_part : -integer_part;
                fraction_part = integer_part.is_zero() ? -fraction_part : fraction_part;
            }
            result.SetDecimal(integer_part, fraction_part, absolute_value(other.mInteger));
            return result;
        }
        const bool denominator_is_integer = other.mNominator.is_zero() && !other.mInteger.is_zero();
        if (denominator_is_integer) {
            const auto A = absolute_value(mInteger);
            const auto B = absolute_value(other.mInteger);
            auto div_part = A / B;
            auto mod_part = A - div_part * B;
            auto integer_part = div_part + mod_part / B;
            auto fraction_part = (absolute_value(mNominator) + mod_part * global.mDenominator) / B;
            if (neg1 ^ neg2) {
                integer_part = integer_part.is_zero() ? integer_part : -integer_part;
                fraction_part = integer_part.is_zero() ? -fraction_part : fraction_part;
            }
            result.SetDecimal(integer_part, fraction_part);
            return result;
        }
        {
            const auto tmp = mInteger * global.mDenominator;
            if ( tmp.is_overflow() ) {
                result.SetInfinity();
                return result;
            }
        }
        if (!neg1 && !neg2) {
            const auto A = mInteger * global.mDenominator + mNominator;
            const auto B = other.mInteger * global.mDenominator + other.mNominator;
            auto integer_part = A / B;
            auto fraction_part = A - integer_part * B;
            result.SetDecimal(integer_part, fraction_part, B);
        }
        if (neg1 && neg2) {
            const int neg1_strong = IsStrongNegative();
            const int neg2_strong = other.IsStrongNegative();
            const int neg1_weak = IsWeakNegative();
            const int neg2_weak = other.IsWeakNegative();
            if (neg1_strong && neg2_strong) {
                const auto A = absolute_value(mInteger) * global.mDenominator + mNominator;
                const auto B = absolute_value(other.mInteger) * global.mDenominator + other.mNominator;
                auto integer_part = A / B;
                auto fraction_part = A - integer_part * B;
                result.SetDecimal(integer_part, fraction_part, B);
            }
            if (neg1_weak && neg2_weak) {
                auto integer_part = mNominator / other.mNominator;
                const auto A = absolute_value(mNominator);
                const auto B = absolute_value(other.mNominator);
                auto div_part = A / B;
                auto fraction_part = A - div_part * B;
                result.SetDecimal(integer_part, fraction_part, B);
            }
            if (neg1_strong && neg2_weak) {
                const auto A = absolute_value(mInteger) * global.mDenominator + mNominator;
                const auto B = absolute_value(other.mNominator);
                auto integer_part = A / B;
                auto fraction_part = A - integer_part * B;
                result.SetDecimal(integer_part, fraction_part, B);
            }
            if (neg1_weak && neg2_strong) {
                const auto A = absolute_value(mNominator);
                const auto B = absolute_value(other.mInteger) * global.mDenominator + other.mNominator;
                auto integer_part = A / B;
                auto fraction_part = A - integer_part * B;
                result.SetDecimal(integer_part, fraction_part, B);
            }
        }
        if (neg1 && !neg2) {
            const int neg1_strong = IsStrongNegative();
            const int neg1_weak = IsWeakNegative();
            if (neg1_strong) {
                const auto A = absolute_value(mInteger) * global.mDenominator + mNominator;
                const auto B = other.mInteger * global.mDenominator + other.mNominator;
                auto integer_part = A / B;
                auto fraction_part = A - integer_part * B;
                integer_part = -integer_part;
                fraction_part = integer_part.is_zero() ? -fraction_part : fraction_part;
                result.SetDecimal(integer_part, fraction_part, B);
            }
            if (neg1_weak) {
                const auto A = absolute_value(mNominator);
                const auto B = other.mInteger * global.mDenominator + other.mNominator;
                auto integer_part = A / B;
                auto fraction_part = A - integer_part * B;
                integer_part = -integer_part;
                fraction_part = integer_part.is_zero() ? -fraction_part : fraction_part;
                result.SetDecimal(integer_part, fraction_part, B);
            }
        }
        if (!neg1 && neg2) {
            const int neg2_strong = other.IsStrongNegative();
            const int neg2_weak = other.IsWeakNegative();
            if (neg2_strong) {
                const auto A = mInteger * global.mDenominator + mNominator;
                const auto B = absolute_value(other.mInteger) * global.mDenominator + other.mNominator;
                auto integer_part = A / B;
                auto fraction_part = A - integer_part * B;
                integer_part = -integer_part;
                fraction_part = integer_part.is_zero() ? -fraction_part : fraction_part;
                result.SetDecimal(integer_part, fraction_part, B);
            }
            if (neg2_weak) {
                const auto A = mInteger * global.mDenominator + mNominator;
                const auto B = absolute_value(other.mNominator);
                auto integer_part = A / B;
                auto fraction_part = A - integer_part * B;
                integer_part = -integer_part;
                fraction_part = integer_part.is_zero() ? -fraction_part : fraction_part;
                result.SetDecimal(integer_part, fraction_part, B);
            }
        }
        return result;
    }
};

/**
 * @brief Оператор сравнения чисел Decimal по их строковому представлению.
 * @param lhs Первое число.
 * @param rhs Второе число.
 * @return Равны/Не равны.
 */
bool inline operator==(const Decimal& lhs, const Decimal& rhs) {
    return lhs.ValueAsStringView() == rhs.ValueAsStringView();
}

inline Decimal::_Static Decimal::global;

}
