/**
 * @author nawww83@gmail.com
 * @brief Класс для арифметики N-битных беззнаковых целых чисел.
 */

#pragma once

#include <cstdint>
#include <bit>
#include <cassert>
#include <compare>
#include <algorithm>
#include <functional>
#include <limits>
#include <ostream>
#include <string>
#include <type_traits>
#include <utility>
#include "u128.hpp" // generic

namespace bignum
{

/**
     * @brief Иерархический класс для длинных чисел.
     * @tparam Half Тип "половинки" (например, uint64_t или U128).
     */
template <typename Half>
class UBig
{
    Half mLow{0};
    Half mHigh{0};

public:
    // Используем универсальный способ определения разрядности
    static constexpr uint32_t HALF_WIDTH = static_cast<uint32_t>(bignum::generic::bit_size<Half>());
    static constexpr uint32_t WIDTH = HALF_WIDTH * 2;

    // Разрядность лимба для быстрого деления: 64 бита там, где есть 128-битные
    // целые (GCC/Clang), иначе 32 бита (MSVC). Алгоритм деления — общий.
#if defined(__SIZEOF_INT128__)
    using limb_t = uint64_t;
    using wide_t = unsigned __int128; // двойной лимб
    using swide_t = __int128;         // знаковый двойной лимб
#else
    using limb_t = uint32_t;
    using wide_t = uint64_t;
    using swide_t = int64_t;
#endif
    static constexpr uint32_t LIMB_BITS = static_cast<uint32_t>(sizeof(limb_t) * 8);
    static constexpr uint32_t LIMBS = WIDTH / LIMB_BITS;
    static constexpr uint32_t HALF_LIMBS = HALF_WIDTH / LIMB_BITS;
    static constexpr bool FLAT = (WIDTH % LIMB_BITS == 0) && (HALF_WIDTH % LIMB_BITS == 0);

    // Тип "половинки" для использования в generic-функциях
    using value_type = Half;

    // --- Конструкторы ---
    constexpr UBig() noexcept = default;

    // Используем std::move, так как Half может быть тяжелым (например, U1024)
    constexpr UBig(Half low, Half high) noexcept
        : mLow(std::move(low)), mHigh(std::move(high)) {}

    constexpr UBig(Half low) noexcept
        : mLow(std::move(low)), mHigh(0) {}

    /**
         * @brief Универсальный конструктор от целочисленного примитива.
         * @details Реализован шаблоном, а не как UBig(uint64_t), чтобы не
         * конфликтовать с UBig(Half), когда сам Half — примитив (uint64_t).
         * @tparam T Любой целочисленный тип, кроме самого Half.
         */
    template <typename T>
        requires(std::is_integral_v<T> && !std::is_same_v<std::remove_cvref_t<T>, Half>)
    constexpr UBig(T val) noexcept
        : mLow(static_cast<Half>(val)), mHigh(0) {}

    // Конструктор от string_view (или char*)
    constexpr UBig(std::string_view s)
    {
        *this = fromString(s);
    }

    // --- Доступ ---
    [[nodiscard]] constexpr const Half &low() const noexcept { return mLow; }
    [[nodiscard]] constexpr const Half &high() const noexcept { return mHigh; }
    [[nodiscard]] constexpr Half &low() noexcept { return mLow; }
    [[nodiscard]] constexpr Half &high() noexcept { return mHigh; }

    /**
         * @brief Младшие 64 бита числа (сужение без проверки диапазона).
         */
    [[nodiscard]] constexpr uint64_t to_uint64() const noexcept
    {
        if constexpr (requires { mLow.to_uint64(); })
            return mLow.to_uint64(); // Вложенный UBig
        else if constexpr (requires { mLow.low(); })
            return static_cast<uint64_t>(mLow.low()); // U128
        else
            return static_cast<uint64_t>(mLow); // Примитив
    }

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

    // --- Логические проверки ---
    [[nodiscard]] constexpr explicit operator bool() const noexcept
    {
        return mLow != low_min() || mHigh != low_min();
    }
    [[nodiscard]] constexpr bool operator!() const noexcept
    {
        return mLow == low_min() && mHigh == low_min();
    }

    // --- Сложение и вычитание ---

