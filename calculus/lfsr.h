#pragma once

/**
 * @author Новиков А.В., nawww83@gmail.com.
 *
 * Генератор LFSR в поле GF(p^m) с числом ячеек m = [1, 8] и простым модулем p = [2, 256*256).
 * Реализованы:
 *  - Генератор общего назначения:
 *     - p = [2, 256) для m = [5, 8],
 *     - p = [2, 256*256) для m = [1, 4].
 * Сдвоенный (счетверенный) генератор с фиксированным m = 4, p = [2, 256) находится в random_gen.h.
 */

#include <cstdint>
#include <cassert>
#include <array>
#include <type_traits>
#include <utility>
#include <concepts>
#include <limits>
#include <bit>

#if (defined(__x86_64__) || defined(_M_X64)) && !defined(USE_NO_SSE)
#ifndef USE_SSE
#define USE_SSE
#endif
#endif

#ifdef USE_SSE
#include <immintrin.h>
#include <smmintrin.h>
#endif

namespace lfsr8
{

using u64 = uint64_t;
using u32 = uint32_t;
using u16 = uint16_t;
using u16x8 = std::array<u16, 8>;
using u32x4 = std::array<u32, 4>;

template <int m>
class MType
{
public:
    typedef typename std::conditional<(m <= 4), u32, u16>::type SAMPLE;
    typedef typename std::conditional<(m <= 4), u32x4, u16x8>::type STATE;
};

template <int p, int m>
inline void modulo_p(typename lfsr8::MType<m>::STATE &state)
{
    for (int i = 0; i < m; i++)
    {
        state[i] %= (u32)p;
    }
    // Векторный путь обрабатывает все ячейки STATE, в том числе неиспользуемые (>= m).
    // Обнуляем их, чтобы get_state() и сравнение состояний не возвращали мусор.
    for (std::size_t i = (std::size_t)m; i < state.size(); i++)
    {
        state[i] = 0;
    }
}

// Вспомогательная функция для скалярного next()
template <int p, int m>
inline void scalar_next(typename lfsr8::MType<m>::STATE &state, const typename lfsr8::MType<m>::STATE &K, typename lfsr8::MType<m>::SAMPLE input)
{
    using SAMPLE = typename lfsr8::MType<m>::SAMPLE;
    const SAMPLE m_v = state[m - 1];
    for (int i = m - 1; i > 0; i--)
    {
        state[i] = (state[i - 1] + (u32)m_v * K[i]) % (u32)p;
    }
    state[0] = (input + (u32)m_v * K[0]) % (u32)p;
}

// Вспомогательная функция для скалярного back()
template <int p, int m>
inline void scalar_back(typename lfsr8::MType<m>::STATE &state, const typename lfsr8::MType<m>::STATE &K, const typename lfsr8::MType<m>::STATE &K_inv, typename lfsr8::MType<m>::SAMPLE input)
{
    using SAMPLE = typename lfsr8::MType<m>::SAMPLE;
    // (state[0] - input) mod p без знаковых и переполняющихся промежуточных значений.
    const u32 diff = ((u32)state[0] + (u32)p - (u32)input) % (u32)p;
    const u32 m_v = ((u32)K_inv[0] * diff) % (u32)p;
    for (int i = 0; i < m - 1; i++)
    {
        const u32 prod = m_v * (u32)K[i + 1];
        const u32 subtrahend = prod % (u32)p;
        int32_t res = (int32_t)state[i + 1] - (int32_t)subtrahend;
        res += res < 0 ? (int32_t)p : (int32_t)0;
        state[i] = (SAMPLE)res;
    }
    state[m - 1] = (SAMPLE)m_v;
}

template <std::unsigned_integral T>
constexpr T safe_ipow(int base, int exp)
{
    T baseT = base;
    T result = 1;
    for (int i = 0; i < exp; ++i)
    {
        // Проверка: не превысит ли следующее умножение предел типа T
        if (i > 0 && baseT > ((std::numeric_limits<T>::max)() / result))
        {
            return 0; // Возвращаем 0 как маркер переполнения
        }
        result *= baseT;
    }
    return result;
}

/**
     * @brief Генератор LFSR общего назначения в поле GF(p^m).
     * @details
     * p должно быть простым числом в интервале:
     *   [2, 256) для длин регистра (4, 8].
     *   [2, 256*256) для длин регистра [1, 4].
     * m - длина регистра, [1, 8].
     */
template <int p, int m>
class LFSR
{
    static_assert(m > 0 && m <= 8, "Ошибка: длина регистра m должна быть в интервале [1, 8]!");
    static_assert(p > 1, "Ошибка: модуль p должен быть больше 1!");
    static_assert((m > 4) ? (p < 256) : (p < 256 * 256),
                  "Ошибка: модуль p вне допустимого интервала для заданной длины регистра!");

