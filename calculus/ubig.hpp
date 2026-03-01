/**
 * @author nawww83@gmail.com
 * @brief Класс для арифметики N-битных беззнаковых целых чисел.
 */

#pragma once

#include <cstdint>
#include <bit>
#include <compare>
#include <algorithm>
#include <utility>
#include "u128.hpp" // generic

namespace bignum
{

/**
     * @brief Иерархический класс для длинных чисел.
     * @tparam ULOW Тип "половинки" (например, uint64_t или U128).
     */
template <typename ULOW>
class UBig
{
    ULOW mLow{0};
    ULOW mHigh{0};

public:
    // Используем универсальный способ определения разрядности
    static constexpr uint32_t HALF_WIDTH = static_cast<uint32_t>(bignum::generic::bit_size<ULOW>());
    static constexpr uint32_t WIDTH = HALF_WIDTH * 2;

    // Тип "половинки" для использования в generic-функциях
    using value_type = ULOW;

    // --- Конструкторы ---
    // --- Конструкторы ---
    constexpr UBig() noexcept = default;

    // Используем std::move, так как ULOW может быть тяжелым (например, U1024)
    constexpr UBig(ULOW low, ULOW high) noexcept
        : mLow(std::move(low)), mHigh(std::move(high)) {}

    constexpr UBig(ULOW low) noexcept
        : mLow(std::move(low)), mHigh(0) {}

    // Универсальный конструктор от примитива
    constexpr UBig(uint64_t val) noexcept
        : mLow(static_cast<ULOW>(val)), mHigh(0) {}

    // Конструктор от string_view (или char*)
    constexpr UBig(std::string_view s)
    {
        *this = fromString(s);
    }

    // --- Доступ ---
    [[nodiscard]] constexpr const ULOW &low() const noexcept { return mLow; }
    [[nodiscard]] constexpr const ULOW &high() const noexcept { return mHigh; }
    [[nodiscard]] constexpr ULOW &low() noexcept { return mLow; }
    [[nodiscard]] constexpr ULOW &high() noexcept { return mHigh; }

    // --- Сравнение ---
    constexpr bool operator==(const UBig &other) const noexcept
    {
        return mLow == other.mLow && mHigh == other.mHigh;
    }
    constexpr std::strong_ordering operator<=>(const UBig &other) const noexcept
    {
        if (auto cmp = mHigh <=> other.mHigh; cmp != 0)
            return cmp;
        return mLow <=> other.mLow;
    }

    // --- Сложение и вычитание ---

    // --- Сложение ---
    constexpr UBig &operator+=(const UBig &other) noexcept
    {
        ULOW old_low = mLow;
        mLow += other.mLow;

        // 1. Сначала учитываем перенос из младшей части
        if (mLow < old_low)
        {
            // Проверяем, можно ли вызвать ++ у объекта типа ULOW
            if constexpr (requires(ULOW &obj) { ++obj; })
            {
                ++mHigh;
            }
            else
            {
                mHigh += 1; // Fallback для совсем простых типов
            }
        }

        mHigh += other.mHigh;
        return *this;
    }

    // --- Вычитание ---
    constexpr UBig &operator-=(const UBig &other) noexcept
    {
        ULOW old_low = mLow;
        mLow -= other.mLow;

        // 1. Учитываем заем ПЕРЕД вычитанием основной части
        if (old_low < other.mLow)
        {
            // Проверяем, можно ли вызвать -- у объекта типа ULOW
            if constexpr (requires(ULOW &obj) { --obj; })
            {
                --mHigh;
            }
            else
            {
                mHigh -= 1;
            }
        }

        mHigh -= other.mHigh;
        return *this;
    }

    constexpr UBig operator+(const UBig &other) const noexcept { return UBig(*this) += other; }
    constexpr UBig operator-(const UBig &other) const noexcept { return UBig(*this) -= other; }

    // --- Инкремент / Декремент ---
    constexpr void inc() noexcept
    {
        ULOW old = mLow;
        // 1. Инкрементируем младшую часть
        if constexpr (requires(ULOW &l) { ++l; })
            ++mLow;
        else
            mLow += 1;

        // 2. Если произошел перенос (результат стал меньше старого)
        if (mLow < old)
        {
            // Инкрементируем старшую часть тем же способом
            if constexpr (requires(ULOW &h) { ++h; })
                ++mHigh;
            else
                mHigh += 1;
        }
    }