    // --- Сложение ---
    constexpr UBig &operator+=(const UBig &other) noexcept
    {
        Half old_low = mLow;
        mLow += other.mLow;

        // 1. Сначала учитываем перенос из младшей части
        if (mLow < old_low)
        {
            // Проверяем, можно ли вызвать ++ у объекта типа Half
            if constexpr (requires(Half &obj) { ++obj; })
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
        Half old_low = mLow;
        mLow -= other.mLow;

        // 1. Учитываем заем ПЕРЕД вычитанием основной части
        if (old_low < other.mLow)
        {
            // Проверяем, можно ли вызвать -- у объекта типа Half
            if constexpr (requires(Half &obj) { --obj; })
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
        Half old = mLow;
        // 1. Инкрементируем младшую часть
        if constexpr (requires(Half &l) { ++l; })
            ++mLow;
        else
            mLow += 1;

        // 2. Если произошел перенос (результат стал меньше старого)
        if (mLow < old)
        {
            // Инкрементируем старшую часть тем же способом
            if constexpr (requires(Half &h) { ++h; })
                ++mHigh;
            else
                mHigh += 1;
        }
    }

    constexpr void dec() noexcept
    {
        Half old = mLow;
        // 1. Декрементируем младшую часть
        if constexpr (requires(Half &l) { --l; })
            --mLow;
        else
            mLow -= 1;

        // 2. Если произошло заимствование (результат стал больше старого)
        if (mLow > old)
        {
            // Декрементируем старшую часть
            if constexpr (requires(Half &h) { --h; })
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
    static constexpr UBig mult_ext(const Half &x, const Half &y) noexcept
    {
        constexpr int Q = WIDTH / 4; // Четверть ширины N-битного числа.
        Half MASK = (Half{1} << Q) - Half{1};
        Half x_low = x & MASK;
        Half y_low = y & MASK;
        Half x_high = x >> Q;
        Half y_high = y >> Q;
        Half t1 = x_low * y_low;
        Half t = t1 >> Q;
        Half t21 = x_low * y_high;
        Half q = t21 >> Q;
        Half p = t21 & MASK;
        Half t22 = x_high * y_low;
        Half s = t22 >> Q;
        Half r = t22 & MASK;
        Half t3 = x_high * y_high;
        UBig result{t1};
        Half div = (q + s) + ((p + r + t) >> Q);
        auto p1 = t21 << Q;
        auto p2 = t22 << Q;
        Half mod = p1 + p2;
        result.mLow += mod;
        result.mHigh += div;
        result.mHigh += t3;
        return result;
    }

    /**
         * @brief Возведение N/2-битного числа в квадрат с расширением до N-битного числа.
         * Оптимизировано по сравнению с обычным умножением (x * x).
         */
    static constexpr UBig square_ext(const Half &x) noexcept
    {
        constexpr uint32_t Q = WIDTH / 4; // Четверть ширины N-битного числа.
        Half MASK = (Half{1} << Q) - Half{1};
        Half x_low = x & MASK;
        Half x_high = x >> Q;
        Half t1 = x_low * x_low;
        Half t = t1 >> Q;
        Half t21 = x_low * x_high;
        Half q = t21 >> Q;
        Half p = t21 & MASK;
        Half t3 = x_high * x_high;
        UBig result{t1};
        Half div = (q << 1) + (((p << 1) + t) >> Q);
        auto p1 = t21 << Q;
        Half mod = p1 << 1;
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
    constexpr UBig &operator*=(const UBig &other) noexcept { return *this = (*this * other); }

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
        if (other.mHigh == Half{0})
        {
            auto [q, r] = *this / other.mLow;
            return {q, UBig{r}};
        }

        // 1. Начальная оценка частного и остатка по старшим половинам
        auto [Q, R] = divide_as_pair(mHigh, other.mHigh);

        // 2. Учет вклада младших половин (Delta-коррекция)
        constexpr Half MAX_V = low_max();
        const Half Delta = MAX_V - other.mLow;
        const UBig DeltaQ = mult_ext(Delta, Q);

        const UBig sum_1 = UBig{0, R} + DeltaQ;
        const UBig sub_val = UBig{0, Q};
        const bool is_neg = sum_1 < sub_val;

        UBig W1 = is_neg ? (sub_val - sum_1) : (sum_1 - sub_val);

        // 3. Вычисление поправочного коэффициента Quotient
        const Half C1 = (other.mHigh < MAX_V) ? (other.mHigh + 1ull) : MAX_V;
        const Half W2 = MAX_V - get_quotient(Delta, C1);

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
         * @brief Оператор деления "широкого" числа на "половинку" (UBig / Half).
         * @return std::pair<UBig, Half> {Частное Q, Остаток R}.
         */
    constexpr std::pair<UBig, Half> operator/(const Half &other) const
    {
        assert(other != Half{0});

        if (other == Half{1})
            return {*this, Half{0}};
        if (mHigh == Half{0})
        {
            auto [q, r] = divide_as_pair( mLow, other);
            return {UBig(q), r};
        }

        if constexpr (FLAT)
        {
            // === Быстрый путь: длинное деление (алгоритм D Кнута) по лимбам ===
            // Разрядность лимба выбирается автоматически (64 бита с __int128, иначе 32).
            // Это основной горячий путь проекта.
            limb_t u[LIMBS] = {};
            limb_t v[HALF_LIMBS] = {};
            limb_t q[LIMBS] = {};
            limb_t r[HALF_LIMBS] = {};

            UBig tmp = *this;
            for (uint32_t i = 0; i < LIMBS; ++i)
            {
                u[i] = static_cast<limb_t>(tmp.to_uint64());
                tmp >>= LIMB_BITS;
            }
            UBig dtmp{other};
            for (uint32_t i = 0; i < HALF_LIMBS; ++i)
            {
                v[i] = static_cast<limb_t>(dtmp.to_uint64());
                dtmp >>= LIMB_BITS;
            }

            // Отбрасываем незначащие старшие лимбы делителя: divmnu требует v[n-1] != 0.
            int n = static_cast<int>(HALF_LIMBS);
            while (n > 1 && v[n - 1] == 0)
                --n;

            divmnu(q, r, u, v, static_cast<int>(LIMBS), n);

            UBig Q{0};
            for (uint32_t i = 0; i < LIMBS; ++i)
                Q |= UBig(q[i]) << (LIMB_BITS * i);

            UBig R{0};
            for (uint32_t i = 0; i < HALF_LIMBS; ++i)
                R |= UBig(r[i]) << (LIMB_BITS * i);

            // Остаток строго меньше делителя (< 2^HALF_WIDTH), поэтому помещается в одну "половинку".
            return {Q, R.mLow};
        }
        else
        {
            // --- Резервный авторский итеративный метод (для ширин, не кратных 64) ---
            UBig X = *this;
            const UBig X_old = X;
            UBig Q{0};
            Half R{0};

            std::pair<Half, Half> rcp = bignum::generic::reciprocal_and_extend(other);
            const Half rcp_compl = other - rcp.second;

            const bool make_inverse = rcp_compl < rcp.second;
            if (make_inverse)
                ++(rcp.first);

            for (;;)
            {
                const bool x_has_high = (X.mHigh != Half{0});

                if (x_has_high)
                    Q += mult_ext(X.mHigh, rcp.first);

                Q += UBig(get_quotient(X.mLow, other));

                const Half carry = bignum::generic::smart_remainder_adder(R, X.mLow, other, rcp.second);
                Q += UBig(carry);

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

                if (Q > X_old)
                {
                    Q = -Q;
                    R = other - R;
                    if (R == other)
                    {
                        R = Half{0};
                        Q.inc();
                    }
                }
                break;
            }
            return {Q, R};
        }
    }

    // --- Деление с присваиванием и остаток ---
    // Тип результата всегда совпадает с типом левого операнда (UBig) — как у U128.
    // Для узкого делителя Half конверсия в UBig происходит неявно, а внутри
    // operator/ срабатывает быстрый путь (other.mHigh == 0).
    constexpr UBig &operator/=(const UBig &other) noexcept { return *this = (*this / other).first; }
    constexpr UBig &operator%=(const UBig &other) noexcept { return *this = (*this / other).second; }

    /**
         * @brief Остаток от деления.
         */
    [[nodiscard]] constexpr UBig operator%(const UBig &other) const noexcept { return (*this / other).second; }

    /**
         * @brief Количество ведущих нулей.
         */
    [[nodiscard]] constexpr uint32_t countl_zero() const noexcept
    {
        // Если старшая часть не ноль, считаем нули в ней.
        // Если ноль — прибавляем всю ширину старшей части к нулям в младшей.
        if (mHigh != 0)
        {
            if constexpr (requires(Half l) { l.countl_zero(); })
                return mHigh.countl_zero();
            else
                return std::countl_zero(static_cast<uint64_t>(mHigh));
        }
        uint32_t l_zeros = 0;
        if constexpr (requires(Half l) { l.countl_zero(); })
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
            if constexpr (requires(Half l) { l.countr_zero(); })
                return mLow.countr_zero();
            else
                return std::countr_zero(static_cast<uint64_t>(mLow));
        }
        uint32_t r_zeros = 0;
        if constexpr (requires(Half l) { l.countr_zero(); })
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
        auto pc = [](const Half &val)
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
         * @brief Максимальное значение "половинки".
         * @details Работает и для классов (U128, вложенный UBig), и для примитивов.
         */
    [[nodiscard]] static constexpr Half low_max() noexcept
    {
        if constexpr (requires { Half::max(); })
            return Half::max();
        else
            return std::numeric_limits<Half>::max();
    }

    /**
         * @brief Минимальное значение "половинки" (обычно 0).
         */
    [[nodiscard]] static constexpr Half low_min() noexcept
    {
        if constexpr (requires { Half::min(); })
            return Half::min();
        else
            return Half{0};
    }

    /**
         * @brief Возвращает максимально возможное значение для данного типа UBig.
         * Рекурсивно заполняет все уровни вложенности единицами.
         */
    [[nodiscard]] static constexpr UBig max() noexcept
    {
        return UBig{low_max(), low_max()};
    }

    /**
         * @brief Возвращает минимальное значение (0).
         */
    [[nodiscard]] static constexpr UBig min() noexcept
    {
        return UBig{low_min(), low_min()};
    }

    /**
         * @brief Вспомогательный метод для получения только частного.
         */
    static constexpr UBig get_quotient(const UBig &a, const Half &b)
    {
        return (a / b).first;
    }

    /**
         * @brief Вспомогательный метод для получения только частного.
         */
    static constexpr Half get_quotient(const Half &a, const Half &b)
    {
        return bignum::generic::div_rem(a, b).first;
    }

    /**
         * @brief Вспомогательный метод для получения частного и остатка в виде пары.
         */
    static constexpr auto divide_as_pair(const Half &a, const Half &b)
    {
        return bignum::generic::div_rem(a, b);
    }

    /**
         * @brief Длинное деление (алгоритм D Кнута) в системе счисления 2^LIMB_BITS.
         * @details Делит u (m лимбов) на v (n лимбов). v[n-1] != 0, n >= 1.
         *          u изменяется на месте. q получает m-n+1 лимбов (остальные
         *          должны быть предварительно обнулены), r — n лимбов.
         *          Разрядность лимба (64 или 32 бита) выбирается на этапе
         *          компиляции — так код работает и на MSVC без __int128.
         */
    static constexpr void divmnu(limb_t *q, limb_t *r, limb_t *u,
                                 const limb_t *v, int m, int n) noexcept
    {
        // Делитель из одного лимба — простой проход.
        if (n == 1)
        {
            limb_t k = 0;
            for (int j = m - 1; j >= 0; --j)
            {
                wide_t cur = (static_cast<wide_t>(k) << LIMB_BITS) | u[j];
                q[j] = static_cast<limb_t>(cur / v[0]);
                k = static_cast<limb_t>(cur % v[0]);
            }
            r[0] = k;
            return;
        }

        constexpr wide_t B = static_cast<wide_t>(1) << LIMB_BITS;
        const int s = static_cast<int>(std::countl_zero(v[n - 1]));

        // Нормализация: сдвигаем делитель так, чтобы старший бит старшего лимба был установлен.
        limb_t vn[LIMBS];
        for (int i = n - 1; i > 0; --i)
            vn[i] = (v[i] << s) | (s ? (v[i - 1] >> (LIMB_BITS - s)) : 0);
        vn[0] = static_cast<limb_t>(v[0] << s);

        limb_t un[LIMBS + 1];
        un[m] = s ? (u[m - 1] >> (LIMB_BITS - s)) : 0;
        for (int i = m - 1; i > 0; --i)
            un[i] = (u[i] << s) | (s ? (u[i - 1] >> (LIMB_BITS - s)) : 0);
        un[0] = static_cast<limb_t>(u[0] << s);

        for (int j = m - n; j >= 0; --j)
        {
            // Оценка очередного лимба частного с коррекцией.
            wide_t num = (static_cast<wide_t>(un[j + n]) << LIMB_BITS) | un[j + n - 1];
            wide_t qhat = num / vn[n - 1];
            wide_t rhat = num % vn[n - 1];
            while (qhat >= B || qhat * vn[n - 2] > ((rhat << LIMB_BITS) | un[j + n - 2]))
            {
                --qhat;
                rhat += vn[n - 1];
                if (rhat >= B)
                    break;
            }

            // Умножение и вычитание: un[j .. j+n] -= qhat * vn
            swide_t k = 0;
            for (int i = 0; i < n; ++i)
            {
                wide_t p = qhat * vn[i];
                swide_t t = static_cast<swide_t>(un[i + j]) - k - static_cast<swide_t>(static_cast<limb_t>(p));
                un[i + j] = static_cast<limb_t>(t);
                k = static_cast<swide_t>(p >> LIMB_BITS) - (t >> LIMB_BITS);
            }
            swide_t t2 = static_cast<swide_t>(un[j + n]) - k;
            un[j + n] = static_cast<limb_t>(t2);
            q[j] = static_cast<limb_t>(qhat);

            if (t2 < 0)
            {
                // Оценка была на единицу больше — прибавляем делитель обратно.
                q[j] -= 1;
                limb_t carry = 0;
                for (int i = 0; i < n; ++i)
                {
                    wide_t ss = static_cast<wide_t>(un[i + j]) + vn[i] + carry;
                    un[i + j] = static_cast<limb_t>(ss);
                    carry = static_cast<limb_t>(ss >> LIMB_BITS);
                }
                un[j + n] = static_cast<limb_t>(un[j + n] + carry);
            }
        }

        // Денормализация остатка.
        for (int i = 0; i < n; ++i)
            r[i] = (un[i] >> s) | (s ? (un[i + 1] << (LIMB_BITS - s)) : 0);
    }

    /**
         * @brief Рекурсивное преобразование в десятичную строку.
         * Использует деление всего числа на 10^18 для точности.
         */
    [[nodiscard]] std::string toString() const
    {
        if (mHigh == Half{0} && mLow == Half{0})
            return "0";

        UBig copy = *this;
        std::string final_res = "";

        // Используем 10^18 как базу. Она гарантированно влезает в uint64_t
        // и позволяет удобно дополнять нулями.
        const uint64_t base_val = 1000000000000000000ULL;
        const Half base_low{base_val};

        while (copy > UBig{0})
        {
            // Вызываем ваше половинчатое деление (UBig / Half)
            // Оно возвращает пару {UBig quotient, Half remainder}
            auto [q, r] = copy / base_low;

            // Превращаем остаток r в строку.
            // Если Half - это u64, используем std::to_string.
            // Если Half - это U128, вызываем r.toString().
            std::string part;
            if constexpr (std::is_same_v<Half, uint64_t>)
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

        // Для U256 (где Half=U128) мы можем эффективно
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

/**
     * @brief Потоковый вывод UBig в десятичном виде.
     */
template <typename Half>
std::ostream &operator<<(std::ostream &os, const UBig<Half> &val)
{
    return os << val.toString();
}

} // namespace bignum

namespace std
{
/**
     * @brief Хеш для UBig. Алгоритм комбинирования хешей половин — как у U128.
     */
template <typename Half>
struct hash<bignum::UBig<Half>>
{
    size_t operator()(const bignum::UBig<Half> &v) const noexcept
    {
        size_t h1 = std::hash<Half>{}(v.low());
        size_t h2 = std::hash<Half>{}(v.high());
        return h1 ^ (h2 + 0x9e3779b9 + (h1 << 6) + (h1 >> 2));
    }
};
} // namespace std