    using STATE = typename MType<m>::STATE;
    using SAMPLE = typename MType<m>::SAMPLE;

public:
    /**
         * @brief Конструктор с параметром.
         * @param K Коэффициенты (-g[0], ..., -g[m-1]) порождающего полинома g(x) степени m в поле GF(p).
         * @details Коэффициент при x^m равен 1 и не используется в явном виде.
         * K[0] должен быть ненулевым по модулю p (иначе обратного по умножению коэффициента не существует).
         */
    constexpr LFSR(STATE K) : m_K(K)
    {
        m_calculate_inverse_of_K();
    };

    /**
         * @brief Максимальный период генератора (достигается для примитивного g(x)).
         * @details Для неприводимого, но не примитивного g(x) период делит T_MAX.
         * Для приводимого g(x) период может не делить T_MAX: например, при
         * g(x) = (x - a) * h(x), где h неприводим и примитивен, период равен p^(m-1) - 1,
         * а T_MAX = p^m - 1 = p*(p^(m-1) - 1) + (p - 1), то есть T_MAX % T = p - 1 != 0.
         * Поэтому в таком случае x^T_MAX != 1, и показатель T_MAX - 1 не является
         * обратным (см. inverse_of).
         */
    static constexpr u64 P_POW_M = safe_ipow<u64>(p, m);

    // Проверяем, что переполнения не случилось (P_POW_M не стал 0)
    static_assert(P_POW_M != 0,
                  "Ошибка: Период превышает возможности 64-битного целого числа!");

    static constexpr u64 T_MAX = P_POW_M - 1;

    /**
         * @brief Установить состояние.
         * @param state Состояние.
         * @details Каждая ячейка приводится по модулю p, поэтому в качестве состояния можно
         * передавать произвольные значения (например, сырой сид генератора). Ячейки с индексом
         * >= m обнуляются, чтобы get_state() не возвращал мусор.
         */
    void set_state(STATE state)
    {
        for (int i = 0; i < m; ++i)
        {
            if ((u32)state[i] >= (u32)p)
            {
                state[i] = (SAMPLE)((u32)state[i] % (u32)p);
            }
        }
        for (std::size_t i = (std::size_t)m; i < state.size(); ++i)
        {
            state[i] = 0;
        }
        m_state = std::move(state);
    }

    /**
         * @brief Установить единичное состояние.
         */
    void set_unit_state()
    {
        m_state = {1};
    }

    /**
         * Установить коэффициенты порождающего полинома g(x).
         */
    void set_K(STATE K)
    {
        m_K = K;
        m_calculate_inverse_of_K();
    }

    /**
         * @brief Сделать шаг вперед (осуществить один такт генератора).
         * @param input Входной символ (должен быть приведен по модулю p!), который подается на вход генератора.
         */
    void next(SAMPLE input = 0)
    {
#ifdef USE_SSE
        if constexpr (m > 4)
        {
            __m128i a = _mm_set1_epi16(m_state[m - 1]);
            __m128i b = _mm_load_si128((const __m128i *)&m_K[0]);
            __m128i tmp = _mm_mullo_epi16(a, b);
            __m128i c = _mm_load_si128((const __m128i *)&m_state[0]);
            c = _mm_slli_si128(c, 2);
            __m128i mask = _mm_slli_si128(_mm_set1_epi16(-1), 2);
            __m128i inp = _mm_andnot_si128(mask, _mm_set1_epi16(input));
            c = _mm_add_epi16(c, tmp);
            c = _mm_add_epi16(c, inp);
            _mm_store_si128((__m128i *)&m_state[0], c);
            modulo_p<p, m>(m_state);
        }
        else
        {
            scalar_next<p, m>(m_state, m_K, input);
        }
#else // Для процессора общего назначения.
        scalar_next<p, m>(m_state, m_K, input);
#endif
    }

