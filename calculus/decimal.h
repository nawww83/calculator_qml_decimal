#pragma once

#include <cmath>   // std::pow
#include <cassert> // assert
#include <array>   // std::array
#include <cstring> // std::memcpy
#include <string>  // std::string
#include <climits> // CHAR_BIT
#include <algorithm> // std::clamp


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

constexpr char DIGITS[10] {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9'};

/**
 * @brief Количество цифр числа.
 * @param x Число.
 * @return Количество цифр, минимум 1.
 */
inline int num_of_digits(long long x) {
    int i = 0;
    while (x) {
        x /= 10;
        i++;
    }
    return i + (i == 0);
}

/**
 * @brief Есть ли переполнение при умножении двух чисел.
 * @param x Первый аргумент.
 * @param y Второй аргумент.
 * @return Да/нет.
 */
inline bool is_mult_overflow_ll(long long x, long long y) {
    if (x != 0 && y != 0) {
        const long long z = (unsigned long long)(x)*(unsigned long long)(y);
        long long try_x = z / y;
        long long try_y = z / x;
        return try_x != x || try_y != y;
    }
    return false;
}

/**
 * @brief Есть ли переполнение при сложении двух чисел.
 * @param x Первый аргумент.
 * @param y Второй аргумент.
 * @return Да/нет.
 */
inline bool is_add_overflow_ll(long long x, long long y) {
    constexpr int bit_size = sizeof(unsigned long long) * CHAR_BIT;
    unsigned long long z = (unsigned long long)(x) + (unsigned long long)(y);
    const auto max_value = (1ull << (bit_size - 1)) - 1;
    return (x < 0 && y < 0 && z <= max_value) || (x > 0 && y > 0 && z > max_value);
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
 * основанного на 64-битных целой и дробной частей.
 */
class Vector64 {
    /**
     * @brief Наибольшее количество хранимых символов.
     */
    static constexpr int MAX_SIZE = 56;

    std::array<char, MAX_SIZE + 1> mBuffer{};

    /**
     * @brief Значимый размер буфера.
     */
    int mRealSize = 0;

    /**
     * @brief Вернуть размер, ограниченный отрезком [0, MAX_SIZE].
     * @param size Размер.
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
    explicit Vector64() = default;

    /**
     * @brief Конструктор.
     * @param str
     */
    Vector64(const std::string& str) noexcept {
        FillData(str.data(), str.size());
    }

    Vector64(std::string&& str) = delete;

    /**
     * @brief Конструктор копирования.
     * @param other
     */
    Vector64(const Vector64& other) noexcept {
        FillData(other.mBuffer.data(), other.RealSize());
    }

    Vector64(Vector64&& other) = delete;

    Vector64& operator=(const std::string& str) {
        FillData(str.data(), str.size());
        return *this;
    }

    Vector64& operator=(const Vector64& other) {
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
    }

    /**
     * @brief Получить строковое представления числа.
     */
    auto GetStringView() const noexcept {
        return std::string_view(mBuffer.data(), mRealSize);
    }
};

#ifdef PRECISION_W2
template <int width=2> // precision
#endif
#ifdef PRECISION_W3
template <int width=3> // precision
#endif
#ifdef PRECISION_W4
template <int width=4> // precision
#endif
class Decimal {
    /**
     * @brief Целое представление числа.
     */
    long long mInteger = 0;

    /**
     * @brief Числитель дробной части числа.
     */
    long long mNominator = 0;

    /**
     * @brief Знаменатель дробной части числа.
     */
    long long mDenominator = std::pow(10ll, width);

    /**
     * @brief Строковое представление числа.
     */
    Vector64 mStringRepresentation;

    /**
     * @brief Преобразовать компоненты Decimal в строковое представление числа.
     */
    void TransformToString() {
        if (IsOverflowed() || IsNotANumber()) {
            mStringRepresentation = "";
            return;
        }
        long long r = mInteger;
        const int sign = IsNegative();
        if (std::abs(mNominator) >= mDenominator) {
            r = sign == 0 ? r + mNominator / mDenominator : r - mNominator / mDenominator;
            mNominator = (mNominator >= 0) ? mNominator % mDenominator : -((-mNominator) % mDenominator);
        }
        long long fraction = (std::pow(10ll, width) * (mNominator < 0 ? -mNominator : mNominator)) / mDenominator;
        const int required_length = (num_of_digits(r) + 1 + width) + (sign == 0 ? 0 : 1); // Целая часть, разделитель, дробная часть (precision), знак.
        assert(required_length <= Vector64::MaxSize());
        assert(required_length > 0);
        mStringRepresentation.Resize(required_length);
        if (sign != 0) {
            mStringRepresentation[0] = chars::minus_sign;
        }
        if (r == 0) {
            mStringRepresentation[required_length - width - 2] = chars::zero;
        }
        r = r < 0 ? -r : r;
        if (r == std::numeric_limits<long long>::min()) {
            mInteger = -1;
            mNominator = -1;
            mDenominator = std::pow(10ll, width);
            mStringRepresentation = "";
            return;
        }
        for (int i = 0; r != 0 ; i++) {
            mStringRepresentation[required_length - width - 2 - i] = DIGITS[r % 10ll];
            r /= 10ll;
        }
        mStringRepresentation[required_length - 1 - width] = chars::separator;
        for (int i = 0; i < width; i++) {
            mStringRepresentation[required_length - 1 - i] = DIGITS[fraction % 10ll];
            fraction /= 10ll;
        }
    }

    /**
     * @brief Преобразовать строковое представление числа в компоненты Decimal.
     */
    void TransformToDecimal() {
        if (mStringRepresentation.RealSize() < 1) {
            mInteger = 0;
            mNominator = 0;
            mDenominator = 0;
            return;
        }
        const int the_sign = mStringRepresentation[0] == chars::minus_sign ? 1 : 0;
        int current_index = the_sign == 0 ? 0 : 1;
        char digit = mStringRepresentation[current_index];
        mInteger = (long long)undigits(digit); // В случае некорректного символа возвращается ноль.
        current_index++;
        digit = mStringRepresentation[current_index];
        auto hasMultOverflow = [](long long x, long long y) -> bool {
            return x > std::numeric_limits<long long>::max() / y;
        };
        auto hasAddOverflow = [](long long x, long long y) -> bool {
            return x > 0 && y > std::numeric_limits<long long>::max() - x;
        };
        while ((digit != chars::separator && digit != chars::alternative_separator) && digit != chars::null) {
            if (hasMultOverflow(mInteger, 10ll)) {
                mInteger = -1;
                mNominator = -1;
                mDenominator = std::pow(10ll, width);
                return;
            }
            mInteger *= 10ll;
            if (hasAddOverflow(undigits(digit), mInteger)) {
                mInteger = -1;
                mNominator = -1;
                mDenominator = std::pow(10ll, width);
                return;
            }
            mInteger += (long long)undigits(digit);
            current_index++;
            digit = mStringRepresentation[current_index];
        }
        if (mInteger == std::numeric_limits<long long>::min()) {
            mInteger = -1;
            mNominator = -1;
            mDenominator = std::pow(10ll, width);
            return;
        }
        mInteger = the_sign == 0 ? mInteger : -mInteger;
        current_index++;
        digit = mStringRepresentation[current_index];
        mDenominator = std::pow(10ll, width);
        mNominator = (long long)undigits(digit);
        current_index++;
        digit = mStringRepresentation[current_index];
        const int length = mStringRepresentation.RealSize();
        int idx_width = 1;
        while (current_index < length) {
            if (idx_width >= width) { // Слишком много цифр после запятой.
                break;
            }
            mNominator *= 10ll;
            mNominator += (long long)undigits(digit);
            current_index++;
            digit = mStringRepresentation[current_index];
            idx_width++;
        }
        while (idx_width < width) { // Добавление нулей. Например 4,5 => 4,50 при width = 2.
            mNominator *= 10ll;
            mNominator += 0ll;
            idx_width++;
        }
        if (mInteger == 0 && the_sign != 0) { // Если целая часть равна нулю, то знак храним в числителе.
            mNominator = -mNominator;
        }
    }
public:
    explicit Decimal() {
        TransformToString();
    }

    /**
     * @brief Установить Decimal число покомпонентно.
     * @param integer Целая часть
     * @param nominator Числитель дробной части.
     * @param denominator Знаменатель дробной части.
     */
    void SetDecimal(long long integer, long long nominator, long long denominator) {
        mInteger = integer;
        mNominator = nominator;
        mDenominator = denominator;
        // Сделать прямое-обратное преобразования для формирования знаменателя 10^width.
        TransformToString();
        TransformToDecimal();
    }

    /**
     * @brief Число целое.
     * @return Да/нет.
     */
    bool IsInteger() const {
        return mNominator == 0;
    }

    /**
     * @brief Произошло переполнение при выполнении операции.
     * @return Да/нет.
     */
    bool IsOverflowed() const {
        return mDenominator <= 0 || (mInteger < 0 && mNominator < 0);
    }

    /**
     * @brief Не число.
     * @return Да/нет.
     */
    bool IsNotANumber() const {
        return mDenominator == 0 && mInteger == 0 && mNominator == 0;
    }

    /**
     * @brief Число отрицательное в узком (сильном) смысле.
     * Здесь число по модулю не меньше единицы и знак хранится в целой части.
     * @return Да/нет.
     */
    bool IsStrongNegative() const {
        return mInteger < 0;
    }

    /**
     * @brief Число отрицательное в широком (слабом) смысле.
     * Здесь число по модулю меньше единице и знак хранится в числителе.
     * @return Да/нет.
     */
    bool IsWeakNegative() const {
        return mInteger == 0 && mNominator < 0;
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
        return mInteger == 0 && mNominator == 0;
    }

    auto ValueAsStringView() const {
        return mStringRepresentation.GetStringView();
    }

    /**
     * @brief Вернуть представление числа в виде double.
     * @return
     */
    double ValueAsDouble() const {
        const double the_sign = mInteger < 0 ? -1. : 1.;
        const double value = double(mNominator) / double(mDenominator);
        return mInteger != 0 ? double(mInteger) + the_sign * value : value;
    }

    auto IntegerPart() const {
        return mInteger;
    }

    auto Nominator() const {
        return mNominator;
    }

    auto Denominator() const {
        return mDenominator;
    }

    /**
     * @brief Установить строковое представление числа.
     * @param s Строковое представление числа.
     */
    void SetStringRepresentation(std::string& s) {
        mStringRepresentation = Vector64(s);
        TransformToDecimal();
        TransformToString();
    }

    Decimal operator+(const Decimal& other) const {
        Decimal result{};
        const int neg1 = IsNegative();
        const int neg2 = other.IsNegative();
        bool is_overflow1 = is_add_overflow_ll(mInteger, other.mInteger);
        bool is_overflow2 = is_add_overflow_ll(mNominator, other.mNominator);
        if (is_overflow1 || is_overflow2) {
            result.SetDecimal(-1, -1, mDenominator);
            return result;
        }
        auto sum = mInteger + other.mInteger;
        const bool equal_fractions = (mDenominator == other.mDenominator);
        assert(equal_fractions);
        auto f = std::abs(mNominator) + std::abs(other.mNominator);
        const bool have_differ_signs = neg1 ^ neg2;
        if (neg1 && !neg2) {
            f = -std::abs(mNominator) + std::abs(other.mNominator);
        }
        if (!neg1 && neg2) {
            f = std::abs(mNominator) - std::abs(other.mNominator);
        }
        if (have_differ_signs) {
            if (f < 0 && sum < 0) {
                f = -f;
            } else
                if (f < 0 && sum > 0) {
                    f += mDenominator;
                    sum -= 1;
                } else
                    if (f > 0 && sum < 0) {
                        f -= mDenominator;
                        sum += 1;
                        if (sum != 0) {
                            f = std::abs(f);
                        }
                    }
        }
        if (neg1 && neg2) {
            if (sum == 0) {
                f = -f;
            }
        }
        result.SetDecimal(sum, f, mDenominator);
        return result;
    }

    Decimal& operator+=(const Decimal& other) {
        return *this + other;
    }

    Decimal operator-(const Decimal& other) const {
        Decimal res{};
        res.SetDecimal(-other.mInteger, (other.mInteger != 0) ? other.mNominator : -other.mNominator, other.mDenominator);
        return res + *this;
    }

    Decimal& operator-=(const Decimal& other) {
        return *this - other;
    }

    Decimal operator*(const Decimal& other) const {
        Decimal result{};
        const bool neg1 = IsNegative();
        const bool neg2 = other.IsNegative();
        const bool overflow1 = is_mult_overflow_ll(mInteger, other.mNominator);
        const bool overflow2 = is_mult_overflow_ll(other.mInteger, mNominator) ;
        const bool overflow3 = is_mult_overflow_ll(mInteger, mNominator);
        const bool overflow4 = is_mult_overflow_ll(other.mInteger, other.mNominator);
        const bool overflow5 = is_mult_overflow_ll(mNominator, other.mNominator);
        const bool overflow6 = is_mult_overflow_ll(mInteger, other.mInteger);
        const bool equal_fractions = (mDenominator == other.mDenominator);
        assert(equal_fractions);
        const bool all_integers = (mNominator == 0 && other.mNominator == 0);
        if (overflow6) {
            result.SetDecimal(-1, -1, mDenominator);
            return result;
        }
        auto integer_part = mInteger * other.mInteger;
        auto fraction_part = integer_part * 0;
        if (all_integers) {
            result.SetDecimal(integer_part, fraction_part, mDenominator);
            return result;
        }
        const bool left_integer = ((mNominator == 0) && (other.mNominator != 0));
        if (left_integer) {
            if (overflow1) {
                result.SetDecimal(-1, -1, mDenominator);
                return result;
            }
            integer_part += ((neg1 ^ neg2) ? -1ll : 1ll) * (std::abs(mInteger) * std::abs(other.mNominator)) / mDenominator;
            fraction_part = (std::abs(mInteger) * std::abs(other.mNominator)) % mDenominator;
            if (neg1 ^ neg2) {
                fraction_part = integer_part == 0 ? -fraction_part : fraction_part;
            }
            result.SetDecimal(integer_part, fraction_part, mDenominator);
            return result;
        }
        const bool right_integer = ((mNominator != 0) && (other.mNominator == 0));
        if (right_integer) {
            if (overflow2) {
                result.SetDecimal(-1, -1, mDenominator);
                return result;
            }
            integer_part += ((neg1 ^ neg2) ? -1ll : 1ll) * (std::abs(mNominator)*std::abs(other.mInteger)) / mDenominator;
            fraction_part = (std::abs(mNominator) * std::abs(other.mInteger)) % mDenominator;
            if (neg1 ^ neg2) {
                fraction_part = integer_part == 0 ? -fraction_part : fraction_part;
            }
            result.SetDecimal(integer_part, fraction_part, mDenominator);
            return result;
        }
        if (overflow1 || overflow2 || overflow3 || overflow4 || overflow5 || overflow6) {
            result.SetDecimal(-1, -1, mDenominator);
            return result;
        }
        if (!neg1 && !neg2) {
            integer_part += (mInteger*other.mNominator + mNominator*other.mInteger + (mNominator*other.mNominator)/mDenominator) / mDenominator;
            fraction_part = (mInteger*other.mNominator + mNominator*other.mInteger + (mNominator*other.mNominator)/mDenominator) % mDenominator;
        }
        if (neg1 && neg2) {
            const int neg1_strong = IsStrongNegative();
            const int neg2_strong = other.IsStrongNegative();
            const int neg1_weak = IsWeakNegative();
            const int neg2_weak = other.IsWeakNegative();
            if (neg1_strong && neg2_strong) {
                integer_part += (std::abs(mInteger)*other.mNominator + std::abs(other.mInteger)*mNominator + \
                                                    (mNominator*other.mNominator)/mDenominator) / mDenominator;
                fraction_part = (std::abs(mInteger)*other.mNominator + std::abs(other.mInteger)*mNominator + \
                                                    (mNominator*other.mNominator)/mDenominator) % mDenominator;

            }
            if (neg1_weak && neg2_strong) {
                integer_part += (std::abs(other.mInteger)*std::abs(mNominator) + (std::abs(mNominator)*other.mNominator)/mDenominator) / mDenominator;
                fraction_part = (std::abs(other.mInteger)*std::abs(mNominator) + (std::abs(mNominator)*other.mNominator)/mDenominator) % mDenominator;
            }
            if (neg1_strong && neg2_weak) {
                integer_part += (std::abs(mInteger)*std::abs(other.mNominator) + (mNominator*std::abs(other.mNominator))/mDenominator) / mDenominator;
                fraction_part = (std::abs(mInteger)*std::abs(other.mNominator) + (mNominator*std::abs(other.mNominator))/mDenominator) % mDenominator;
            }
            if (neg1_weak && neg2_weak) {
                integer_part += ((std::abs(mNominator)*std::abs(other.mNominator))/mDenominator) / mDenominator;
                fraction_part = ((std::abs(mNominator)*std::abs(other.mNominator))/mDenominator) % mDenominator;
            }
        }
        if (neg1 && !neg2) {
            const int neg1_strong = IsStrongNegative();
            const int neg1_weak = IsWeakNegative();
            if (neg1_strong) {
                integer_part = std::abs(integer_part) + (std::abs(mInteger)*other.mNominator + other.mInteger*mNominator + \
                                                                            (mNominator*other.mNominator)/mDenominator) / mDenominator;
                fraction_part = (std::abs(mInteger)*other.mNominator + other.mInteger*mNominator + \
                                                                            (mNominator*other.mNominator)/mDenominator) % mDenominator;
                integer_part = -integer_part;
                fraction_part = integer_part == 0 ? -fraction_part : fraction_part;
            }
            if (neg1_weak) {
                integer_part = (other.mInteger*std::abs(mNominator) + (std::abs(mNominator)*other.mNominator)/mDenominator) / mDenominator;
                fraction_part = (other.mInteger*std::abs(mNominator) + (std::abs(mNominator)*other.mNominator)/mDenominator) % mDenominator;
                integer_part = -integer_part;
                fraction_part = integer_part == 0 ? -fraction_part : fraction_part;
            }
        }
        if (!neg1 && neg2) {
            const int neg2_strong = other.IsStrongNegative();
            const int neg2_weak = other.IsWeakNegative();
            if (neg2_strong) {
                integer_part = std::abs(integer_part) + (mInteger*other.mNominator + std::abs(other.mInteger)*mNominator + \
                                                                (mNominator*other.mNominator)/mDenominator) / mDenominator;
                fraction_part = (mInteger*other.mNominator + std::abs(other.mInteger)*mNominator + \
                                                                (mNominator*other.mNominator)/mDenominator) % mDenominator;
                integer_part = -integer_part;
                fraction_part = integer_part == 0 ? -fraction_part : fraction_part;
            }
            if (neg2_weak) {
                integer_part = (mInteger*std::abs(other.mNominator) + (mNominator*std::abs(other.mNominator))/mDenominator) / mDenominator;
                fraction_part = (mInteger*std::abs(other.mNominator) + (mNominator*std::abs(other.mNominator))/mDenominator) % mDenominator;
                integer_part = -integer_part;
                fraction_part = integer_part == 0 ? -fraction_part : fraction_part;
            }
        }
        result.SetDecimal(integer_part, fraction_part, mDenominator);
        return result;
    }

    Decimal& operator*=(const Decimal& other) {
        return *this * other;
    }

    Decimal operator/(const Decimal& other) const {
        Decimal result{};
        const bool neg1 = IsNegative();
        const bool neg2 = other.IsNegative();
        const bool equal_fractions = (mDenominator == other.mDenominator);
        assert(equal_fractions);
        const bool all_integers = (mNominator == 0) && (other.mNominator == 0);
        if (all_integers) {
            auto integer_part = std::abs(mInteger) / std::abs(other.mInteger);
            auto fraction_part = std::abs(mInteger) % std::abs(other.mInteger);
            if (neg1 ^ neg2) {
                integer_part = integer_part != 0 ? -integer_part : integer_part;
                fraction_part = integer_part == 0 ? -fraction_part : fraction_part;
            }
            result.SetDecimal(integer_part, fraction_part, std::abs(other.mInteger));
            return result;
        }
        const bool denominator_is_integer = (other.mNominator == 0) && (other.mInteger != 0);
        if (denominator_is_integer) {
            auto integer_part = std::abs(mInteger) / std::abs(other.mInteger) + \
                                        (std::abs(mInteger) % std::abs(other.mInteger)) / std::abs(other.mInteger);
            auto fraction_part = (std::abs(mNominator) + (std::abs(mInteger) % std::abs(other.mInteger)) * mDenominator) / std::abs(other.mInteger);
            if (neg1 ^ neg2) {
                integer_part = integer_part != 0 ? -integer_part : integer_part;
                fraction_part = integer_part == 0 ? -fraction_part : fraction_part;
            }
            result.SetDecimal(integer_part, fraction_part, mDenominator);
            return result;
        }
        if ( is_mult_overflow_ll(mInteger, mDenominator) ) {
            result.SetDecimal(-1, -1, mDenominator);
            return result;
        }
        if (!neg1 && !neg2) {
            auto integer_part = (mInteger*mDenominator + mNominator) / (other.mInteger*mDenominator + other.mNominator);
            auto fraction_part = (mInteger*mDenominator + mNominator) % (other.mInteger*mDenominator + other.mNominator);
            result.SetDecimal(integer_part, fraction_part, other.mInteger*mDenominator + other.mNominator);
        }
        if (neg1 && neg2) {
            const int neg1_strong = IsStrongNegative();
            const int neg2_strong = other.IsStrongNegative();
            const int neg1_weak = IsWeakNegative();
            const int neg2_weak = other.IsWeakNegative();
            if (neg1_strong && neg2_strong) {
                auto integer_part = (std::abs(mInteger)*mDenominator + mNominator) / (std::abs(other.mInteger)*mDenominator + other.mNominator);
                auto fraction_part = (std::abs(mInteger)*mDenominator + mNominator) % (std::abs(other.mInteger)*mDenominator + other.mNominator);
                result.SetDecimal(integer_part, fraction_part, std::abs(other.mInteger)*mDenominator + other.mNominator);
            }
            if (neg1_weak && neg2_weak) {
                auto integer_part = mNominator / other.mNominator;
                auto fraction_part = std::abs(mNominator) % std::abs(other.mNominator);
                result.SetDecimal(integer_part, fraction_part, std::abs(other.mNominator));
            }
            if (neg1_strong && neg2_weak) {
                auto integer_part = (std::abs(mInteger)*mDenominator + mNominator) / std::abs(other.mNominator);
                auto fraction_part = (std::abs(mInteger)*mDenominator + mNominator) % std::abs(other.mNominator);
                result.SetDecimal(integer_part, fraction_part, std::abs(other.mNominator));
            }
            if (neg1_weak && neg2_strong) {
                auto integer_part = std::abs(mNominator) / (std::abs(other.mInteger)*mDenominator + other.mNominator);
                auto fraction_part = std::abs(mNominator) % (std::abs(other.mInteger)*mDenominator + other.mNominator);
                result.SetDecimal(integer_part, fraction_part, std::abs(other.mInteger)*mDenominator + other.mNominator);
            }
        }
        if (neg1 && !neg2) {
            const int neg1_strong = IsStrongNegative();
            const int neg1_weak = IsWeakNegative();
            if (neg1_strong) {
                auto integer_part = (std::abs(mInteger)*mDenominator + mNominator) / (other.mInteger*mDenominator + other.mNominator);
                auto fraction_part = (std::abs(mInteger)*mDenominator + mNominator) % (other.mInteger*mDenominator + other.mNominator);
                integer_part = -integer_part;
                fraction_part = integer_part == 0 ? -fraction_part : fraction_part;
                result.SetDecimal(integer_part, fraction_part, other.mInteger*mDenominator + other.mNominator);
            }
            if (neg1_weak) {
                auto integer_part = std::abs(mNominator) / (other.mInteger*mDenominator + other.mNominator);
                auto fraction_part = std::abs(mNominator) % (other.mInteger*mDenominator + other.mNominator);
                integer_part = -integer_part;
                fraction_part = integer_part == 0 ? -fraction_part : fraction_part;
                result.SetDecimal(integer_part, fraction_part, other.mInteger*mDenominator + other.mNominator);
            }
        }
        if (!neg1 && neg2) {
            const int neg2_strong = other.IsStrongNegative();
            const int neg2_weak = other.IsWeakNegative();
            if (neg2_strong) {
                auto integer_part = (mInteger*mDenominator + mNominator) / (std::abs(other.mInteger)*mDenominator + other.mNominator);
                auto fraction_part = (mInteger*mDenominator + mNominator) % (std::abs(other.mInteger)*mDenominator + other.mNominator);
                integer_part = -integer_part;
                fraction_part = integer_part == 0 ? -fraction_part : fraction_part;
                result.SetDecimal(integer_part, fraction_part, std::abs(other.mInteger)*mDenominator + other.mNominator);
            }
            if (neg2_weak) {
                auto integer_part = (mInteger*mDenominator + mNominator) / std::abs(other.mNominator);
                auto fraction_part = (mInteger*mDenominator + mNominator) % std::abs(other.mNominator);
                integer_part = -integer_part;
                fraction_part = integer_part == 0 ? -fraction_part : fraction_part;
                result.SetDecimal(integer_part, fraction_part, std::abs(other.mNominator));
            }
        }
        return result;
    }

    Decimal& operator/=(const Decimal& other) {
        return *this / other;
    }
};

/**
 * @brief Оператор сравнения чисел Decimal. Ориентируется на строковое представление чисел.
 * @param lhs Первое число.
 * @param rhs Второе число.
 * @return Равны/неравны.
 */
bool inline operator==(const Decimal<>& lhs, const Decimal<>& rhs) {
    return lhs.ValueAsStringView() == rhs.ValueAsStringView();
}

}
