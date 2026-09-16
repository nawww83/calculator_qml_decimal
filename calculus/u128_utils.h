#pragma once

#include "u128.hpp"
#include "ubig.hpp"
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

/**
 * @brief Структура для передачи результатов предварительного анализа числа.
 */
struct PreprocessResult
{
    std::map<U128, int> factors;
    U128 composite_remainder = 0;
    int global_power = 1;
};

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
inline void mult_mod(U128 &x, const U128 &y, const U128 &m)
{
    using namespace bignum;
    using U256 = UBig<U128>;

    // Вычисляем точное 256-битное произведение
    const U256 z = U256::mult_ext(x, y);

    // Явно указываем компилятору тип делителя, совпадающий с шаблоном
    const U128 &divisor = m;

    // Вызываем оператор деления. Так как divisor имеет тип U128 (который является ULOW для U256),
    // компилятор выберет быстрый детерминированный метод с обратной величиной.
    std::pair<U256, U128> res = z.operator/(divisor);

    x = res.second;
}

/**
 * @brief Возведение в квадрат по заданному модулю.
 * @param x Число, возводимое в квадрат. Сюда кладется результат (x^2) mod m.
 * @param m Модуль.
 */
inline void square_mod(U128 &x, const U128 &m)
{
    using namespace bignum;
    using U256 = UBig<U128>;

    // Используем ваш оптимизированный метод square_ext вместо mult_ext
    const U256 z = U256::square_ext(x);

    const U128 &divisor = m;
    std::pair<U256, U128> res = z.operator/(divisor);

    x = res.second;
}

/**
 * @brief Возведение в квадрат с суммированием по заданному модулю.
 * @param x Число, возводимое в квадрат. Сюда кладется результат (x^2 + y) mod m.
 * @param y Аддитивная компонента.
 * @param m Модуль.
 */
inline void square_add_mod(U128 &x, const U128 &y, const U128 &m)
{
    using namespace bignum;
    using U256 = UBig<U128>;

    U256 z = U256::square_ext(x);
    z += U256{y};

    const U128 &divisor = m;
    std::pair<U256, U128> res = z.operator/(divisor);

    x = res.second;
}

/**
 * @brief Умножение двух чисел с суммированием по заданному модулю.
 * @param x Число, возводимое в квадрат. Сюда кладется результат (x*y + z) mod m.
 * @param y Аддитивная компонента.
 * @param m Модуль.
 */