    /**
         * @brief Сделать шаг назад. Является операцией, обратной к next(input).
         * @param input Входной символ (должен быть приведен по модулю p!), который подается на вход генератора.
         */
    void back(SAMPLE input = 0)
    {
        scalar_back<p, m>(m_state, m_K, m_inv_K, input);
    }

    /**
         * @brief Возвести в квадрат, то есть вычислить состояние (x^s)^2, где
         * x^s - текущее состояние (некоторая степень s вспомогательной переменной x).
         * Соответствует s итерациям next() - прямое вычисление (долго, если s - большое число).
         */
    void square()
    {
        m_state = m_mult(m_state, m_state, m_K);
    }

    /**
         * @brief Умножить текущее состояние x^s на некоторое другое состояние x^t.
         * Итоговое состояние генератора становится равным x^(s+t).
         * @param other Другое состояние x^t.
         * @details Реализуется через m_mult(): свёртка двух многочленов степени m-1 и
         * приведение результата по g(x), см. описание m_mult(). Алиасинг other == m_state
         * безопасен, так как m_mult() не изменяет аргументы до возврата результата.
         */
    void mult_by(const STATE &other)
    {
        m_state = m_mult(m_state, other, m_K);
    }

    /**
         * @brief Возвести текущее состояние генератора в степень.
         * @param q Показатель степени.
         */
    void power_by(u64 q)
    {
        m_state = m_pow(m_state, m_K, q);
    }

    /**
         * @brief Возвести состояние в степень.
         * @param state Состояние x^t.
         * @param K Коэффициенты порождающего полинома g(x).
         * @param q Показатель степени.
         * @return Состояние x^(qt).
         */
    static constexpr STATE power_by(const STATE &state, const STATE &K, u64 q)
    {
        return m_pow(state, K, q);
    }

    /**
         * @brief Вычислить обратное состояние.
         * @param state Состояние x^t (элемент цикла генератора).
         * @param K Коэффициенты порождающего полинома g(x).
         * @param T Период генератора, то есть порядок x в кольце GF(p)[x]/(g(x)) (x^T = 1).
         * @return Состояние, 1/x^t.
         * @details Так как x^T = 1, для любого состояния цикла s = x^t имеем
         * s^(T-1) = x^(tT-t) = (x^T)^t * x^(-t) = x^(-t) = 1/s. Поэтому показатель T-1
         * корректен при любом (в том числе приводимом) g(x), лишь бы K[0] != 0
         * (иначе x необратим и обратного состояния не существует).
         * Подходит любое кратное порядка x. Для неприводимого g(x) можно передавать
         * T = T_MAX = p^m - 1 (примитивность не требуется). Периоды реальных
         * генераторов см. в random_gen.h; для приводимых g(x) T_MAX не подходит.
         */
    static constexpr STATE inverse_of(const STATE &state, const STATE &K, u64 T)
    {
        assert(T > 0);
        return power_by(state, K, (T > 0) ? (T - 1) : 0);
    }

    /**
         * @brief Насытить генератор.
         * @param q Количество тактов.
         */
    void saturate(int q = m)
    {
        for (int i = 0; i < q; ++i)
        {
            next();
        }
    }

    /**
         * @brief Является ли заданное состояние текущим состоянием генератора.
         * @param state Заданное состояние.
         * @return Да/нет.
         */
    bool is_state(const STATE &state) const
    {
#ifdef USE_SSE
        bool result = true;
        for (int i = 0; i < m; ++i)
        {
            result &= (m_state[i] == state[i]);
        }
        return result;
#else
        return (state == m_state);
#endif
    }

    STATE get_generator_coeffs() const
    {
        return m_K;
    }

    /**
         * @brief Коэффициенты для шага назад.
         * @details Значим только элемент [0] (обратный по умножению K[0]), остальные элементы нулевые.
         */
    STATE get_inverse_of_coeffs() const
    {
        return m_inv_K;
    }

    STATE get_state() const
    {
        return m_state;
    }

