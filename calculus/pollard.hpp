/**
 * @file pollard.hpp
 * @brief Методы Полларда (p-1 и ро-Брента).
 * @details Портировано из проекта factorizer (bignum::generic_ecm). Работает
 *          поверх общих модульных хелперов из u128::siqs, рассчитано на U128.
 */

#pragma once

#include "u128.hpp"
#include "siqs.hpp"

#include <algorithm>
#include <cstdint>
#include <optional>

namespace u128::pollard
{

// --- Сложение/вычитание по модулю N без переполнения ---

template <typename T>
inline T mod_add(const T &a, const T &b, const T &m)
{
    T res = a + b;
    if (res >= m || res < a)
    {
        res = res - m;
    }
    return res;
}

template <typename T>
inline T mod_sub(const T &a, const T &b, const T &m)
{
    if (a >= b)
    {
        return a - b;
    }
    return m - (b - a);
}

template <typename T>
inline T mod_double(const T &a, const T &m)
{
    return mod_add(a, a, m);
}

// --- 1. Pollard p - 1 ---

template <typename T>
std::optional<T> pollard_pm1(const T &n, uint32_t B1 = 100000)
{
    auto primes = u128::utils::primes(B1);
    T a = T(2);

    for (uint32_t p : primes)
    {
        if (u128::Globals::LoadStop())
            return std::nullopt;

        uint32_t p_pow = p;
        while (1ULL * p_pow * p <= B1)
        {
            p_pow *= p;
        }

        u128::siqs::int_power_mod(a, T(p_pow), n);

        T diff = (a > T(1)) ? (a - T(1)) : (n - T(1));
        T g = u128::siqs::gcd_generic(diff, n);
        if (g > T(1) && g < n)
        {
            return g;
        }
        if (g == n)
        {
            break;
        }
    }
    return std::nullopt;
}

// --- 2. Pollard-Brent rho с пакетированием GCD ---

template <typename T>
T pollard_brent_rho(const T &n, uint64_t max_steps = 200000)
{
    if (n <= T(1))
        return n;
    if ((n & T(1)) == T(0))
        return T(2);

    T y = T(2);
    T c = T(1);
    T g = T(1);
    T q = T(1);
    T ys = T(0);
    T x = T(0);

    uint64_t r = 1;
    uint64_t steps = 0;

    while (g == T(1) && steps < max_steps && !u128::Globals::LoadStop())
    {
        x = y;
        for (uint64_t i = 0; i < r; ++i)
        {
            u128::siqs::square_mod(y, n);
            y = mod_add(y, c, n);
        }

        uint64_t k = 0;
        while (k < r && g == T(1) && steps < max_steps && !u128::Globals::LoadStop())
        {
            ys = y;
            uint64_t limit = std::min<uint64_t>(128, r - k);
            for (uint64_t i = 0; i < limit; ++i)
            {
                u128::siqs::square_mod(y, n);
                y = mod_add(y, c, n);
                T diff = (x >= y) ? (x - y) : (y - x);
                u128::siqs::mult_mod(q, diff, n);
            }

            g = u128::siqs::gcd_generic(q, n);
            k += limit;
            steps += limit;
        }

        r *= 2;
    }

    if (g == n)
    {
        do
        {
            u128::siqs::square_mod(ys, n);
            ys = mod_add(ys, c, n);
            T diff = (x >= ys) ? (x - ys) : (ys - x);
            g = u128::siqs::gcd_generic(diff, n);
        } while (g == T(1));
    }

    if (g > T(1) && g < n)
    {
        return g;
    }

    return T(1);
}

} // namespace u128::pollard
