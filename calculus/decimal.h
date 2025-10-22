#pragma once

#include <cassert>   // assert
#include <array>     // std::array
#include <string>    // std::string
#include <climits>   // CHAR_BIT
#include <algorithm> // std::clamp
#include "i128.hpp"    // I128
#include "u128_utils.h"

namespace dec_n {

using namespace bignum::i128;

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
         * @brief Наибольшее количество цифр после запятой.
         */
        static constexpr int MAX_WIDTH = 12;

        /**
         * @brief Знаменатель дробной части числа.
         */
        I128 mDenominator = u128::utils::int_power(10, mWidth);
    } global;

    /**
     * @brief Целая часть числа.
     */
    I128 mInteger {0};

    /**
     * @brief Числитель дробной части числа.
     */
    I128 mNominator {0};

    /**
     * @brief Измененный знаменатель. В процессе операций иногда требуется изменить знаменатель.
     * Если значение -1, то знаменатель не изменился.
     * Значение по умолчанию равно нулю для отработки NaN.
     */
    I128 mChangedDenominator {0};

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
        mChangedDenominator = mChangedDenominator == -I128{1} ? global.mDenominator : mChangedDenominator;
        // Сократим общий множитель.
        auto gcd = u128::utils::gcd(mNominator.unsigned_part(), mChangedDenominator.unsigned_part());
        if (!mNominator.is_zero() && gcd > I128{1}) {
            mNominator = (mNominator / gcd).first;
            mChangedDenominator = (mChangedDenominator / gcd).first;
        }
        auto r = mInteger;
        const int the_sign = IsNegative();
        // Выделим целую часть при необходимости.
        if (mNominator.abs() >= mChangedDenominator) {
            const auto& [tmp, remainder] = mNominator / mChangedDenominator;
            r = the_sign == 0 ? r + tmp : r - tmp;
            if (r.is_overflow()) {
                mStringRepresentation = "inf";
                return;
            }
            if (mNominator.is_nonegative()) {
                const auto& res = mNominator - mChangedDenominator * tmp;
                mNominator = res;
            } else {
                const auto& res = mNominator + mChangedDenominator * tmp;
                mNominator = res;
            }
        }
        // Пересчитаем числитель и знаменатель к эталонным.
        I128 fraction = mNominator.is_negative() ? -mNominator : mNominator;
        const auto& etalon_denominator = global.mDenominator;
        if (etalon_denominator != mChangedDenominator) {
            fraction = fraction * etalon_denominator;
            if (!fraction.is_singular())
                std::tie(fraction, std::ignore) = fraction / mChangedDenominator;
        }
        if (fraction.is_singular()) { // Улучшаем точность дробной части.
            // Может возникнуть, например, когда делим два больших сопоставимых числа.
            fraction = mNominator.is_negative() ? -mNominator : mNominator;
            if (etalon_denominator < mChangedDenominator) {
                const auto& scale = (mChangedDenominator / etalon_denominator).first;
                fraction = (fraction / scale).first;
            } else
            if (etalon_denominator > mChangedDenominator && mChangedDenominator.is_positive()) {
                const auto& [scale, rem] = etalon_denominator / mChangedDenominator;
                fraction = fraction * scale + ((rem * fraction) / mChangedDenominator).first;
            }
        }
        // Восстанавливаем эталонные числитель и знаменатель.
        mNominator = mNominator.is_negative() ? -fraction : fraction;
        mChangedDenominator = global.mDenominator;
        // Коррекция всех девяток.
        if ((global.mWidth > 0) && ((fraction + I128{1}) == mChangedDenominator)) {
            fraction = I128{0};
            r += the_sign != 0 ? -I128{1} : I128{1};
            mNominator = I128{0};
            mInteger = r;
            if (r.is_overflow()) {
                mStringRepresentation = "inf";
                return;
            }
        }
        //
        const int separator_length = global.mWidth < 1 ? 0 : 1;
        const auto& r_len = u128::utils::num_of_digits(r.unsigned_part());
        const int required_length = (r_len + separator_length + global.mWidth) + (the_sign != 0 ? 1 : 0);
        // Целая часть, разделитель, дробная часть (precision), знак.
        assert(required_length <= Vector128::MaxSize());
        assert(required_length > 0);
        mStringRepresentation.Resize(required_length);
        mStringRepresentation[0] = the_sign != 0 ? chars::minus_sign : mStringRepresentation[0];
        if (r.is_zero())
            mStringRepresentation[required_length - global.mWidth - 1 - separator_length] = chars::zero;
        r = r.abs();
        if (r.is_overflow()) {
            mStringRepresentation = "inf";
            return;
        }
        U128 ru = r.unsigned_part();
        for (int i = 0; ru != 0 ; i++) {
            const auto mod10 = ru.mod10();
            if (mod10 < 0)
                break;
            mStringRepresentation[required_length - global.mWidth - 1 - separator_length - i] = DIGITS[mod10];
            ru = ru.div10();
        }
        if (separator_length > 0)
            mStringRepresentation[required_length - 1 - global.mWidth] = chars::separator;
        U128 fraction_u = fraction.unsigned_part();
        for (int i = 0; i < global.mWidth; i++) {
            const auto mod10 = fraction_u.mod10();
            if (mod10 < 0) // Сингулярный fraction автоматически не отобразится.
                break;
            mStringRepresentation[required_length - 1 - i] = DIGITS[mod10];
            fraction_u = fraction_u.div10();
        }
    }

    /**
     * @brief Преобразовать строковое представление числа в компоненты Decimal.
     */
    void TransformToDecimal() {
        if (mStringRepresentation.RealSize() < 1) {
            mInteger = I128{0};
            mNominator = I128{0};
            mChangedDenominator = I128{0};
            return;
        }
        if (mStringRepresentation.GetStringView().starts_with("inf")) {
            mInteger = -I128{1};
            mNominator = -I128{1};
            return;
        }
        mNominator = I128{0};
        mChangedDenominator = global.mDenominator;
        const int the_sign = mStringRepresentation[0] == chars::minus_sign ? 1 : 0;
        int current_index = the_sign != 0 ? 1 : 0;
        char digit = mStringRepresentation[current_index];
        mInteger = I128{0};
        mInteger = undigits(digit); // В случае некорректного символа возвращается ноль.
        current_index++;
        digit = mStringRepresentation[current_index];
        bool is_overflow = false;
        while ((digit != chars::separator && digit != chars::alternative_separator) && digit != chars::null) {
            if (const auto tmp = mInteger * ULOW{10}; tmp.is_overflow()) {
                is_overflow = true;
                break;
            }
            mInteger = mInteger * ULOW{10};
            if (auto tmp = mInteger + I128{static_cast<ULOW>(undigits(digit)), 0}; tmp.is_overflow()) {
                is_overflow = true;
                break;
            }
            mInteger = mInteger + I128{static_cast<ULOW>(undigits(digit)), 0};
            current_index++;
            digit = mStringRepresentation[current_index];
        } // while
        if (is_overflow) {
            mInteger = -I128{1};
            mNominator = -I128{1};
            mStringRepresentation = "inf";
            return;
        }
        mInteger = the_sign != 0 ? -mInteger : mInteger;
        if (digit == chars::null)
            return;
        current_index++;
        digit = mStringRepresentation[current_index];
        mNominator = mNominator + u128::utils::get_by_digit( undigits(digit) );
        current_index++;
        digit = mStringRepresentation[current_index];
        const int length = mStringRepresentation.RealSize();
        int idx_width = 1;
        while (current_index < length) {
            if (idx_width >= global.mWidth) // Слишком много цифр после запятой.
                break;
            mNominator = mNominator * ULOW{10};
            mNominator = mNominator + u128::utils::get_by_digit( undigits(digit) );
            current_index++;
            digit = mStringRepresentation[current_index];
            idx_width++;
        }
        while (idx_width < global.mWidth) { // Добавление нулей. Например 4,5 => 4,50 при width = 2.
            mNominator = mNominator * ULOW{10};
            mNominator = mNominator + I128{0};
            idx_width++;
        }
        if (mInteger.is_zero() && the_sign != 0) // Если целая часть равна нулю, то знак храним в числителе.
            mNominator = -mNominator;
    }