    constexpr void dec() noexcept
    {
        ULOW old = mLow;
        // 1. Декрементируем младшую часть
        if constexpr (requires(ULOW &l) { --l; })
            --mLow;
        else
            mLow -= 1;

        // 2. Если произошло заимствование (результат стал больше старого)
        if (mLow > old)
        {
            // Декрементируем старшую часть
            if constexpr (requires(ULOW &h) { --h; })
                --mHigh;
            else
                mHigh -= 1;
        }
    }

    constexpr UBig &operator++() noexcept
    {
        inc();
        return *this;
    }
    constexpr UBig &operator--() noexcept
    {
        dec();
        return *this;
    }

    /**
         * @brief Унарный минус (отрицание) для иерархического числа.
         * @return Результат операции (2^N - *this).
         */
    [[nodiscard]] constexpr UBig operator-() const noexcept
    {
        // 1. Побитовая инверсия обеих половинок (~x)
        // 2. Прибавление единицы к результату (+1)

        UBig res(~mLow, ~mHigh);
        res.inc(); // Используем наш эффективный метод инкремента с пробросом переноса
        return res;
    }

    // --- Умножение ---
    static constexpr UBig mult_ext(const ULOW &x, const ULOW &y) noexcept
    {
        constexpr int Q = WIDTH / 4; // Четверть ширины N-битного числа.
        ULOW MASK = (ULOW{1} << Q) - ULOW{1};
        ULOW x_low = x & MASK;
        ULOW y_low = y & MASK;
        ULOW x_high = x >> Q;
        ULOW y_high = y >> Q;
        ULOW t1 = x_low * y_low;
        ULOW t = t1 >> Q;
        ULOW t21 = x_low * y_high;
        ULOW q = t21 >> Q;
        ULOW p = t21 & MASK;
        ULOW t22 = x_high * y_low;
        ULOW s = t22 >> Q;
        ULOW r = t22 & MASK;
        ULOW t3 = x_high * y_high;
        UBig result{t1};
        ULOW div = (q + s) + ((p + r + t) >> Q);
        auto p1 = t21 << Q;
        auto p2 = t22 << Q;
        ULOW mod = p1 + p2;
        result.mLow += mod;
        result.mHigh += div;
        result.mHigh += t3;
        return result;
    }

    /**
         * @brief Возведение N/2-битного числа в квадрат с расширением до N-битного числа.
         * Оптимизировано по сравнению с обычным умножением (x * x).
         */
    static constexpr UBig square_ext(const ULOW &x) noexcept
    {
        constexpr uint32_t Q = WIDTH / 4; // Четверть ширины N-битного числа.
        ULOW MASK = (ULOW{1} << Q) - ULOW{1};
        ULOW x_low = x & MASK;
        ULOW x_high = x >> Q;
        ULOW t1 = x_low * x_low;
        ULOW t = t1 >> Q;
        ULOW t21 = x_low * x_high;
        ULOW q = t21 >> Q;
        ULOW p = t21 & MASK;
        ULOW t3 = x_high * x_high;
        UBig result{t1};
        ULOW div = (q << 1) + (((p << 1) + t) >> Q);
        auto p1 = t21 << Q;
        ULOW mod = p1 << 1;
        result.mLow += mod;
        result.mHigh += div;
        result.mHigh += t3;
        return result;
    }

    /**
         * @brief
         */
    constexpr UBig operator*(const UBig &other) const noexcept
    {
        UBig res = UBig::mult_ext(mLow, other.mLow);
        res.mHigh += (mLow * other.mHigh);
        res.mHigh += (mHigh * other.mLow);
        return res;
    }

    // --- Сдвиги ---
    constexpr UBig &operator<<=(uint32_t s) noexcept
    {
        if (s >= WIDTH)
        {
            mLow = mHigh = 0;
        }
        else if (s >= HALF_WIDTH)
        {
            mHigh = mLow << (s - HALF_WIDTH);
            mLow = 0;
        }
        else if (s > 0)
        {
            // Защищаем сдвиг: если s > 0, то (HALF_WIDTH - s) всегда < HALF_WIDTH
            mHigh = (mHigh << s) | (mLow >> (HALF_WIDTH - s));
            mLow <<= s;
        }
        return *this;
    }

