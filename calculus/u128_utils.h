#pragma once

#include "u128.hpp"
#include "ubig.hpp"
#include <_mingw_mac.h>
#include <atomic>
#include <chrono>
#include <map>        // std::map
#include <optional>
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

struct TripleU128 {
    U128 mCell[3];
};

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

/**
 * @brief Случайное число на отрезке [a, b].
 * @param a
 * @param b
 * @return
 */
inline U128 get_random_value_ab(const U128& a, const U128& b) {
    assert(b >= a);
    const U128& m = b - a + 1;
    return m != 0 ? a + (get_random_value() % m) : get_random_value();
}

inline U128 get_random_half_value() {
    static RandomGenerator g_prng;
    U128 result {g_prng.mGenerator.next_u64(), 0};
    g_prng.mGenerator.next_u64();
    return result;
}

/**
 * @brief Sieve of Eratosthenes.
 * @param n
 * @return
 */
inline std::vector<unsigned> primes(unsigned n) {
    std::vector<unsigned> ps;
    ps.reserve(n);
    std::vector<uint8_t> b(n + 1, uint8_t(1));
    for (unsigned p = 2; p < n + 1; ++p) {
        if (b.at(p) != 0) {
            ps.push_back(p);
            for (unsigned i = p; i < n + 1; i += p) {
                b[i] = 0;
            }
        }
    }
    return ps;
}

/**
 * @brief int_power
 * @param x
 * @param y
 * @return
 */
inline U128 int_power(ULOW x, int y)
{
    U128 result{1};
    for (int i = 1; i <= y; ++i)
        result = result * x;
    return result;
}

/**
 * @brief Сложение двух чисел по заданному модулю.
 * @param x Сюда кладется результат (x + y) mod m.
 * @param y.
 * @param m Модуль.
 */
inline void add_mod(U128& x, const U128& y, const U128& m)
{
    using namespace bignum::ubig;
    using U256 = UBig<U128, 256>;
    const auto& z = U256{x} + U256{y};
    x = z % m;
}

/**
 * @brief Вычитание двух чисел по заданному модулю.
 * @param x Сюда кладется результат (x - y) mod m.
 * @param y.
 * @param m Модуль.
 */
inline void sub_mod(U128& x, const U128& y, const U128& m)
{
    const bool is_normal = x >= y;
    const auto& z = is_normal ? x - y : y - x;
    x = is_normal ? z % m : m - (z % m);
}

/**
 * @brief Умножение двух чисел по заданному модулю.
 * @param x Сюда кладется результат (x*y) mod m.
 * @param y.
 * @param m Модуль.
 */
inline void mult_mod(U128& x, const U128& y, const U128& m)
{
    using namespace bignum::ubig;
    using U256 = UBig<U128, 256>;
    const U256& z = U256::mult_ext(x, y);
    x = z % m;
}

/**
 * @brief Возведение в квадрат по заданному модулю.
 * @param x Число, возводимое в квадрат. Сюда кладется результат (x^2) mod m.
 * @param m Модуль.
 */
inline void square_mod(U128& x, const U128& m)
{
    using namespace bignum::ubig;
    using U256 = UBig<U128, 256>;
    U256 z { U256::square_ext(x) };
    x = z % m;
}

/**
 * @brief Возведение в квадрат с суммированием по заданному модулю.
 * @param x Число, возводимое в квадрат. Сюда кладется результат (x^2 + y) mod m.
 * @param y Аддитивная компонента.
 * @param m Модуль.
 */
inline void square_add_mod(U128& x, const U128& y, const U128& m)
{
    using namespace bignum::ubig;
    using U256 = UBig<U128, 256>;
    U256 z { U256::square_ext(x) };
    if (x < U128::get_max_value()) {
        z += U256{y};
        x = z % m;
    } else {
        z = U256{z % m} + U256{y};
        x = z % m;
    }
}

/**
 * @brief Умножение двух чисел с суммированием по заданному модулю.
 * @param x Число, возводимое в квадрат. Сюда кладется результат (x*y + z) mod m.
 * @param y Аддитивная компонента.
 * @param m Модуль.
 */
inline void mult_add_mod(U128& x, const U128& y, const U128& z, const U128& m)
{
    using namespace bignum::ubig;
    using U256 = UBig<U128, 256>;
    U256 w { U256::mult_ext(x, y) };
    if (x < U128::get_max_value() || y < U128::get_max_value()) {
        w += U256{z};
        x = w % m;
    } else {
        w = U256{w % m} + U256{z};
        x = w % m;
    }
}

/**
 * @brief Степень числа по заданному модулю.
 * @param x Основание степени. Сюда кладется результат (x^y) mod m.
 * @param y Степень.
 * @param m Модуль.
 */
inline void int_power_mod(U128& x, const U128& y, const U128& m)
{
    U128 exponent = y;
    U128 base = x;
    x = 1;
    while (exponent != 0)
    {
        if ((exponent & 1) == 1)
            mult_mod(x, base, m);
        exponent >>= 1;
        square_add_mod(base, 0, m);
    }
}

bool miller_test(U128 d, const U128 &n);

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
template <typename T>
inline T gcd(T x, T y)
{
    while (y != 0)
    {
        const auto& r = x % y;
        x = y;
        y = r;
    }
    return x;
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
    const auto& rx = x % p;
    U128 y2 = 0;
    for (U128 y = 0; y < p; y.inc())
    {
        if (const auto& ry2 = y2 % p; ry2 == rx)
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
    U128 result[2];
    int idx = 0;
    const auto& rx = x % p;
    U128 y2 = 0;
    for (U128 y = 0; y < p; y.inc())
    {
        if (const auto& ry2 = y2 % p; ry2 == rx)
            result[idx++] = y;
        y2 += (y + y + U128{1});
        if (idx == 2) break;
    }
    if (idx == 1)
        result[1] = result[0];
    return std::make_pair(result[0], result[1]);
}

/**
 * @brief Возвращает величину, обратную a по модулю m.
 * @param a Входное число.
 * @param m Модуль.
 * @param success Флаг успешности нахождения обратной величины.
 * @return Обратная к a величина, y, так, что y*a = 1 mod m.
 */
U128 modular_inverse(U128 a, U128 m, bool& success);

/**
 * @brief Addition in Elliptic curve modulo m space.
 * @param p
 * @param q
 * @param a
 * @param b
 * @param m
 * @return
 */
TripleU128 elliptic_add(const TripleU128& p, const TripleU128& q, const U128& a, const U128& b, const U128& m);

/**
 * @brief Multiplication (repeated addition and doubling).
 * @param k
 * @param p
 * @param a
 * @param b
 * @param m
 * @return
 */
TripleU128 elliptic_mul(U128 k, TripleU128 p, const U128& a, const U128& b, const U128& m);

/**
 * @brief lenstra
 * @param n
 * @param limit (~100000)
 * @return
 */
std::optional<U128> lenstra(const U128& n, unsigned int limit);

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
 * @brief Метод факторизации Ферма.
 * @param x Факторизуемое число.
 * @return Два множителя.
 */
std::pair<U128, U128> ferma_method(U128 x);

/**
 * @brief Алгоритм ро Полларда.
 * @param n Факторизуемое число.
 * @return Множитель.
 */
U128 ro_pollard(const U128& n, std::optional<U128> limit);

/**
 * @brief Факторизация числа.
 * @param x Факторизуемое число.
 * @return Результат разложения на простые множители {a prime number, a non-negative power}.
 */
std::map<U128, int> factor(U128 x);

} // namespace utils

inline Globals::_gu128 Globals::global_u128;

} // namespace u128
