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
    static constexpr uint32_t HALF_WIDTH = sizeof(ULOW) * 8;
    static constexpr uint32_t WIDTH = HALF_WIDTH * 2;

    // --- Конструкторы ---
    constexpr UBig() noexcept = default;
    constexpr UBig(ULOW low, ULOW high) noexcept : mLow(std::move(low)), mHigh(std::move(high)) {}
    constexpr UBig(ULOW low) noexcept : mLow(std::move(low)), mHigh(0) {}
    constexpr UBig(uint64_t val) noexcept : mLow(static_cast<ULOW>(val)), mHigh(0) {}

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
    constexpr UBig &operator+=(const UBig &other) noexcept
    {
        ULOW old_low = mLow;
        mLow += other.mLow;
        mHigh += other.mHigh;

        // Проброс переноса (Carry)
        if (mLow < old_low)
        {
            // Если у ULOW есть свой метод inc(), вызываем его (для U128/U256)
            // Иначе используем обычный инкремент (для uint64_t)
            if constexpr (requires(ULOW &h) { h.inc(); })
            {
                mHigh.inc();
            }
            else
            {
                mHigh++;
            }
        }
        return *this;
    }

    constexpr UBig operator+(const UBig &other) const noexcept { return UBig(*this) += other; }

    constexpr UBig &operator-=(const UBig &other) noexcept
    {
        ULOW old_low = mLow;
        mLow -= other.mLow;
        mHigh -= other.mHigh;

        // Проброс заимствования (Borrow)
        if (old_low < other.mLow)
        {
            if constexpr (requires(ULOW &h) { h.dec(); })
            {
                mHigh.dec();
            }
            else
            {
                mHigh--;
            }
        }
        return *this;
    }
    constexpr UBig operator-(const UBig &other) const noexcept { return UBig(*this) -= other; }

    // --- Инкремент / Декремент ---
    constexpr void inc() noexcept
    {
        if (++mLow == 0)
            ++mHigh;
    }
    constexpr void dec() noexcept
    {
        if (mLow == 0)
            --mHigh;
        --mLow;
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

    constexpr UBig operator*(const UBig &other) const noexcept
    {
        // 1. Полное произведение младших частей (N/2 * N/2 -> N)
        UBig lo_lo = mult_ext(mLow, other.mLow);

        // 2. Средние части (нам нужны только их нижние половины)
        // Используем методы .low() вместо прямого обращения к .mLow
        ULOW mid1 = (mLow * other.mHigh).low();
        ULOW mid2 = (mHigh * other.mLow).low();

        // 3. Сборка результата
        // Итоговое Low = lo_lo.low()
        // Итоговое High = lo_lo.high() + mid1 + mid2
        return UBig{lo_lo.low(), lo_lo.high() + mid1 + mid2};
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

    // --- Деление ---
    std::pair<UBig, UBig> operator/(const UBig &other) const
    {
        if (*this < other)
            return {UBig{0}, *this};

        if (other.mHigh == ULOW{0})
        {
            // Вызов половинчатого деления (UBig / ULOW)
            auto res = *this / other.mLow;
            return {res.first, UBig(res.second)};
        }

        // Нормализация
        uint32_t s = other.mHigh.countl_zero();
        UBig v = other << s;
        UBig u = *this;

        // Начальная оценка частного (может ошибаться на +2..-2 из-за нормализации)
        auto res = u.mHigh / v.mHigh;
        UBig q_res;
        if constexpr (requires { res.first; })
            q_res = std::move(UBig(res.first));
        else
            q_res = std::move(UBig(res));

        UBig prod = q_res * other;

        // Коррекция сверху
        while (prod > *this)
        {
            prod -= other;
            q_res.dec(); // Вместо q_res--
        }

        // Коррекция снизу
        UBig rem = *this - prod;
        while (rem >= other)
        {
            rem -= other;
            q_res.inc(); // Вместо q_res++
        }

        return {q_res, rem};
    }

    /**
         * @brief Оператор деления "широкого" числа на "половинку" (UBig / ULOW).
         * @return std::pair<UBig, ULOW> {Частное Q, Остаток R}.
         */
    std::pair<UBig, ULOW> operator/(const ULOW &other) const
    {
        assert(other != ULOW{0});

        if (other == ULOW{1})
            return {*this, ULOW{0}};
        if (mHigh == ULOW{0})
        {
            // Если число помещается в одну половинку, используем деление самого типа ULOW.
            // Предполагается, что ULOW / ULOW возвращает пару или имеет операторы / и %.
            if constexpr (requires(ULOW l) { l / other; })
            {
                auto res = mLow / other;
                if constexpr (requires { res.first; })
                    return {UBig(res.first), res.second};
                else
                    return {UBig(mLow / other), mLow % other};
            }
            else
            {
                return {UBig(mLow / other), mLow % other};
            }
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
     * @brief
     */
    std::pair<UBig, ULOW> operator/=(const ULOW &Y)
    {
        ULOW remainder;
        std::tie(*this, remainder) = *this / Y;
        return std::make_pair(*this, remainder);
    }

    /**
     * @brief operator %
     * @param other
     * @return
     */
    ULOW operator%(const ULOW &other) const
    {
        return (*this / other).second;
    }

    /**
     * @brief operator %=
     * @param other
     * @return
     */
    ULOW& operator%=(const ULOW &other)
    {
        *this = *this % other;
        return *this;
    }

    /**
     * @brief operator %
     * @param other
     * @return
     */
    UBig operator%(const UBig &other) const
    {
        return (*this / other).second;
    }

    /**
     * @brief operator %=
     * @param other
     * @return
     */
    UBig& operator%=(const UBig &other)
    {
        *this = *this % other;
        return *this;
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
    static constexpr ULOW get_quotient(const ULOW &a, const ULOW &b)
    {
        if constexpr (requires { (a / b).first; })
        {
            // Случай для UBig (возвращает pair)
            return (a / b).first;
        }
        else
        {
            // Случай для u64 или U128 (возвращает само число)
            return a / b;
        }
    }

    /**
         * @brief Рекурсивное преобразование в десятичную строку.
         */
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
};

} // namespace bignum