    constexpr UBig &operator>>=(uint32_t s) noexcept
    {
        if (s >= WIDTH)
        {
            mLow = mHigh = 0;
        }
        else if (s >= HALF_WIDTH)
        {
            mLow = mHigh >> (s - HALF_WIDTH);
            mHigh = 0;
        }
        else if (s > 0)
        {
            mLow = (mLow >> s) | (mHigh << (HALF_WIDTH - s));
            mHigh >>= s;
        }
        return *this;
    }
    constexpr UBig operator<<(uint32_t s) const noexcept { return UBig(*this) <<= s; }
    constexpr UBig operator>>(uint32_t s) const noexcept { return UBig(*this) >>= s; }

    // --- Логика ---
    constexpr UBig &operator&=(const UBig &other) noexcept
    {
        mLow &= other.mLow;
        mHigh &= other.mHigh;
        return *this;
    }
    constexpr UBig &operator|=(const UBig &other) noexcept
    {
        mLow |= other.mLow;
        mHigh |= other.mHigh;
        return *this;
    }
    constexpr UBig &operator^=(const UBig &other) noexcept
    {
        mLow ^= other.mLow;
        mHigh ^= other.mHigh;
        return *this;
    }
    constexpr UBig operator&(const UBig &other) const noexcept { return UBig(*this) &= other; }
    constexpr UBig operator|(const UBig &other) const noexcept { return UBig(*this) |= other; }
    constexpr UBig operator^(const UBig &other) const noexcept { return UBig(*this) ^= other; }
    constexpr UBig operator~() const noexcept { return UBig(~mLow, ~mHigh); }

    /**
         * @brief Оператор деления.
         * @details Авторский метод деления двух "широких" чисел, состоящих из двух половинок - "узких" чисел.
         * Отсутствует "раскачка" алгоритма для "плохих" случаев деления: (A*w + B)/(1*w + D).
         * @return Частное от деления и остаток.
         */
    constexpr std::pair<UBig, UBig> operator/(const UBig &other) const
    {
        assert(other != UBig{0});

        if (*this < other)
            return {UBig{0}, *this};

        // Оптимизация для "узкого" делителя
        if (other.mHigh == ULOW{0})
        {
            auto [q, r] = *this / other.mLow;
            return {q, UBig{r}};
        }

        // 1. Начальная оценка частного и остатка по старшим половинам
        auto [Q, R] = divide_as_pair(mHigh, other.mHigh);

        // 2. Учет вклада младших половин (Delta-коррекция)
        constexpr ULOW MAX_V = ULOW::max();
        const ULOW Delta = MAX_V - other.mLow;
        const UBig DeltaQ = mult_ext(Delta, Q);

        const UBig sum_1 = UBig{0, R} + DeltaQ;
        const UBig sub_val = UBig{0, Q};
        const bool is_neg = sum_1 < sub_val;

        UBig W1 = is_neg ? (sub_val - sum_1) : (sum_1 - sub_val);

        // 3. Вычисление поправочного коэффициента Quotient
        const ULOW C1 = (other.mHigh < MAX_V) ? (other.mHigh + 1ull) : MAX_V;
        const ULOW W2 = MAX_V - get_quotient(Delta, C1);

        UBig Quotient = get_quotient(get_quotient(W1, W2), C1);
        if (is_neg)
            Quotient = -Quotient;

        // 4. Формирование итогового частного (только младшая часть, т.к. private case)
        UBig result = UBig{Q} + Quotient - UBig{is_neg ? 1ull : 0ull};

        // 5. Финальная коррекция (обычно 0-1 итерация)
        // Используем N для вычисления точного остатка
        UBig N = other * result.mLow;

        // Если переоценили (result > истинного)
        while (*this < N)
        {
            result.dec();
            N -= other;
        }

        UBig Remainder = *this - N;

        // Если недооценили (result < истинного)
        while (Remainder >= other)
        {
            Remainder -= other;
            result.inc();
        }

        return {result, Remainder};
    }