inline void mult_add_mod(U128 &x, const U128 &y, const U128 &z, const U128 &m)
{
    using namespace bignum;
    using U256 = UBig<U128>;

    // 1. Вычисляем точное 256-битное произведение
    U256 w = U256::mult_ext(x, y);

    // 2. Безопасно прибавляем z без риска переполнения 256 бит
    w += U256{z};

    // 3. Вызываем быстрый оператор деления широкого на узкое (UBig / ULOW)
    const U128 &divisor = m;
    std::pair<U256, U128> res = w.operator/(divisor);

    x = res.second;
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
 * @brief Целочисленный квадратный корень sqrt(x) на аппаратной скорости GCC.
 * @param exact Возвращает true, если x — полный квадрат.
 */
inline U128 isqrt(const U128 &x, bool &exact)
{
    if (x == 0) {
        exact = true;
        return 0;
    }

#if defined(__SIZEOF_INT128__)
    // --- Быстрый путь для GCC / Clang / Linux ---
    unsigned __int128 v = (static_cast<unsigned __int128>(x.high()) << 64) | x.low();

    // Подсчет значащих бит через встроенную функцию GCC
    uint32_t bits = 128 - __builtin_clzll(static_cast<unsigned long long>(v >> 64) ? (v >> 64) : v);
    if (static_cast<unsigned long long>(v >> 64))
        bits += 64;

    unsigned __int128 x0 = static_cast<unsigned __int128>(1) << ((bits + 1) / 2);
    unsigned __int128 x1;

    for (;;) {
        x1 = (x0 + v / x0) >> 1;
        if (x1 >= x0) {
            exact = (x0 * x0 == v);
            return U128(static_cast<uint64_t>(x0), static_cast<uint64_t>(x0 >> 64));
        }
        x0 = x1;
    }
#else
    // --- Надежный fallback-путь для MSVC / Windows ---
    U128 x0 = U128{1} << ((x.bit_width() + 1) / 2);
    U128 x1;

    for (;;) {
        U128 quotient = x / x0;
        x1 = (x0 + quotient) >> 1;

        if (x1 >= x0) {
            exact = (x0 * x0 == x);
            return x0;
        }
        x0 = x1;
    }
#endif
}

// Перегрузка для удобства
inline U128 isqrt(const U128& x) {
    bool dummy;
    return isqrt(x, dummy);
}

/**
 * @brief Целочисленный корень m-й степени из x.
 */
inline U128 nroot(const U128 &x, unsigned m)
{
    if (m == 0)
        return 0;
    if (x <= 1 || m == 1)
        return x;
    if (m >= 128)
        return (x > 0) ? U128{1} : U128{0};
    if (m == 2)
        return isqrt(x);

#if defined(__SIZEOF_INT128__)
    // --- 1. БЫСТРЫЙ ПУТЬ ДЛЯ GCC / LINUX (__int128) ---
    unsigned __int128 v = (static_cast<unsigned __int128>(x.high()) << 64) | x.low();

    // Начальное приближение
    uint32_t target_bits = (x.bit_width() + m - 1) / m;
    unsigned __int128 x0 = static_cast<unsigned __int128>(1) << target_bits;
    if (x0 > v)
        x0 = v;

    for (;;) {
        // Быстрое возведение x0 в степень (m - 1) на уровне __int128
        unsigned __int128 p = 1;
        unsigned __int128 base = x0;
        unsigned exp = m - 1;

        constexpr unsigned __int128 max_u128 = ~static_cast<unsigned __int128>(0);

        while (exp > 0) {
            if (exp & 1) {
                // Если зафиксировано переполнение, принудительно обнуляем p
                if (p > 0 && base > 0 && p > max_u128 / base) {
                    p = 0;
                    break;
                }
                p *= base;
            }
            exp >>= 1;
            if (exp > 0) {
                if (base > 0 && base > max_u128 / base) {
                    p = 0;
                    break;
                }
                base *= base;
            }
        }

        // Если p == 0 (или из-за переполнения, или изначально), частное равно 0
        unsigned __int128 quotient = (p == 0) ? 0 : (v / p);

        unsigned __int128 x1;
        if (x0 > quotient) {
            unsigned __int128 diff = (x0 - quotient) / m;
            if (diff == 0)
                x1 = x0 - 1;
            else
                x1 = x0 - diff;
        } else {
            return U128(static_cast<uint64_t>(x0), static_cast<uint64_t>(x0 >> 64));
        }

        if (x1 >= x0) {
            return U128(static_cast<uint64_t>(x0), static_cast<uint64_t>(x0 >> 64));
        }
        x0 = x1;
    }

#else
    // --- 2. НАДЕЖНЫЙ FALLBACK-ПУТЬ ДЛЯ MSVC / WINDOWS ---
    uint32_t target_bits = (x.bit_width() + m - 1) / m;
    U128 x0 = U128{1} << target_bits;
    if (x0 > x)
        x0 = x;

    U128 m_val{m};

    for (;;) {
        U128 p = int_power_fast(x0, m - 1);
        U128 quotient = (p == 0) ? U128{0} : (x / p);

        U128 x1;
        if (x0 > quotient) {
            U128 diff = (x0 - quotient) / m_val;
            if (diff == 0)
                x1 = x0 - 1;
            else
                x1 = x0 - diff;
        } else {
            return x0;
        }

        if (x1 >= x0)
            return x0;
        x0 = x1;
    }
#endif
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
std::map<U128, int> factor_internal(U128 x);

/**
 * @brief factor_parallel
 * @param x
 * @return 
 */
std::map<U128, int> factor_parallel(U128 x);

} // namespace utils

inline Globals::_gu128 Globals::global_u128;

} // namespace u128
