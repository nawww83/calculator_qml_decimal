#pragma once

#include "u128.hpp"
#include "ubig.hpp"
#include <_mingw_mac.h>
#include <atomic>
#include <map> // std::map
#include <optional>
#include <utility> // std::pair

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


U128 get_random_value();

/**
 * @brief Случайное число на отрезке [a, b].
 * @param a
 * @param b
 * @return
 */
U128 get_random_value_ab(const U128& a, const U128& b);

U128 get_random_half_value();

/**
 * @brief Sieve of Eratosthenes.
 * @param n
 * @return
 */
inline std::vector<unsigned> primes(unsigned n) {
    if (n < 2) return {};

    std::vector<unsigned> ps;
    // Оценка количества простых (n/ln n) для reserve
    ps.reserve(n / 6);

    // Используем вектор bool (или uint8_t) только для нечетных чисел
    std::vector<uint8_t> is_prime(n + 1, 1);

    ps.push_back(2);

    // Идем только по нечетным
    for (unsigned p = 3; p <= n; p += 2) {
        if (is_prime[p]) {
            ps.push_back(p);

            // Начинаем с p*p, шаг 2*p (чтобы попадать только на нечетные)
            if (1ULL * p * p <= n) {
                for (unsigned i = p * p; i <= n; i += 2 * p) {
                    is_prime[i] = 0;
                }
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
inline U128 int_power(u64 x, int y)
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
    using namespace bignum;
    using U256 = UBig<U128>;
    auto z = U256{x} + U256{y};
    x = (z / m).second;
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
    const auto z = is_normal ? x - y : y - x;
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
    using namespace bignum;
    using U256 = UBig<U128>;
    const U256 z = U256::mult_ext(x, y);
    x = (z / m).second;
}

/**
 * @brief Возведение в квадрат по заданному модулю.
 * @param x Число, возводимое в квадрат. Сюда кладется результат (x^2) mod m.
 * @param m Модуль.
 */
inline void square_mod(U128& x, const U128& m)
{
    using namespace bignum;
    using U256 = UBig<U128>;
    U256 z { U256::square_ext(x) };
    x = (z / m).second;
}

/**
 * @brief Возведение в квадрат с суммированием по заданному модулю.
 * @param x Число, возводимое в квадрат. Сюда кладется результат (x^2 + y) mod m.
 * @param y Аддитивная компонента.
 * @param m Модуль.
 */
inline void square_add_mod(U128& x, const U128& y, const U128& m)
{
    using namespace bignum;
    using U256 = UBig<U128>;
    U256 z { U256::square_ext(x) };
    if (x < U128::max()) {
        z += U256{y};
        x = (z / m).second;
    } else {
        z = U256{(z / m).second} + U256{y};
        x = (z / m).second;
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
    using namespace bignum;
    using U256 = UBig<U128>;
    U256 w { U256::mult_ext(x, y) };
    if (x < U128::max() || y < U128::max()) {
        w += U256{z};
        x = (w / m).second;
    } else {
        w = U256{(w / m).second} + U256{z};
        x = (w / m).second;
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

/**
 * @brief Быстрое возведение в степень x^y mod 2^128.
 * Использует алгоритм бинарного возведения в квадрат.
 */
inline U128 int_power_fast(U128 x, unsigned y) noexcept
{
    if (y == 0) return U128{1};
    if (x == 0) return U128{0};
    if (x == 1) return U128{1};
    if (x == 2) return (y < 128) ? (U128{1} << y) : U128{0};

    U128 result{1};
    for (;;) {
        if (y & 1) result *= x;
        y >>= 1;
        if (y == 0) break;
        x *= x;
    }
    return result;
}

/**
 * @brief Целочисленный квадратный корень sqrt(x).
 * @param exact Возвращает true, если x — полный квадрат.
 */
inline U128 isqrt(const U128& x, bool& exact)
{
    if (x == 0) {
        exact = true;
        return 0;
    }

    // Начальное приближение: 2^(ceil(bits/2))
    U128 x0 = U128{1} << ((x.bit_width() + 1) / 2);
    U128 x1;

    for (;;) {
        U128 remainder;
        // ОПТИМИЗАЦИЯ: Получаем частное и остаток за один проход
        U128 quotient = U128::divide<true, true>(x, x0, &remainder);

        x1 = (x0 + quotient) >> 1;

        // В целочисленном методе Ньютона x1 >= x0 означает сходимость к floor(sqrt)
        if (x1 >= x0) {
            exact = (x0 * x0 == x);
            return x0;
        }
        x0 = x1;
    }
}

// Перегрузка для удобства
inline U128 isqrt(const U128& x) {
    bool dummy;
    return isqrt(x, dummy);
}

/**
 * @brief Целочисленный корень m-й степени из x.
 */
inline U128 nroot(const U128& x, unsigned m)
{
    if (m == 0) return 0;
    if (x <= 1 || m == 1) return x;
    if (m >= 128) return (x > 0) ? U128{1} : U128{0};
    if (m == 2) return isqrt(x);

    // 1. Начальное приближение "сверху"
    // Берем 2^(ceil(bit_width / m))
    uint32_t target_bits = (x.bit_width() + m - 1) / m;
    U128 x0 = U128{1} << target_bits;

    // Страховка: если 2^target_bits оказался больше x
    if (x0 > x) x0 = x;

    U128 m_val{m};
    [[maybe_unused]] U128 m_minus_1 = m_val - 1;

    for (;;) {
        U128 p = int_power_fast(x0, m - 1);
        U128 quotient = (p == 0) ? U128{0} : (x / p);

        // Стандартная формула Ньютона: x1 = ((m-1)*x0 + quotient) / m
        // Чтобы избежать переполнения (m-1)*x0, считаем через разность:
        U128 x1;
        if (x0 > quotient) {
            // Идем вниз: x1 = x0 - (x0 - quotient) / m
            U128 diff = (x0 - quotient) / m_val;

            // КРИТИЧЕСКИЙ МОМЕНТ: если diff == 0, но x0 > quotient,
            // это не значит, что мы на месте. Это значит, что шаг < 1.
            // В целых числах нам нужно принудительно сделать шаг -1.
            if (diff == 0) x1 = x0 - 1;
            else x1 = x0 - diff;
        } else {
            // Если x0 <= quotient, мы либо нашли корень, либо зашли снизу.
            // Метод Ньютона сверху вниз гарантирует, что x0 >= floor(root).
            return x0;
        }

        // Если x1 перелетел через корень (стал слишком маленьким)
        // или если мы начали расти — останавливаемся.
        if (x1 >= x0) return x0;

        // Проверка: не стал ли x1^m меньше x?
        // Если стал — значит x1 и есть наш floor(root).
        // Но проще довериться сходимости и сделать еще одну итерацию.
        x0 = x1;
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
        x /= 10u;
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
    T r;
    while (y != 0)
    {
        if constexpr (requires { { x / y } -> std::same_as<std::pair<T, T>>; }) {
            r = (x / y).second;
        }
        else {
            r = (x % y);
        }
        x = y;
        y = r;
    }
    return x;
}

/**
 * @brief Является ли число x квадратичным вычетом по модулю p.
 */
inline bool is_quadratic_residue(const U128& x, const U128& p)
{
    const auto rx = x % p;
    U128 y2 = 0;
    for (U128 y = 0; y < p; y++)
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
    for (U128 y = 0; y < p; y++)
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


U128 modular_inverse(U128 a, U128 m, bool &success);

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