    SAMPLE get_cell(int idx) const
    {
        assert((idx >= 0) && (idx < m));
        return m_state[idx];
    }

private:
    /**
         * @brief Умножение двух состояний: результат = lhs * rhs по модулю g(x).
         * @details Свёртка двух многочленов степени m-1 и приведение результата по
         * g(x) = x^m + g[0] + g[1]x + ... + g[m-1]x^(m-1), то есть по правилу
         * x^m = K[0] + K[1]x + ... + K[m-1]x^(m-1), где K[i] = -g[i].
         * Приведение выполняется от старших степеней к младшим (схема Горнера), поэтому
         * достаточно (m-1)*m умножений без вызовов next(). Все промежуточные суммы
         * накапливаются в u64 и берутся по модулю p только для коэффициента-делителя
         * и в конце: максимальное значение ячейки не превышает (2m-1)*(p-1)^2, что
         * для разрешенных p и m заведомо меньше 2^64.
         * @param lhs Первый сомножитель.
         * @param rhs Второй сомножитель.
         * @param K Коэффициенты порождающего полинома g(x).
         * @return Произведение состояний.
         */
    static constexpr STATE m_mult(const STATE &lhs, const STATE &rhs, const STATE &K)
    {
        // Коэффициенты произведения (степени 0 .. 2m-2).
        std::array<u64, 2 * m - 1> coeffs{};
        for (int i = 0; i < m; ++i)
        {
            const u64 a = lhs[i];
            if (a == 0)
                continue;
            for (int j = 0; j < m; ++j)
            {
                coeffs[i + j] += a * (u64)rhs[j];
            }
        }

        // Приведение по модулю g(x): x^power = x^(power-m) * x^m.
        for (int power = 2 * m - 2; power >= m; --power)
        {
            const u64 t = coeffs[power] % (u64)p;
            if (t == 0)
                continue;
            for (int i = 0; i < m; ++i)
            {
                coeffs[power - m + i] += t * (u64)K[i];
            }
        }

        STATE res{};
        for (int i = 0; i < m; ++i)
        {
            res[i] = (SAMPLE)(coeffs[i] % (u64)p);
        }
        return res;
    }

    /**
         * @brief Возведение состояния в степень (схема «слева направо»).
         * @details Не требует временного генератора и не копирует состояние на каждом шаге:
         * не более log2(q) возведений в квадрат и (popcount(q)-1) умножений.
         * @param x Состояние x^t.
         * @param K Коэффициенты порождающего полинома g(x).
         * @param q Показатель степени.
         * @return Состояние x^(qt).
         */
    static constexpr STATE m_pow(const STATE &x, const STATE &K, u64 q)
    {
        // x^0 = 1.
        STATE acc{};
        acc[0] = 1;
        if (q == 0)
            return acc;
        // Старший установленный бит обрабатывается без умножения на 1.
        acc = x;
        for (int bit = (int)std::bit_width(q) - 2; bit >= 0; --bit)
        {
            acc = m_mult(acc, acc, K);
            if (((q >> bit) & 1u) != 0u)
            {
                acc = m_mult(acc, x, K);
            }
        }
        return acc;
    }

    /**
         * @brief Состояние генератора.
         */
    alignas(16) STATE m_state{};

    /**
         * @brief Коэффициенты порождающего полинома g(x).
         */
    alignas(16) STATE m_K{};

    /**
         * @brief Коэффициенты регистра для выполнения шага назад.
         */
    alignas(16) STATE m_inv_K{};

    /**
         * @brief Вычисляется обратный (по умножению) коэффициент K[0].
         * @details Расширенный алгоритм Евклида. Если обратного не существует (K[0] == 0 по модулю p
         * или p не простое), m_inv_K[0] остается нулевым, а back() для такого генератора не работает.
         */
    constexpr void m_calculate_inverse_of_K()
    {
        m_inv_K[0] = 0;
        const int64_t modulo = p;
        const int64_t g0 = (int64_t)((u32)m_K[0] % (u32)p);
        assert(g0 != 0);
        int64_t r0 = modulo;
        int64_t r1 = g0;
        int64_t t0 = 0;
        int64_t t1 = 1;
        while (r1 != 0)
        {
            const int64_t q = r0 / r1;
            const int64_t r2 = r0 - q * r1;
            r0 = r1;
            r1 = r2;
            const int64_t t2 = t0 - q * t1;
            t0 = t1;
            t1 = t2;
        }
        if (r0 != 1)
        {
            return; // Обратного по умножению нет.
        }
        m_inv_K[0] = (SAMPLE)(((t0 % modulo) + modulo) % modulo);
    }
};

}