public:
    explicit Decimal() {
        TransformToString();
    }

    Decimal operator-() const {
        Decimal result = *this;
        if (result.mInteger.is_zero()) {
            result.mNominator = -result.mNominator;
        } else {
            result.mInteger = -result.mInteger;
        }
        result.TransformToString();
        return result;
    }

    /**
     * @brief Установить количество знаков после запятой.
     * @param width Количество знаков после запятой.
     * @return Произошло ли изменение количества знаков.
     */
    static bool SetWidth(int width) {
        int old_width = global.mWidth;
        global.mWidth = std::clamp(width, 0, global.MAX_WIDTH);
        global.mDenominator = u128::utils::int_power(10, global.mWidth);
        return global.mWidth != old_width;
    }

    static auto GetWidth() {
        return global.mWidth;
    }

    /**
     * @brief Установить ноль.
     */
    void SetZero() {
        SetDecimal(I128{0}, I128{0}, global.mDenominator);
    }

    /**
     * @brief Установить NaN.
     */
    void SetNotANumber() {
        SetDecimal(I128{0}, I128{0}, I128{0});
    }

    /**
     * @brief Установить Inf.
     */
    void SetInfinity() {
        SetDecimal(-I128{1}, -I128{1});
    }

    /**
     * @brief Установить Decimal число покомпонентно.
     * @param integer Целая часть
     * @param nominator Числитель дробной части.
     * @param denominator Знаменатель дробной части.
     */
    void SetDecimal(I128 integer, I128 nominator, I128 denominator = -I128{1}) {
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

    Decimal Abs() const {
        Decimal result = *this;
        if (result.IsNegative()) {
            result = -result;
        }
        return result;
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
        Decimal result;
        if (other.IsOverflowed() || this->IsOverflowed()) {
            result.SetInfinity();
            return result;
        }
        if (other.IsNotANumber() || this->IsNotANumber()) {
            result.SetNotANumber();
            return result;
        }
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
        auto f = mNominator.abs() + other.mNominator.abs();
        const bool have_differ_signs = neg1 ^ neg2;
        if (neg1 && !neg2) {
            f = -mNominator.abs() + other.mNominator.abs();
        }
        if (!neg1 && neg2) {
            f = mNominator.abs() - other.mNominator.abs();
        }
        if (have_differ_signs) {
            if (f.is_negative() && sum.is_negative()) {
                f = -f;
            } else
                if (f.is_negative() && sum.is_positive()) {
                    f += global.mDenominator;
                    sum -= 1;
                } else
                    if (f.is_positive() && sum.is_negative()) {
                        f -= global.mDenominator;
                        sum += 1;
                        if (!sum.is_zero()) {
                            f = f.abs();
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
        Decimal res;
        res.SetDecimal(-other.mInteger, other.mInteger.is_zero() ? -other.mNominator : other.mNominator);
        return res + *this;
    }

    /**
     * @brief Оператор сложения с целым числом.
     * @param other Целое число.
     * @return Результат сложения, this + other, с точностью width.
     */
    Decimal operator+(const I128& other) const {
        Decimal N; N.SetDecimal( other, I128{0});
        return *this + N;
    }

    /**
     * @brief Оператор вычитания целого числа.
     * @param other Целое число.
     * @return Результат вычитания, this - other, с точностью width.
     */
    Decimal operator-(const I128& other) const {
        Decimal N; N.SetDecimal( other, I128{0} );
        return *this - N;
    }

    /**
     * @brief Оператор умножения двух чисел.
     * @param other Второй операнд.
     * @return Результат умножения двух чисел.
     */
    Decimal operator*(const Decimal& other) const {
        Decimal result;
        if (other.IsOverflowed() || this->IsOverflowed()) {
            result.SetInfinity();
            return result;
        }
        if (other.IsNotANumber() || this->IsNotANumber()) {
            result.SetNotANumber();
            return result;
        }
        auto integer_part = mInteger * other.mInteger;
        if (integer_part.is_overflow()) {
            result.SetInfinity();
            return result;
        }
        const bool all_integers = mNominator.is_zero() && other.mNominator.is_zero();
        auto fraction_part = integer_part * ULOW{0};
        if (all_integers) {
            result.SetDecimal(integer_part, fraction_part);
            return result;
        }
        const bool neg1 = IsNegative();
        const bool neg2 = other.IsNegative();
        const bool left_integer = mNominator.is_zero() && !other.mNominator.is_zero();
        if (left_integer) {
            const auto A = mInteger.abs() * other.mNominator.abs();
            if (A.is_overflow()) {
                Decimal N; N.SetDecimal( mInteger, I128{0} ); // Через Decimal вычисляется точно.
                Decimal M; M.SetDecimal( global.mDenominator, I128{0} );
                Decimal P; P.SetDecimal( other.mNominator, I128{0} );
                N = N / M;
                N = N * P;
                result.SetDecimal(integer_part, I128{0});
                result = result + N;
                return result;
            }
            const auto& [tmp, remainder] = A / global.mDenominator;
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
            const auto& A = mNominator.abs() * other.mInteger.abs();
            if (A.is_overflow()) {
                Decimal N; N.SetDecimal( other.mInteger, I128{0} ); // Через Decimal вычисляется точно.
                Decimal M; M.SetDecimal( global.mDenominator, I128{0} );
                Decimal P; P.SetDecimal( mNominator, I128{0} );
                N = N / M;
                N = N * P;
                result.SetDecimal(integer_part, I128{0});
                result = result + N;
                return result;
            }
            const auto& [tmp, remainder] = A / global.mDenominator;
            integer_part += (neg1 ^ neg2) ? -tmp : tmp;
            fraction_part = A - tmp * global.mDenominator;
            if (neg1 ^ neg2) {
                fraction_part = integer_part.is_zero() ? -fraction_part : fraction_part;
            }
            result.SetDecimal(integer_part, fraction_part);
            return result;
        }
        // Оба дробные, и хотя бы один из них имеет ненулевую целую часть.
        if ((!right_integer && !left_integer) && (!this->mInteger.is_zero() || !other.mInteger.is_zero())) {
            if (this->mInteger.abs() >= other.mInteger.abs()) {
                Decimal N; N.SetDecimal(this->mInteger, I128{0});
                result = N * other;
                Decimal M; M.SetDecimal(I128{0}, (this->mInteger.is_negative() ? -this->mNominator : this->mNominator));
                result = result + M * other;
                return result;
            } else {
                Decimal N; N.SetDecimal(other.mInteger, I128{0});
                result = N * (*this);
                Decimal M; M.SetDecimal(I128{0}, (other.mInteger.is_negative() ? -other.mNominator : other.mNominator));
                result = result + M * (*this);
                return result;
            }
        }
        if (!neg1 && !neg2) {
            const auto A = mInteger*other.mNominator + mNominator*other.mInteger + ((mNominator*other.mNominator)/global.mDenominator).first;
            if (A.is_overflow()) {
                result.SetInfinity();
                return result;
            }
            const auto& [tmp, remainder] = A / global.mDenominator;
            integer_part += tmp;
            // fraction_part = A - tmp * global.mDenominator;
            fraction_part = remainder;
        }
        if (neg1 && neg2) {
            const int neg1_strong = IsStrongNegative();
            const int neg2_strong = other.IsStrongNegative();
            const int neg1_weak = IsWeakNegative();
            const int neg2_weak = other.IsWeakNegative();
            if (neg1_strong && neg2_strong) {
                const auto& A = mInteger.abs()*other.mNominator + other.mInteger.abs()*mNominator + ((mNominator*other.mNominator)/global.mDenominator).first;
                if (A.is_overflow()) {
                    result.SetInfinity();
                    return result;
                }
                const auto& [tmp, remainder] = A / global.mDenominator;
                integer_part += tmp;
                // fraction_part = A - tmp * global.mDenominator;
                fraction_part = remainder;

            }
            if (neg1_weak && neg2_strong) {
                const auto& A = other.mInteger.abs()*mNominator.abs() + ((mNominator.abs()*other.mNominator)/global.mDenominator).first;
                if (A.is_overflow()) {
                    result.SetInfinity();
                    return result;
                }
                const auto [tmp, remainder] = A / global.mDenominator;
                integer_part += tmp;
                // fraction_part = A - tmp * global.mDenominator;
                fraction_part = remainder;
            }
            if (neg1_strong && neg2_weak) {
                const auto& A = mInteger.abs()*other.mNominator.abs() + ((mNominator*other.mNominator.abs())/global.mDenominator).first;
                if (A.is_overflow()) {
                    result.SetInfinity();
                    return result;
                }
                const auto [tmp, remainder] = A / global.mDenominator;
                integer_part += tmp;
                // fraction_part = A - tmp * global.mDenominator;
                fraction_part = remainder;
            }
            if (neg1_weak && neg2_weak) {
                const auto& [A, remainder] = (mNominator.abs()*other.mNominator.abs())/global.mDenominator;
                if (A.is_overflow()) {
                    result.SetInfinity();
                    return result;
                }
                const auto& [tmp, remainder2] = A / global.mDenominator;
                integer_part += tmp;
                // fraction_part = A - tmp * global.mDenominator;
                fraction_part = remainder2;
            }
        }
        if (neg1 && !neg2) {
            const int neg1_strong = IsStrongNegative();
            const int neg1_weak = IsWeakNegative();
            if (neg1_strong) {
                const auto& A = mInteger.abs()*other.mNominator + other.mInteger*mNominator + ((mNominator*other.mNominator)/global.mDenominator).first;
                if (A.is_overflow()) {
                    result.SetInfinity();
                    return result;
                }
                const auto& [tmp, remainder] = A / global.mDenominator;
                integer_part = integer_part.abs() + tmp;
                fraction_part = A - tmp * global.mDenominator;
                integer_part = -integer_part;
                fraction_part = integer_part.is_zero() ? -fraction_part : fraction_part;
            }
            if (neg1_weak) {
                const auto& A = other.mInteger*mNominator.abs() + ((mNominator.abs()*other.mNominator)/global.mDenominator).first;
                if (A.is_overflow()) {
                    result.SetInfinity();
                    return result;
                }
                const auto& [tmp, remainder] = A / global.mDenominator;
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
                const auto& A = mInteger*other.mNominator + other.mInteger.abs()*mNominator + ((mNominator*other.mNominator)/global.mDenominator).first;
                if (A.is_overflow()) {
                    result.SetInfinity();
                    return result;
                }
                const auto& [tmp, remainder] = A / global.mDenominator;
                integer_part = integer_part.abs() + tmp;
                fraction_part = A - tmp * global.mDenominator;
                integer_part = -integer_part;
                fraction_part = integer_part.is_zero() ? -fraction_part : fraction_part;
            }
            if (neg2_weak) {
                const auto& A = mInteger*other.mNominator.abs() + ((mNominator*other.mNominator.abs())/global.mDenominator).first;
                if (A.is_overflow()) {
                    result.SetInfinity();
                    return result;
                }
                const auto& [tmp, remainder] = A / global.mDenominator;
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
        Decimal result;
        if (other.IsZero() && !this->IsZero()) {
            result.SetInfinity();
            return result;
        }
        if (other.IsZero() && this->IsZero()) {
            result.SetNotANumber();
            return result;
        }
        if (other.IsOverflowed() || this->IsOverflowed()) {
            result.SetInfinity();
            return result;
        }
        if (other.IsNotANumber() || this->IsNotANumber()) {
            result.SetNotANumber();
            return result;
        }
        const bool neg1 = IsNegative();
        const bool neg2 = other.IsNegative();
        const bool all_integers = mNominator.is_zero() && other.mNominator.is_zero();
        if (all_integers) {
            const auto& A = mInteger.abs();
            const auto& B = other.mInteger.abs();
            auto [integer_part, remainder] = A / B;
            auto fraction_part = A - integer_part * B;
            if (neg1 ^ neg2) {
                integer_part = integer_part.is_zero() ? integer_part : -integer_part;
                fraction_part = integer_part.is_zero() ? -fraction_part : fraction_part;
            }
            result.SetDecimal(integer_part, fraction_part, B);
            return result;
        }
        const bool denominator_is_integer = other.mNominator.is_zero() && !other.mInteger.is_zero();
        if (denominator_is_integer) {
            const auto& A = mInteger.abs();
            const auto& B = other.mInteger.abs();
            const auto& [div_part, remainder] = A / B;
            const auto& mod_part = A - div_part * B;
            auto integer_part = div_part + (mod_part / B).first;
            auto [fraction_part, remainder2] = (mNominator.abs() + mod_part * global.mDenominator) / B;
            if (neg1 ^ neg2) {
                integer_part = integer_part.is_zero() ? integer_part : -integer_part;
                fraction_part = integer_part.is_zero() ? -fraction_part : fraction_part;
            }
            result.SetDecimal(integer_part, fraction_part);
            return result;
        }
        const bool nominator_has_integer = !mInteger.is_zero();
        if (nominator_has_integer) {
            const I128& tmp = mInteger.abs() * global.mDenominator + mNominator.abs();
            if (tmp.is_overflow()) {
                Decimal N; N.SetDecimal( mInteger, mNominator );
                const bool sign = other.IsNegative();
                const auto& D = other.mNominator.abs() + global.mDenominator*other.mInteger.abs();
                Decimal M; M.SetDecimal( sign ? -D : D, I128{0} );
                Decimal P; P.SetDecimal(global.mDenominator, I128{0} );
                const auto old_N = N;
                N = N / M; // Точность теряется, вычисляем ошибку E.
                const auto& E = old_N - N * M;
                N = N * P;
                result = N + ((E * P) / M);
                return result;
            }
        }
        if (!neg1 && !neg2) {
            const auto& A = mInteger * global.mDenominator + mNominator;
            const auto& B = other.mInteger * global.mDenominator + other.mNominator;
            const auto& [integer_part, remainder] = A / B;
            const auto& fraction_part = A - integer_part * B;
            result.SetDecimal(integer_part, fraction_part, B);
        }
        if (neg1 && neg2) {
            const int neg1_strong = IsStrongNegative();
            const int neg2_strong = other.IsStrongNegative();
            const int neg1_weak = IsWeakNegative();
            const int neg2_weak = other.IsWeakNegative();
            if (neg1_strong && neg2_strong) {
                const auto& A = mInteger.abs() * global.mDenominator + mNominator;
                const auto& B = other.mInteger.abs() * global.mDenominator + other.mNominator;
                const auto& [integer_part, remainder] = A / B;
                const auto& fraction_part = A - integer_part * B;
                result.SetDecimal(integer_part, fraction_part, B);
            }
            if (neg1_weak && neg2_weak) {
                auto [integer_part, remainder] = mNominator / other.mNominator;
                const auto& A = mNominator.abs();
                const auto& B = other.mNominator.abs();
                const auto& [div_part, remainder2] = A / B;
                const auto& fraction_part = A - div_part * B;
                result.SetDecimal(integer_part, fraction_part, B);
            }
            if (neg1_strong && neg2_weak) {
                const auto A = mInteger.abs() * global.mDenominator + mNominator;
                const auto B = other.mNominator.abs();
                auto [integer_part, remainder] = A / B;
                auto fraction_part = A - integer_part * B;
                result.SetDecimal(integer_part, fraction_part, B);
            }
            if (neg1_weak && neg2_strong) {
                const auto& A = mNominator.abs();
                const auto& B = other.mInteger.abs() * global.mDenominator + other.mNominator;
                const auto& [integer_part, remainder] = A / B;
                const auto& fraction_part = A - integer_part * B;
                result.SetDecimal(integer_part, fraction_part, B);
            }
        }
        if (neg1 && !neg2) {
            const int neg1_strong = IsStrongNegative();
            const int neg1_weak = IsWeakNegative();
            if (neg1_strong) {
                const auto& A = mInteger.abs() * global.mDenominator + mNominator;
                const auto& B = other.mInteger * global.mDenominator + other.mNominator;
                auto [integer_part, remainder] = A / B;
                auto fraction_part = A - integer_part * B;
                integer_part = -integer_part;
                fraction_part = integer_part.is_zero() ? -fraction_part : fraction_part;
                result.SetDecimal(integer_part, fraction_part, B);
            }
            if (neg1_weak) {
                const auto& A = mNominator.abs();
                const auto& B = other.mInteger * global.mDenominator + other.mNominator;
                auto [integer_part, remainder] = A / B;
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
                const auto& A = mInteger * global.mDenominator + mNominator;
                const auto& B = other.mInteger.abs() * global.mDenominator + other.mNominator;
                auto [integer_part, remainder] = A / B;
                auto fraction_part = A - integer_part * B;
                integer_part = -integer_part;
                fraction_part = integer_part.is_zero() ? -fraction_part : fraction_part;
                result.SetDecimal(integer_part, fraction_part, B);
            }
            if (neg2_weak) {
                const auto& A = mInteger * global.mDenominator + mNominator;
                const auto& B = other.mNominator.abs();
                auto [integer_part, remainder] = A / B;
                auto fraction_part = A - integer_part * B;
                integer_part = -integer_part;
                fraction_part = integer_part.is_zero() ? -fraction_part : fraction_part;
                result.SetDecimal(integer_part, fraction_part, B);
            }
        }
        return result;
    }

    /**
     * @brief Оператор деления на целое число.
     * @param other Делитель.
     * @return Результат деления двух чисел, this / other, с точностью width.
     */
    Decimal operator/(const I128& other) const {
        Decimal N; N.SetDecimal( other, I128{0} );
        return *this / N;
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

/**
 * @brief Извлечение квадратного корня.
 * @param x Число.
 * @param exact Признак, что корень извлекся точно: квадрат даст исходное значение.
 * @return Квадратный корень числа.
 */
inline Decimal Sqrt(Decimal x, bool& exact) {
    if (x.IsNotANumber() || x.IsOverflowed()) {
        exact = false;
        return x;
    }
    if (x.IsZero()) {
        exact = true;
        return x;
    }
    x = x.Abs();
    Decimal result;
    // Установка Nominator позволяет извлекать корень из чисел менее 1.
    result.SetDecimal( u128::utils::isqrt(x.IntegerPart().unsigned_part(), exact), x.Nominator());
    if (exact && x.Nominator().is_zero()) {
        return result;
    }
    exact = false;
    Decimal prevprev;
    prevprev.SetDecimal(-I128{1}, I128{0});
    auto prev = x;
    Decimal two;
    two.SetDecimal(I128{2, 0}, I128{0});
    for (;;) {
        prevprev = prev;
        prev = result;
        const auto tmp = x / result;
        if (!tmp.IsOverflowed()) {
            result = (result + tmp) / two;
        } else { // Нехватка точности из-за большого количества знаков после запятой: делаем "финт ушами".
            const auto tmp_r = x / result.IntegerPart();
            result = (tmp_r + result.IntegerPart()) / two;
        }
        if (result.IsZero()) {
            exact = true;
            return result;
        }
        if (result == prev) {
            exact = (result * result) == x;
            return result;
        }
        if (result == prevprev) {
            return prev;
        }
    }
}

inline Decimal::_Static Decimal::global;

}
