#pragma once

#include <cassert>   // assert
#include <array>     // std::array
#include <string>    // std::string
#include <climits>   // CHAR_BIT
#include <algorithm> // std::clamp
#include "i128.hpp"    // I128
#include "u128_utils.h"
#include "defines.h"

namespace dec_n {

using namespace bignum::i128;

constexpr int undigits(char d) {
    if (d >= '0' && d <= '9') return d - '0';
    return 0;
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
        // Копируем данные
        std::copy_n(input, mRealSize, mBuffer.begin());
        // Обнуляем оставшуюся часть (включая защитный ноль в конце)
        std::fill(mBuffer.begin() + mRealSize, mBuffer.end(), chars::null);
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
    static struct _Static
    {
        /**
         * @brief Текущее количество цифр после запятой.
         */
        int mWidth = 3;

        /**
         * @brief Наибольшее количество цифр после запятой (безопасный предел для uint64_t).
         */
        static constexpr int MAX_WIDTH = 18;

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
        // Автоматически компенсируем потерю до 2 единиц младшего разряда
        if (global.mWidth > 0) {
            I128 diff = mChangedDenominator - fraction;
            // Если до следующего целого числа не хватает всего 1 или 2 единиц (например, ...999 или ...998)
            if (diff > I128{0} && diff <= I128{2}) {
                fraction = I128{0};
                r += the_sign != 0 ? -I128{1} : I128{1};
                mNominator = I128{0};
                mInteger = r;
                if (r.is_overflow()) {
                    mStringRepresentation = "inf";
                    return;
                }
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
        const U128 divisor_10{10ull};

        for (int i = 0; ru != U128{0}; i++) {
            U128 remainder;
            // Передаем указатель &remainder в третий аргумент.
            // Функция запишет туда остаток, а вернет частное.
            U128 quotient = U128::divide<true, true>(ru, divisor_10, &remainder);

            // Используем публичный метод .low() для получения индекса символа
            mStringRepresentation[required_length - global.mWidth - 1 - separator_length - i]
                = DIGITS[remainder.low()];
            ru = quotient;
        }

        if (separator_length > 0)
            mStringRepresentation[required_length - 1 - global.mWidth] = chars::separator;

        U128 fraction_u = fraction.unsigned_part();
        for (int i = 0; i < global.mWidth; i++) {
            U128 remainder;
            U128 quotient = U128::divide<true, true>(fraction_u, divisor_10, &remainder);

            mStringRepresentation[required_length - 1 - i] = DIGITS[remainder.low()];
            fraction_u = quotient;
        }
    }

    /**
     * @brief Преобразовать строковое представление числа в компоненты Decimal.
     */
    void TransformToDecimal()
    {
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

        const int the_sign = (mStringRepresentation[0] == chars::minus_sign) ? 1 : 0;
        int current_index = the_sign != 0 ? 1 : 0;

        U128 accum_integer{0};
        char digit = mStringRepresentation[current_index];
        accum_integer = static_cast<uint64_t>(undigits(digit));
        current_index++;
        digit = mStringRepresentation[current_index];

        bool is_overflow = false;
        while ((digit != chars::separator && digit != chars::alternative_separator)
               && digit != chars::null) {
            // Быстрое умножение на 10: x * 10 = (x << 3) + (x << 1)
            U128 next_val = (accum_integer << 3) + (accum_integer << 1);

            // Контроль переполнения при умножении
            if (next_val < accum_integer) {
                is_overflow = true;
                break;
            }

            U128 prev_val = next_val;
            next_val += static_cast<uint64_t>(undigits(digit));

            // Контроль переполнения при сложении цифры
            if (next_val < prev_val) {
                is_overflow = true;
                break;
            }

            accum_integer = next_val;
            current_index++;
            digit = mStringRepresentation[current_index];
        }

        if (is_overflow) {
            mInteger = -I128{1};
            mNominator = -I128{1};
            mStringRepresentation = "inf";
            return;
        }

        // Собираем целую часть через честный конструктор знакового I128
        mInteger = I128{accum_integer, false};
        mInteger = the_sign != 0 ? -mInteger : mInteger;

        if (digit == chars::null) {
            return;
        }

        // --- Накопление числителя дробной части на чистом U128 ---
        U128 accum_nominator{0};
        current_index++; // Пропускаем разделитель (запятую или точку)
        digit = mStringRepresentation[current_index];

        accum_nominator = static_cast<uint64_t>(undigits(digit));
        current_index++;
        digit = mStringRepresentation[current_index];

        const int length = mStringRepresentation.RealSize();
        int idx_width = 1;

        while (current_index < length) {
            if (idx_width >= global.mWidth)
                break;

            accum_nominator = (accum_nominator << 3) + (accum_nominator << 1)
                              + static_cast<uint64_t>(undigits(digit));

            current_index++;
            digit = mStringRepresentation[current_index];
            idx_width++;
        }

        // Быстрое дополнение нулями (например, 4,5 => 4,500)
        while (idx_width < global.mWidth) {
            accum_nominator = (accum_nominator << 3) + (accum_nominator << 1);
            idx_width++;
        }

        mNominator = I128{accum_nominator, false};

        // Если целая часть ноль, а число отрицательное (например, -0,5) — знак уходит в числитель дробной части
        if (mInteger.is_zero() && the_sign != 0) {
            mNominator = -mNominator;
        }
    }

public:
    explicit Decimal() {
        TransformToString();
    }

    Decimal operator-() const {
        Decimal result = *this;
        if (result.IsNotANumber() || result.IsOverflowed()) return result;
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

    static bool SetMaxWidth() {
        int old_width = global.mWidth;
        global.mWidth = global.MAX_WIDTH;
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
     * @brief operator ==
     * @param other
     * @return
     */
    bool operator==(const Decimal& other) const {
        return mInteger == other.mInteger && mNominator == other.mNominator;
    }

    /**
     * @brief operator <
     * @param other
     * @return
     */
    bool operator<(const Decimal& other) const {
        I128 zero(0);

        // Случай 1: Разные знаки целых частей
        if (mInteger != other.mInteger) {
            // Если обе целые части не равны нулю, просто сравниваем их
            if (mInteger != zero && other.mInteger != zero) {
                return mInteger < other.mInteger;
            }
            // Если одна из целых частей равна нулю, нужно быть аккуратнее,
            // так как знак второго числа может «прятаться» в числителе.
            // Поэтому переходим к общему покомпонентному сравнению ниже.
        }

        // Случай 2: Целые части одинаковы и не равны нулю
        if (mInteger == other.mInteger && mInteger != zero) {
            if (mInteger > zero) {
                // Для положительных: у кого числитель меньше, тот и меньше
                return mNominator < other.mNominator;
            } else {
                // Для отрицательных: знак числителя обычно совпадает со знаком целой части
                // (или числитель хранится как положительный модуль — зависит от вашей архитектуры).
                // Если у вас числитель отрицательного числа тоже отрицательный:
                return mNominator < other.mNominator;
                // Если числитель у вас ВСЕГДА положительный, а знак только в mInteger, то знак меняется:
                // return mNominator > other.mNominator;
            }
        }

        // Случай 3: Обе или одна из целых частей равны нулю (знак может быть в числителе)
        // Самый надежный способ для этого пограничного случая — сравнить mInteger и mNominator напрямую.
        // Так как целая часть имеет больший приоритет, мы можем условно «склеить» их логически:
        if (mInteger != other.mInteger) {
            return mInteger < other.mInteger;
        }
        // Если и целые части равны (например, обе 0), просто сравниваем числители
        return mNominator < other.mNominator;
    }

    bool operator>(const Decimal& other) const { return other < *this; }
    bool operator<=(const Decimal& other) const { return !(*this > other); }
    bool operator>=(const Decimal& other) const { return !(*this < other); }
    bool operator!=(const Decimal& other) const { return !(*this == other); }

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
        auto fraction_part = integer_part * u64{0};
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

/**
 * @brief Sqrt
 * @param x
 * @return
 */
inline Decimal Sqrt(Decimal x){
    bool ok;
    return Sqrt(x, ok);
}

inline Decimal ln_inner(Decimal x) {
    if (x.IsNotANumber() || x.IsOverflowed()) return x;
    if (x.IsZero()) {
        Decimal inf; inf.SetInfinity();
        return inf;
    }

    // Логарифм отрицательного числа не определен (NaN)
    if (x.IsNegative()) {
        Decimal nan; nan.SetNotANumber();
        return nan;
    }

    Decimal one; one.SetDecimal(1, 0);
    Decimal zero; zero.SetDecimal(0, 0);

    if ((x - one).IsZero()) return zero;

    Decimal LN2; LN2.SetStringRepresentation("0.693147180559945309");
    Decimal two; two.SetDecimal(2, 0);

    // Правильные границы приведения
    Decimal upper_bound; upper_bound.SetStringRepresentation("1.414213562373095048"); // Sqrt(2)
    Decimal lower_bound; lower_bound.SetStringRepresentation("0.707106781186547524"); // 1/Sqrt(2)

    int k = 0;

    // Шаг 1: Грубое приведение к диапазону [1/sqrt(2), sqrt(2)]
    while (!(x - upper_bound).IsNegative()) {
        x = x / two;
        k++;
    }
    while ((x - lower_bound).IsNegative()) {
        x = x * two;
        k--;
    }

    // --- ТРЮК ДЛЯ ИТЕРАЦИИ ЧИСЕЛ МЕНЬШЕ 1 ---
    bool invert_sub_sum = false;
    if ((x - one).IsNegative()) {
        x = one / x;
        invert_sub_sum = true;
    }

    // Шаг 2: ИТЕРАЦИОННЫЙ ПРОЦЕСС БРЕЙДИ
    Decimal a = Sqrt(two);
    Decimal w = LN2 / two;
    Decimal sum; sum.SetDecimal(0, 0);

    // Условие !w.IsZero() зависит от точности вашего Decimal.
    // Если разрядов много, лучше итерировать фиксированное число раз (например, 60-100 итераций)
    while (!w.IsZero()) {
        if (!(x - a).IsNegative()) {
            x = x / a;
            sum = sum + w;
        }
        a = Sqrt(a);
        w = w / two;
    }

    // Шаг 3: Финальная сборка
    Decimal k_dec;
    if (k >= 0) {
        k_dec.SetDecimal(k, 0);
    } else {
        k_dec.SetDecimal(I128{-k, true}, 0);
    }

    if (invert_sub_sum) {
        return (k_dec * LN2) - sum;
    }

    return sum + (k_dec * LN2);
}

inline Decimal Ln(Decimal x) {
    return ln_inner(x);
}

inline Decimal exp_inner(Decimal x)
{
    if (x.IsNotANumber() || x.IsOverflowed()) return x;

    if (x.IsZero()) {
        Decimal one; one.SetDecimal(1, 0);
        return one;
    }

    Decimal one; one.SetDecimal(1, 0);
    Decimal LN2; LN2.SetStringRepresentation("0.693147180559945309");

    // 1. Обработка отрицательных чисел: exp(-x) = 1 / exp(x)
    if (x.IsNegative()) {
        x = x.Abs();
        return one / exp_inner(x);
    }

    // 2. Выделяем целую часть k = floor(x / ln2)
    // Разделим x на ln2, чтобы понять, сколько степеней двойки нужно вынести
    Decimal k_dec = x / LN2;
    // Здесь должна быть ваша функция приведения к целому (Truncate / Floor)
    k_dec.SetDecimal(k_dec.IntegerPart(), 0);

    // Остаток r = x - k * ln2
    Decimal r = x - (k_dec * LN2);

    // 3. Вычисление exp(r) через ряд Тейлора: 1 + r + r^2/2! + r^3/3! + ...
    Decimal term = one;
    Decimal sum = one;
    Decimal count;

    for (int i = 1; i < 40; i++) { // 40 итераций обычно за глаза хватает для precision 128-bit
        count.SetDecimal(i, 0);
        term = (term * r) / count;

        if (term.IsZero()) break; // Достигли предела точности типа
        sum = sum + term;
    }

    // 4. Сборка результата: sum * 2^k
    // Переводим k_dec обратно в int для цикла умножения на 2
    int k = k_dec.IntegerPart().unsigned_part().low();
    Decimal two; two.SetDecimal(2, 0);

    for (int i = 0; i < k; i++) {
        sum = sum * two;
        if (sum.IsOverflowed()) return sum;
    }

    return sum;
}

inline Decimal Exp(Decimal x) {
    return exp_inner(x);
}

inline Decimal pow_inner(Decimal x, Decimal y)
{
    if (x.IsNotANumber() || x.IsOverflowed()) return x;
    if (y.IsZero() && !x.IsZero()) {
        Decimal one; one.SetDecimal(1, 0);
        return one;
    }
    if (!y.IsZero() && x.IsZero()) {
        Decimal zero; zero.SetDecimal(0, 0);
        return zero;
    }
    if (y.IsZero() && x.IsZero()) {
        Decimal nan; nan.SetNotANumber();
        return nan;
    }
    // Проверяем, является ли степень целой или полуцелой (N.0 или N.5)
    Decimal two_dec; two_dec.SetDecimal(2, 0);
    Decimal double_y = y * two_dec;

    if (double_y.IsInteger()) {
        Decimal one; one.SetDecimal(1, 0);

        // Выделяем целую часть степени (для 12.5 это 12)
        Decimal floor_y;
        floor_y.SetDecimal(y.IntegerPart(), 0);

        // 1. Считаем целую степень: x^12 через быстрое или последовательное умножение
        Decimal result_int = one;
        Decimal temp_y = floor_y.Abs();
        while (!temp_y.IsZero()) {
            result_int = result_int * x;
            if (result_int.IsOverflowed()) break;
            temp_y = temp_y - one;
        }
        if (!y.IsNegative() && result_int.IsOverflowed()) return result_int;
        if (y.IsNegative() && result_int.IsOverflowed()) {
            Decimal zero; zero.SetDecimal(0, 0);
            return zero;
        }
        // 2. Если есть половинка (.5), умножаем на честный Sqrt(x)
        if (!(y - floor_y).IsZero()) {
            result_int = result_int * Sqrt(x);
        }

        // 3. Обрабатываем отрицательную степень
        if (y.IsNegative()) {
            return one / result_int;
        }

        return result_int;
    }
    else {
        Decimal result = y;
        result = result * Ln(x);
        return Exp(result);
    }
}

inline Decimal Pow(Decimal x, Decimal y)
{
    return pow_inner(x, y);
}

inline Decimal::_Static Decimal::global;

}
