/**
 * @file ecm_factorizer.hpp
 * @brief ECM (метод Ленстры) на кривых Монтгомери.
 * @details Портировано из проекта factorizer (bignum::generic_ecm) и адаптировано
 *          под 128-битные числа калькулятора: T = bignum::u128::U128.
 *          Использует общие модульные хелперы из u128::siqs и редуктор Барретта
 *          для модульного умножения без делений.
 */

#pragma once

#include "u128.hpp"
#include "ubig.hpp"
#include "siqs.hpp"
#include "pollard.hpp"

#include <atomic>
#include <bit>
#include <cassert>
#include <cstdint>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace u128::ecm
{

using bignum::u128::U128;

using u128::pollard::mod_add;
using u128::pollard::mod_sub;
using u128::pollard::mod_double;

// --- Редуктор Барретта: модульное умножение без делений ---

template <typename T>
class BarrettReducer
{
    using DoubleT = bignum::UBig<T>;

    T m_mod;
    DoubleT m_mu;
    uint32_t m_k = 0;

public:
    BarrettReducer() = default;

    explicit BarrettReducer(const T &mod) : m_mod(mod)
    {
        if (mod <= T(3))
            return;
        m_k = mod.bit_width();

        // mu = floor(2^(2k) / mod) вычисляем через деление 2^(2k - 1) / mod.
        // Сдвиг (2k - 1) строго меньше размера DoubleT и не вызывает переполнения:
        // для U128 это 255 < 256.
        DoubleT half_pow = DoubleT(1) << (2 * m_k - 1);
        auto qr = u128::siqs::div_rem(half_pow, DoubleT(mod));

        DoubleT r_double = qr.second << 1;
        DoubleT mod_wide = DoubleT(mod);
        if (r_double >= mod_wide)
        {
            m_mu = (qr.first << 1) + DoubleT(1);
        }
        else
        {
            m_mu = qr.first << 1;
        }

        // Проверка корректности редукции (только для отладочных сборок).
        if (mod > T(7))
        {
            T a = mod - T(2);
            T b = mod - T(3);
            assert(mul(a, b) == T(6));
            assert(sqr(mod - T(1)) == T(1));
        }
    }

    const T &modulus() const { return m_mod; }

    inline T mul(const T &x, const T &y) const
    {
        DoubleT z = DoubleT::mult_ext(x, y);
        return reduce(z);
    }

    inline T sqr(const T &x) const
    {
        DoubleT z = DoubleT::square_ext(x);
        return reduce(z);
    }

    inline T reduce(const DoubleT &z) const
    {
        if (z < DoubleT(m_mod))
        {
            return z.low();
        }

        // 1. q1 = z >> (k + 1)
        DoubleT q1 = z >> (m_k + 1);

        // 2. q2 = (q1 * mu) >> (k - 1)
        // Произведение (q1 * mu) строго < 2^(2k - 2) и помещается в DoubleT.
        DoubleT q1_mu = q1 * m_mu;
        DoubleT q2 = q1_mu >> (m_k - 1);

        // 3. r = z - q2 * m_mod
        DoubleT prod = q2 * DoubleT(m_mod);
        DoubleT r_wide = (z >= prod) ? (z - prod) : DoubleT(0);

        // Финальная коррекция: оценка частного ошибается не более чем на 5 единиц.
        while (r_wide >= DoubleT(m_mod))
        {
            r_wide = r_wide - DoubleT(m_mod);
        }

        return r_wide.low();
    }
};

// --- Арифметика кривых Монтгомери ---

template <typename T>
struct MontgomeryPoint
{
    T X;
    T Z;
};

// --- Безопасный бинарный инвертор a^(-1) mod m (для нечётного m) ---

template <typename T>
std::optional<T> inv_mod_binary(const T &a_in, const T &m)
{
    if (m <= T(1))
        return std::nullopt;
    T a = u128::siqs::div_r(a_in, m);
    if (a == T(0))
        return std::nullopt;

    T u = a;
    T v = m;
    T x1 = T(1);
    T x2 = T(0);

    // (x + m) / 2 без переполнения: x и m нечётны, значит (x + m) / 2 = (x >> 1) + (m >> 1) + 1.
    auto safe_add_m_div2 = [](const T &x, const T &mod) -> T {
        return (x >> 1) + (mod >> 1) + T(1);
    };

    while (u != T(0) && v != T(0))
    {
        while ((u & T(1)) == T(0))
        {
            u = u >> 1;
            if ((x1 & T(1)) != T(0))
            {
                x1 = safe_add_m_div2(x1, m);
            }
            else
            {
                x1 = x1 >> 1;
            }
        }

        while ((v & T(1)) == T(0))
        {
            v = v >> 1;
            if ((x2 & T(1)) != T(0))
            {
                x2 = safe_add_m_div2(x2, m);
            }
            else
            {
                x2 = x2 >> 1;
            }
        }

        if (u >= v)
        {
            u = u - v;
            x1 = mod_sub(x1, x2, m);
        }
        else
        {
            v = v - u;
            x2 = mod_sub(x2, x1, m);
        }
    }

    if (v == T(1))
        return x2;
    if (u == T(1))
        return x1;

    // НОД(a, m) > 1: делитель перехватывается снаружи через gcd.
    return std::nullopt;
}

// --- Канонические формулы Монтгомери (при условии Z_diff = 1) ---

template <typename T>
inline MontgomeryPoint<T> montgomery_double(const MontgomeryPoint<T> &P,
                                            const T &A24,
                                            const BarrettReducer<T> &br)
{
    const T &n = br.modulus();
    T u = mod_add(P.X, P.Z, n);
    T v = mod_sub(P.X, P.Z, n);

    T u2 = br.sqr(u);
    T v2 = br.sqr(v);
    T diff = mod_sub(u2, v2, n); // 4 * X * Z

    T X2 = br.mul(u2, v2);

    T term = br.mul(diff, A24);
    term = mod_add(v2, term, n);

    T Z2 = br.mul(diff, term);

    return {X2, Z2};
}

template <typename T>
inline MontgomeryPoint<T> montgomery_add(const MontgomeryPoint<T> &P,
                                         const MontgomeryPoint<T> &Q,
                                         const T &x_diff,
                                         const BarrettReducer<T> &br)
{
    const T &n = br.modulus();
    T u1 = mod_add(P.X, P.Z, n);
    T v1 = mod_sub(P.X, P.Z, n);
    T u2 = mod_add(Q.X, Q.Z, n);
    T v2 = mod_sub(Q.X, Q.Z, n);

    T t1 = br.mul(u1, v2);
    T t2 = br.mul(v1, u2);

    T add_t = mod_add(t1, t2, n);
    T sub_t = mod_sub(t1, t2, n);

    T X_out = br.sqr(add_t);
    T Z_out = br.sqr(sub_t);
    Z_out = br.mul(Z_out, x_diff);

    return {X_out, Z_out};
}

// --- Решето простых для Stage 2 ---

inline std::vector<uint8_t> make_prime_sieve(uint32_t max_val)
{
    std::vector<uint8_t> is_prime(max_val + 1, 1);
    if (max_val >= 0)
        is_prime[0] = 0;
    if (max_val >= 1)
        is_prime[1] = 0;
    for (uint32_t p = 2; 1ULL * p * p <= max_val; ++p)
    {
        if (is_prime[p])
        {
            for (uint32_t i = p * p; i <= max_val; i += p)
            {
                is_prime[i] = 0;
            }
        }
    }
    return is_prime;
}

// --- Умножение точки на скаляр через лестницу Монтгомери ---

template <typename T>
MontgomeryPoint<T> montgomery_multiply(uint32_t k,
                                       const MontgomeryPoint<T> &P,
                                       const T &A24,
                                       const BarrettReducer<T> &br)
{
    MontgomeryPoint<T> R0 = P;
    MontgomeryPoint<T> R1 = montgomery_double(P, A24, br);

    int msb = 31 - std::countl_zero(k);
    for (int bit = msb - 1; bit >= 0; --bit)
    {
        if ((k >> bit) & 1)
        {
            R0 = montgomery_add(R1, R0, P.X, br);
            R1 = montgomery_double(R1, A24, br);
        }
        else
        {
            R1 = montgomery_add(R0, R1, P.X, br);
            R0 = montgomery_double(R0, A24, br);
        }
    }
    return R0;
}

// --- ECM Stage 2: Baby-Step Giant-Step (BSGS) ---

template <typename T>
std::optional<T> ecm_stage2(MontgomeryPoint<T> &Q,
                            const T &A24,
                            const BarrettReducer<T> &br,
                            uint32_t B1,
                            uint32_t B2,
                            const std::vector<uint8_t> &is_prime_table)
{
    const T &n = br.modulus();

    static const uint32_t J[] = {
        1, 11, 13, 17, 19, 23, 29, 31, 37, 41, 43, 47,
        53, 59, 61, 67, 71, 73, 79, 83, 89, 97, 101, 103
    };

    // 1. Baby-steps: вычисляем S_j = j * Q (в проективных координатах)
    MontgomeryPoint<T> baby[106];
    baby[1] = Q;

    for (uint32_t j : J)
    {
        if (j == 1)
            continue;
        baby[j] = montgomery_multiply(j, Q, A24, br);
        if (baby[j].Z == T(0))
        {
            T g = u128::siqs::gcd_generic(baby[j].X, n);
            if (g > T(1) && g < n)
                return g;
            return std::nullopt;
        }
    }

    // 2. Giant-steps setup: G1 = 210 * Q, G2 = 2 * G1
    MontgomeryPoint<T> G1 = montgomery_multiply(210, Q, A24, br);
    if (G1.Z == T(0))
        return std::nullopt;

    auto g1_z_inv = inv_mod_binary(G1.Z, n);
    if (!g1_z_inv.has_value())
    {
        T g = u128::siqs::gcd_generic(G1.Z, n);
        if (g > T(1) && g < n)
            return g;
        return std::nullopt;
    }
    G1.X = br.mul(G1.X, *g1_z_inv);
    G1.Z = T(1);

    MontgomeryPoint<T> G_prev = G1;
    MontgomeryPoint<T> G_curr = montgomery_double(G1, A24, br);

    uint32_t m_max = (B2 + 105) / 210 + 1;
    T acc = T(1);
    int acc_count = 0;

    for (uint32_t m = 2; m <= m_max; ++m)
    {
        if (u128::Globals::LoadStop())
            return std::nullopt;

        if (m * 210 + 105 > B1)
        {
            for (uint32_t j : J)
            {
                bool need = false;
                uint32_t q_plus = m * 210 + j;
                if (q_plus > B1 && q_plus <= B2 && is_prime_table[q_plus])
                    need = true;
                if (!need && m * 210 >= j)
                {
                    uint32_t q_minus = m * 210 - j;
                    if (q_minus > B1 && q_minus <= B2 && is_prime_table[q_minus])
                        need = true;
                }

                if (need)
                {
                    // Перекрёстное умножение без делений через Барретта:
                    T t1 = br.mul(G_curr.X, baby[j].Z);
                    T t2 = br.mul(baby[j].X, G_curr.Z);
                    T diff = (t1 >= t2) ? (t1 - t2) : (n - (t2 - t1));

                    if (diff != T(0))
                    {
                        acc = br.mul(acc, diff);
                        acc_count++;
                    }
                }
            }
        }

        if (acc_count >= 128)
        {
            T g = u128::siqs::gcd_generic(acc, n);
            if (g > T(1) && g < n)
                return g;
            if (g == n)
                return std::nullopt;
            acc = T(1);
            acc_count = 0;
        }

        if (m < m_max)
        {
            MontgomeryPoint<T> G_next = montgomery_add(G_curr, G1, G_prev.X, br);
            G_prev = G_curr;
            G_curr = G_next;
        }
    }

    if (acc_count > 0)
    {
        T g = u128::siqs::gcd_generic(acc, n);
        if (g > T(1) && g < n)
            return g;
    }

    return std::nullopt;
}

// --- Запуск одной кривой ECM (Stage 1 + Stage 2) ---

template <typename T>
std::optional<T> ecm_one_curve(const T &n, uint64_t seed, uint32_t B1, uint32_t B2,
                               const std::vector<unsigned> &primes,
                               const std::vector<uint8_t> &is_prime_table)
{
    BarrettReducer<T> br(n); // Редуктор Барретта создаётся один раз на кривую

    uint64_t sigma_val = (seed % 1000000000ULL) + 6;
    T sigma(sigma_val);

    T sigma2 = br.sqr(sigma);
    T u = mod_sub(sigma2, T(5), n);
    T v = mod_double(mod_double(sigma, n), n);

    T u3 = br.mul(br.sqr(u), u);
    T v3 = br.mul(br.sqr(v), v);

    auto v3_inv = inv_mod_binary(v3, n);
    if (!v3_inv.has_value())
    {
        T g = u128::siqs::gcd_generic(v3, n);
        if (g > T(1) && g < n)
            return g;
        return std::nullopt;
    }
    T x0 = br.mul(u3, *v3_inv);
    MontgomeryPoint<T> P{x0, T(1)};

    T diff_vu = mod_sub(v, u, n);
    T diff_vu3 = br.mul(br.sqr(diff_vu), diff_vu);

    T sum_3uv = mod_add(mod_add(mod_double(u, n), u, n), v, n);
    T A_num = br.mul(diff_vu3, sum_3uv);

    T denom = br.mul(u3, v);
    denom = mod_double(mod_double(mod_double(mod_double(denom, n), n), n), n); // * 16

    auto denom_inv = inv_mod_binary(denom, n);
    if (!denom_inv.has_value())
    {
        T g = u128::siqs::gcd_generic(denom, n);
        if (g > T(1) && g < n)
            return g;
        return std::nullopt;
    }
    T A24 = br.mul(A_num, *denom_inv);

    // === Stage 1 с пакетированием скаляров ===
    uint64_t current_k = 1;

    for (unsigned p : primes)
    {
        if (p > B1)
            break;
        if (u128::Globals::LoadStop())
            return std::nullopt;

        uint32_t p_pow = p;
        while (1ULL * p_pow * p <= B1)
        {
            p_pow *= p;
        }

        if (current_k * p_pow > 0xFFFFFFFFULL)
        {
            P = montgomery_multiply(static_cast<uint32_t>(current_k), P, A24, br);
            if (P.Z == T(0))
                return std::nullopt;

            auto z_inv = inv_mod_binary(P.Z, n);
            if (!z_inv.has_value())
            {
                T g = u128::siqs::gcd_generic(P.Z, n);
                if (g > T(1) && g < n)
                    return g;
                return std::nullopt;
            }
            P.X = br.mul(P.X, *z_inv);
            P.Z = T(1);

            current_k = p_pow;
        }
        else
        {
            current_k *= p_pow;
        }
    }

    if (current_k > 1)
    {
        P = montgomery_multiply(static_cast<uint32_t>(current_k), P, A24, br);
        if (P.Z == T(0))
            return std::nullopt;

        auto z_inv = inv_mod_binary(P.Z, n);
        if (!z_inv.has_value())
        {
            T g = u128::siqs::gcd_generic(P.Z, n);
            if (g > T(1) && g < n)
                return g;
            return std::nullopt;
        }
        P.X = br.mul(P.X, *z_inv);
        P.Z = T(1);
    }

    // === Stage 2 (BSGS) ===
    if (B2 > B1)
    {
        return ecm_stage2(P, A24, br, B1, B2, is_prime_table);
    }

    return std::nullopt;
}

// --- Параллельный диспетчер ECM ---

template <typename T>
std::optional<T> ecm_parallel(const T &n,
                              unsigned max_curves,
                              uint32_t B1,
                              uint32_t B2,
                              std::function<void(const std::string &)> status_cb)
{
    auto primes = u128::utils::primes(B1);
    auto is_prime_table = make_prime_sieve(B2);

    unsigned num_threads = std::thread::hardware_concurrency();
    if (num_threads == 0)
        num_threads = 4;

    std::atomic<bool> found{false};
    std::atomic<uint32_t> curve_counter{0};
    std::mutex res_mutex;
    std::optional<T> result = std::nullopt;

    std::vector<std::thread> workers;
    workers.reserve(num_threads);

    for (unsigned t = 0; t < num_threads; ++t)
    {
        workers.emplace_back([&, t]() {
            while (!found.load(std::memory_order_relaxed) &&
                   !u128::Globals::LoadStop())
            {
                uint32_t c_idx = curve_counter.fetch_add(1);
                if (c_idx >= max_curves)
                    break;

                if (status_cb && c_idx % 4 == 0)
                {
                    status_cb("ECM: Тестирование кривых (" + std::to_string(c_idx) +
                              " / " + std::to_string(max_curves) + "), B1=" + std::to_string(B1) +
                              ", B2=" + std::to_string(B2));
                }

                uint64_t seed = 0x9E3779B97F4A7C15ULL * (c_idx + 1) + t * 9973;
                auto factor = ecm_one_curve(n, seed, B1, B2, primes, is_prime_table);

                if (factor.has_value())
                {
                    std::lock_guard<std::mutex> lock(res_mutex);
                    if (!result.has_value())
                    {
                        result = factor;
                        found.store(true, std::memory_order_relaxed);
                    }
                    return;
                }
            }
        });
    }

    for (auto &w : workers)
    {
        if (w.joinable())
            w.join();
    }

    return result;
}

// --- Лестница параметров ECM, подобранная под 128-битные числа ---
// (первые ступени конвейера factorizer; далее GNFS-диапазон не нужен).

struct EcmStage
{
    unsigned curves;
    uint32_t B1;
    uint32_t B2;
};

inline const EcmStage ECM_PLAN[] = {
    {25, 2000, 60000},
    {50, 10000, 300000},
    {150, 50000, 1500000},
    {450, 110000, 3500000},
};

/**
 * @brief Пытается найти нетривиальный делитель числа n методом ECM.
 * @return Делитель d (1 < d < n) или std::nullopt, если не удалось.
 */
inline std::optional<U128> factorize(const U128 &n)
{
    for (const EcmStage &stage : ECM_PLAN)
    {
        if (u128::Globals::LoadStop())
            break;

        auto f = ecm_parallel(n, stage.curves, stage.B1, stage.B2, nullptr);
        if (f.has_value() && f.value() > 1 && f.value() < n)
        {
            return f;
        }
    }
    return std::nullopt;
}

} // namespace u128::ecm