    /**
         * @brief Оператор деления "широкого" числа на "половинку" (UBig / ULOW).
         * @return std::pair<UBig, ULOW> {Частное Q, Остаток R}.
         */
    constexpr std::pair<UBig, ULOW> operator/(const ULOW &other) const
    {
        assert(other != ULOW{0});

        if (other == ULOW{1})
            return {*this, ULOW{0}};
        if (mHigh == ULOW{0})
        {
            auto [q, r] = divide_as_pair( mLow, other);
            return {UBig(q), r};
        }

        // --- Авторский итеративный метод ---
        UBig X = *this;
        const UBig X_old = X;
        UBig Q{0};
        ULOW R{0};

        // Вычисляем обратную величину через вашу обобщенную функцию
        std::pair<ULOW, ULOW> rcp = bignum::generic::reciprocal_and_extend(other);
        const ULOW rcp_compl = other - rcp.second;

        // Ускорение сходимости (выбор направления коррекции)
        const bool make_inverse = rcp_compl < rcp.second;
        if (make_inverse)
            ++(rcp.first);

        for (;;)
        {
            const bool x_has_high = (X.mHigh != ULOW{0});

            // 1. Накапливаем частное
            if (x_has_high)
                Q += mult_ext(X.mHigh, rcp.first);

            // Добавляем результат деления младшей части
            Q += UBig(get_quotient(X.mLow, other));

            // 2. Считаем остаток через вашу функцию adder
            const ULOW carry = bignum::generic::smart_remainder_adder(R, X.mLow, other, rcp.second);
            Q += UBig(carry);

            // 3. Обновляем X для следующей итерации
            if (x_has_high)
            {
                X = mult_ext(X.mHigh, make_inverse ? rcp_compl : rcp.second);
            }
            else
            {
                X = UBig{0};
            }

            if (X != UBig{0})
            {
                if (make_inverse)
                {
                    Q = -Q;
                    R = other - R;
                }
                continue;
            }

            // 4. Финальная коррекция знака
            if (Q > X_old)
            {
                Q = -Q;
                R = other - R;
                if (R == other)
                {
                    R = ULOW{0};
                    Q.inc();
                }
            }
            break;
        }
        return {Q, R};
    }

    /**
         * @brief Количество ведущих нулей.
         */
    [[nodiscard]] constexpr uint32_t countl_zero() const noexcept
    {
        // Если старшая часть не ноль, считаем нули в ней.
        // Если ноль — прибавляем всю ширину старшей части к нулям в младшей.
        if (mHigh != 0)
        {
            if constexpr (requires(ULOW l) { l.countl_zero(); })
                return mHigh.countl_zero();
            else
                return std::countl_zero(static_cast<uint64_t>(mHigh));
        }
        uint32_t l_zeros = 0;
        if constexpr (requires(ULOW l) { l.countl_zero(); })
            l_zeros = mLow.countl_zero();
        else
            l_zeros = std::countl_zero(static_cast<uint64_t>(mLow));

        return HALF_WIDTH + l_zeros;
    }

    /**
         * @brief Количество завершающих нулей.
         */
    [[nodiscard]] constexpr uint32_t countr_zero() const noexcept
    {
        if (mLow != 0)
        {
            if constexpr (requires(ULOW l) { l.countr_zero(); })
                return mLow.countr_zero();
            else
                return std::countr_zero(static_cast<uint64_t>(mLow));
        }
        uint32_t r_zeros = 0;
        if constexpr (requires(ULOW l) { l.countr_zero(); })
            r_zeros = mHigh.countr_zero();
        else
            r_zeros = std::countr_zero(static_cast<uint64_t>(mHigh));

        return HALF_WIDTH + r_zeros;
    }

    /**
         * @brief Количество установленных бит (единиц).
         */
    [[nodiscard]] constexpr uint32_t popcount() const noexcept
    {
        auto pc = [](const ULOW &val)
        {
            if constexpr (requires { val.popcount(); })
                return val.popcount();
            else
                return std::popcount(static_cast<uint64_t>(val));
        };
        return pc(mHigh) + pc(mLow);
    }

    /**
         * @brief Минимальное количество бит для представления числа.
         */
    [[nodiscard]] constexpr uint32_t bit_width() const noexcept
    {
        return WIDTH - countl_zero();
    }

    /**
         * @brief Проверка, является ли число степенью двойки.
         */
    [[nodiscard]] constexpr bool has_single_bit() const noexcept
    {
        return popcount() == 1;
    }

    /**
         * @brief Округление вниз до ближайшей степени двойки.
         */
    [[nodiscard]] constexpr UBig bit_floor() const noexcept
    {
        if (*this == 0)
            return UBig{0, 0};
        return UBig{1, 0} << (bit_width() - 1);
    }

