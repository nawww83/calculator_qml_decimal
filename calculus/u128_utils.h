#pragma once

#include "u128.hpp"
#include "ubig.hpp"
#include <_mingw_mac.h>
#include <atomic>
#include <chrono>
#include <map>        // std::map
#include <optional>   // std::optional
#include <random>
#include <tuple>      // std::ignore, std::tie
#include <utility>    // std::pair

#include "random_gen.h"

namespace u128
{

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

namespace utils
{

using namespace bignum::u128;

struct RandomGenerator {

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

    explicit RandomGenerator() {
        lfsr8::u64 tmp;
        mGenerator.seed(get_random_u32x4((lfsr8::u64)&tmp));
    }
    lfsr_rng_2::gens mGenerator;
};

inline U128 get_random_value() {
    static RandomGenerator g_prng;
    U128 result { g_prng.mGenerator.next_u64(), g_prng.mGenerator.next_u64()};
    g_prng.mGenerator.next_u64();
    g_prng.mGenerator.next_u64();
    return result;
}

inline U128 get_random_half_value() {
    static RandomGenerator g_prng;
    U128 result {g_prng.mGenerator.next_u64(), 0};
    g_prng.mGenerator.next_u64();
    return result;
}

inline U128 int_power(ULOW x, int y)
{
    U128 result{1};
    for (int i = 1; i <= y; ++i)
        result = result * x;
    return result;
}

/**
 * @brief Умножение двух чисел по заданному модулю.
 * @param x Основание степени. Сюда кладется результат (x*y) mod m.
 * @param y Степень.
 * @param m Модуль.
 */
inline void mult_mod(U128& x, const U128& y, const U128& m)
{
    assert(m != 0);
    using namespace bignum::ubig;
    using U256 = UBig<U128, 256>;
    const U256& z = U256::mult_ext(x, y);
    x = (z / m).second;
}

/**
 * @brief Степень числа по заданному модулю.
 * @param x Основание степени. Сюда кладется результат (x^y) mod m.
 * @param y Степень.
 * @param m Модуль.
 */
inline void int_power_mod(U128& x, const U128& y, const U128& m)
{
    assert( m != 0 );
    U128 exponent = y;
    U128 base = x;
    x = 1;
    while (exponent != 0)
    {
        if ((exponent & 1) == 1)
            mult_mod(x, base, m);
        exponent >>= 1;
        mult_mod(base, base, m);
    }
}

bool miller_test(U128 d, U128 n);

/**
 * @brief Количество цифр числа.
 * @param x Число.
 * @return Количество цифр, минимум 1.
 */
inline int num_of_digits(U128 x)
{
    int i = 0;
    while (x != 0)
    {
        x = x.div10();
        i++;
    }
    return i + (i == 0);
}

/**
 * НОД.
 */
inline U128 gcd(U128 x, U128 y)
{
    if (x == y)
        return x;
    if (x > y)
    {
        while (y != 0)
        {
            const U128 y_copy = y;
            y = (x / y).second;
            x = y_copy;
        }
        return x;
    }
    else
    {
        while (x != 0)
        {
            const U128 x_copy = x;
            x = (y / x).second;
            y = x_copy;
        }
        return y;
    }
}

/**
 * @brief Целочисленный квадратный корень числа, sqrt(x).
 * @param exact Точно ли прошло извлечение корня.
 */
inline U128 isqrt(const U128& x, bool &exact)
{
    exact = false;
    if (x == 0)
    {
        exact = true;
        return x;
    }
    const auto bits = x.bit_length();
    U128 result = U128{1} << (bits / 2);
    U128 reg_x[2] = {x, U128{0}}; // Регистр сдвига.
    constexpr auto TWO = ULOW{2};
    for (;;) // Метод Ньютона.
    {
        reg_x[1] = reg_x[0];
        reg_x[0] = result;
        const auto &[quotient, remainder] = x / result;
        std::tie(result, std::ignore) = (result + quotient) / TWO;
        if (result == reg_x[0])
        {
            exact = remainder == 0 && quotient == result;
            return result;
        }
        if (result == reg_x[1])
            return reg_x[0];
    }
}

/**
 * @brief Целочисленный квадратный корень числа, sqrt(x).
 */
inline U128 isqrt(const U128& x)
{
    [[maybe_unused]] bool exact;
    return isqrt(x, exact);
}

/**
 * @brief Является ли число x квадратичным вычетом по модулю p.
 */
inline bool is_quadratic_residue(const U128& x, const U128& p)
{
    assert(p != 0);
    const auto& [_, rx] = x / p;
    U128 y2 = 0;
    for (U128 y = 0; y < p; y.inc())
    {
        if (const auto& [_, ry2] = y2 / p; ry2 == rx)
            return true;
        y2 += (y + y + U128{1});
    }
    return false;
}

/**
 * @brief Квадратный корень числа x по модулю p.
 */
inline std::pair<U128, U128> sqrt_mod(const U128& x, const U128& p)
{
    assert(p != 0);
    U128 result[2];
    int idx = 0;
    const auto& [_, rx] = x / p;
    U128 y2 = 0;
    for (U128 y = 0; y < p; y.inc())
    {
        if (const auto& [_, ry2] = y2 / p; ry2 == rx)
            result[idx++] = y;
        y2 += (y + y + U128{1});
        if (idx == 2) break;
    }
    if (idx == 1)
        result[1] = result[0];
    return std::make_pair(result[0], result[1]);
}

/**
 * @brief Является ли число простым.
 * @param x Проверяемое число.
 * @param k Количество раундов теста Миллера-Рабина.
 * @return Да/нет. Если "да", то существует некоторая вероятность ошибки, зависящая от k.
 */
bool is_prime(U128 x, int k);

/**
 * @brief Делит первое число на второе до "упора".
 * @param x Делимое.
 * @param q Делитель.
 * @return Пара {Делитель, Количество успешных делений}
 */
std::pair<U128, int> div_by_q(U128 &x, const U128& q);

/**
 * @brief Алгоритм Полларда p-1.
 * @param x Факторизуемое число.
 * @param limit Максимальное количество итераций.
 * @return Множитель.
 */
U128 pollard_minus_p(const U128& x, std::optional<U128> limit );

/**
 * @brief Метод факторизации Ферма.
 * @param x Факторизуемое число.
 * @return Два множителя.
 */
std::pair<U128, U128> ferma_method(U128 x);

/**
 * @brief Факторизация числа.
 * @param x Факторизуемое число.
 * @return Результат разложения на простые множители {a prime number, a non-negative power}.
 */
std::map<U128, int> factor(U128 x);

} // namespace utils

inline Globals::_gu128 Globals::global_u128;

} // namespace u128