    /**
         * @brief Возвращает максимально возможное значение для данного типа UBig.
         * Рекурсивно заполняет все уровни вложенности единицами.
         */
    [[nodiscard]] static constexpr UBig max() noexcept
    {
        if constexpr (requires { ULOW::max(); })
        {
            // Если ULOW — это ваш класс (U128, U256...), вызываем его max()
            return UBig{ULOW::max(), ULOW::max()};
        }
        else
        {
            // Если ULOW — это примитив (uint64_t)
            return UBig{std::numeric_limits<ULOW>::max(), std::numeric_limits<ULOW>::max()};
        }
    }

    /**
         * @brief Возвращает минимальное значение (0).
         */
    [[nodiscard]] static constexpr UBig min() noexcept
    {
        if constexpr (requires { ULOW::min(); })
        {
            return UBig{ULOW::min(), ULOW::min()};
        }
        else
        {
            return UBig{ULOW{0}, ULOW{0}};
        }
    }

    /**
         * @brief Вспомогательный метод для получения только частного.
         */
    static constexpr UBig get_quotient(const UBig &a, const ULOW &b)
    {
        return (a / b).first;
    }

    /**
         * @brief Вспомогательный метод для получения только частного.
         */
    static constexpr ULOW get_quotient(const ULOW &a, const ULOW &b)
    {
        return bignum::generic::div_rem(a, b).first;
    }

    /**
         * @brief Вспомогательный метод для получения частного и остатка в виде пары.
         */
    static constexpr auto divide_as_pair(const ULOW &a, const ULOW &b)
    {
        return bignum::generic::div_rem(a, b);
    }

    /**
         * @brief Рекурсивное преобразование в десятичную строку.
         * Использует деление всего числа на 10^18 для точности.
         */
    [[nodiscard]] std::string toString() const
    {
        if (mHigh == ULOW{0} && mLow == ULOW{0})
            return "0";

        UBig copy = *this;
        std::string final_res = "";

        // Используем 10^18 как базу. Она гарантированно влезает в uint64_t
        // и позволяет удобно дополнять нулями.
        const uint64_t base_val = 1000000000000000000ULL;
        const ULOW base_low{base_val};

        while (copy > UBig{0})
        {
            // Вызываем ваше половинчатое деление (UBig / ULOW)
            // Оно возвращает пару {UBig quotient, ULOW remainder}
            auto [q, r] = copy / base_low;

            // Превращаем остаток r в строку.
            // Если ULOW - это u64, используем std::to_string.
            // Если ULOW - это U128, вызываем r.toString().
            std::string part;
            if constexpr (std::is_same_v<ULOW, uint64_t>)
            {
                part = std::to_string(r);
            }
            else
            {
                part = r.toString();
            }

            // Если это не последний (самый старший) кусок,
            // дополняем его ведущими нулями до 18 знаков.
            if (q > UBig{0})
            {
                if (part.length() < 18)
                {
                    part = std::string(18 - part.length(), '0') + part;
                }
            }

            final_res = part + final_res;
            copy = q;
        }

        return final_res;
    }

    static constexpr UBig fromString(std::string_view s)
    {
        if (s.empty())
            return UBig{0};

        size_t p = 0;
        while (p < s.length() && (s[p] == ' ' || s[p] == '+'))
            p++;

        UBig res{0};

        // Для U256 (где ULOW=U128) мы можем эффективно
        // обрабатывать строки блоками по 18 цифр,
        // используя уже готовый оператор * и + самого UBig.
        while (p < s.length())
        {
            uint64_t block_val = 0;
            uint64_t multiplier = 1;

            size_t end = std::min(p + 18, s.length());
            for (; p < end; ++p)
            {
                if (s[p] < '0' || s[p] > '9')
                    break;
                block_val = block_val * 10 + (s[p] - '0');
                multiplier *= 10;
            }

            // Используем арифметику UBig: res = res * multiplier + block_val
            // multiplier автоматически преобразуется в UBig через конструктор от u64
            res = (res * UBig(multiplier)) + UBig(block_val);

            if (p < s.length() && (s[p] < '0' || s[p] > '9'))
                break;
        }
        return res;
    }
};

} // namespace bignum
