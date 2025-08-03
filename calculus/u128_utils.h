#pragma once

#include "u128.h"
#include <atomic>
#include <chrono>
#include <map>        // std::map
#include <random>
#include <vector>     // std::vector
#include <tuple>      // std::ignore, std::tie
#include <utility>    // std::pair
#include <functional> // std::function
#include "solver.h"   // GaussJordan

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

struct RandomGenerator {
    explicit RandomGenerator() {
        lfsr8::u64 tmp;
        mGenerator.seed(get_random_u32x4((lfsr8::u64)&tmp));
    }
    lfsr_rng_2::gens mGenerator;
};

inline U128 get_random_value() {
    U128 result;
    static RandomGenerator g_prng2;
    result.mLow = g_prng2.mGenerator.next_u64();
    result.mHigh = g_prng2.mGenerator.next_u64();
    g_prng2.mGenerator.next_u64();
    g_prng2.mGenerator.next_u64();
    return result;
}

inline U128 get_random_half_value() {
    U128 result;
    static RandomGenerator g_prng1;
    result.mLow = g_prng1.mGenerator.next_u64();
    result.mHigh = 0;
    g_prng1.mGenerator.next_u64();
    return result;
}

inline U128 int_power(ULOW x, int y)
{
    u128::U128 result{1};
    for (int i = 1; i <= y; ++i)
    {
        result = result * x;
    }
    return result;
}

/**
         * @brief Количество цифр числа.
         * @param x Число.
         * @return Количество цифр, минимум 1.
         */
inline int num_of_digits(U128 x)
{
    int i = 0;
    while (!x.is_zero())
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
    if (x.is_singular())
        return x;
    if (y.is_singular())
        return y;
    if (x == y)
    {
        return x;
    }
    if (x > y)
    {
        while (!y.is_zero())
        {
            const U128 y_copy = y;
            y = (x / y).second;
            x = y_copy;
        }
        return x;
    }
    else
    {
        while (!x.is_zero())
        {
            const U128 x_copy = x;
            x = (y / x).second;
            y = x_copy;
        }
        return y;
    }
}

/**
         * Целочисленный квадратный корень.
         * @param exact Точно ли прошло извлечение корня.
         */
inline U128 isqrt(U128 x, bool &exact)
{
    exact = false;
    if (x.is_singular())
    {
        return x;
    }
    const U128 c{ULOW(1) << U128::mHalfWidth, 0};
    U128 result;
    x = x.abs();
    if (x >= U128{0, 1})
    {
        result = c;
    }
    else
    {
        result = U128{ULOW(1) << (U128::mHalfWidth / 2), 0};
    }
    U128 prevprev = -U128{1};
    U128 prev = x;
    for (;;)
    {
        prevprev = prev;
        prev = result;
        const auto [tmp, remainder] = x / result;
        std::tie(result, std::ignore) = (result + tmp) / 2;
        if (result.is_zero())
        {
            exact = true;
            return result;
        }
        if (result == prev)
        {
            exact = (tmp == prev) && remainder.is_zero(); // Нет остатка от деления.
            return result;
        }
        if (result == prevprev)
        {
            return prev;
        }
    }
}

/**
         * @brief Является ли заданное число квадратичным вычетом.
         * @param x Тестируемое число.
         * @param p Простой модуль.
         * @return Да/нет.
         */
inline bool is_quadratiq_residue(U128 x, U128 p)
{
    // y^2 = x mod p
    auto [_, r1] = x / p;
    for (U128 y{0}; y < p; y.inc())
    {
        U128 sq = y * y;
        auto [_, r2] = sq / p;
        if (r2 == r1)
            return true;
    }
    return false;
}

/**
         * @brief Возвращает корень квадратный из заданного числа
         * по заданному модулю.
         * @param x Число.
         * @param p Простой модуль.
         * @return Два значения корня.
         */
inline std::pair<U128, U128> sqrt_mod(U128 x, U128 p)
{
    // return  sqrt(x) mod p
    U128 result[2];
    int idx = 0;
    const auto [_, r1] = x / p;
    for (U128 y{0}; y < p; y.inc())
    {
        U128 sq = y * y;
        auto [_, r2] = sq / p;
        if (r2 == r1)
            result[idx++] = y;
    }
    if (idx == 1)
    { // Если не был установлен второй корень.
        result[1] = result[0];
    }
    return std::make_pair(result[0], result[1]);
}

inline bool is_prime(U128 x)
{
    [[maybe_unused]] bool exact;
    const auto x_sqrt = isqrt(x, exact) + U128{1};
    U128 d{2, 0};
    bool is_ok = true;
    while (d < x_sqrt)
    {
        auto [_, remainder] = x / d;
        is_ok &= !remainder.is_zero();
        d.inc();
    }
    return is_ok;
}

class PrimesGenerator
{
public:
    U128 next()
    {
        if (mPrimes.empty())
        {
            mPrimes.push_back(U128{2, 0});
            return mPrimes.back();
        }
        U128 last_prime = mPrimes.back();
        for (;;)
        {
            bool is_prime = true;
            last_prime.inc();
            for (const auto &p : mPrimes)
            {
                U128 rem = (last_prime / p).second;
                if (rem.is_zero())
                {
                    is_prime = false;
                    break;
                }
            }
            if (is_prime)
            {
                mPrimes.push_back(last_prime);
                return mPrimes.back();
            }
        }
    }

private:
    std::vector<U128> mPrimes;
};

/**
         * @brief Делит первое число на второе до "упора".
         * @param x Делимое.
         * @param q Делитель.
         * @return Пара {Делитель, количество успешных делений}
         */
inline std::pair<U128, int> div_by_q(U128 &x, ULOW q)
{
    auto [tmp, remainder] = x / q;
    int i = 0;
    while (remainder.is_zero())
    {
        i++;
        x = tmp;
        std::tie(tmp, remainder) = x / q;
    }
    return std::make_pair(U128{q}, i);
}

inline std::pair<U128, U128> ferma_method(U128 x)
{
    U128 x_sqrt;
    {
        bool is_exact;
        x_sqrt = isqrt(x, is_exact);
        if (is_exact)
            return std::make_pair(x_sqrt, x_sqrt);
    }
    const auto error = x - x_sqrt * x_sqrt;
    auto y = U128{2} * x_sqrt + U128{1} - error;
    {
        bool is_exact;
        auto y_sqrt = isqrt(y, is_exact);
        const auto delta = x_sqrt + x_sqrt + U128{3, 0};
        y = y + delta;
        if (is_exact)
            return std::make_pair(x_sqrt + U128{1} - y_sqrt, x_sqrt + U128{1} + y_sqrt);
    }
    const auto &k_upper = x_sqrt;
    for (auto k = U128{2};; k.inc())
    {
        if (!(k.mLow & 65535) && Globals::LoadStop() ) // Проверка на стоп через каждые 65536 отсчетов.
            break;
        if (k > k_upper)
        {
            return std::make_pair(x, U128{1}); // x - простое число.
        }
        if (k.mLow % 2)
        { // Проверка с другой стороны: ускоряет поиск.
            // Основано на равенстве, следующем из метода Ферма: индекс k = (F^2 + x) / (2F) - floor(sqrt(x)).
            // Здесь F - кандидат в множители, x - раскладываемое число.
            const auto &N1 = k * k + x;
            if ((N1.mLow % 2) == 0)
            {
                auto [q1, remainder] = N1 / (k + k); // Здесь k как некоторый множитель F.
                if (remainder.is_zero() && (q1 > x_sqrt))
                {
                    auto [q2, remainder] = x / k;
                    if (remainder.is_zero()) // На всякий случай оставим.
                        return std::make_pair(k, q2);
                }
            }
        }
        if (const auto &r = y.mod10(); (r != 1 && r != 9)) // Просеиваем заведомо лишние.
            continue;
        bool is_exact;
        const auto y_sqrt = isqrt(y, is_exact);
        const auto delta = (x_sqrt + x_sqrt) + (k + k) + U128{1};
        y = y + delta;
        if (!is_exact)
            continue;
        const auto first_multiplier = x_sqrt + k - y_sqrt;
        return std::make_pair(first_multiplier, x_sqrt + k + y_sqrt);
    }
    return std::make_pair(x, U128{1}); // По какой-то причине не раскладывается.
};

inline std::map<U128, int> factor(U128 x)
{
    Globals::SetStop(false);
    if (x.is_zero())
    {
        return {{x, 1}};
    }
    if (x.is_unit())
    {
        return {{x, 1}};
    }
    if (x.is_singular())
    {
        return {{x, 1}};
    }
    x = x.abs();
    std::map<U128, int> result{};
    { // Обязательное для метода Ферма деление на 2.
        const auto& [p, i] = div_by_q(x, 2);
        if (i > 0)
            result[p] = i;
        if (x < U128{2, 0})
        {
            return result;
        }
    }
    // Делим на простые из списка: опционально.
    for (const auto &el : std::vector{3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37, 41, 43, 47, 53, 59, 61, 67, 71, 73, 79, 83, 89, 97, 101, 103, 107, 109, 113, 127, 131, 137, 139, 149, 151, 157, 163, 167, 173, 179, 181, 191, 193, 197, 199, 211, 223, 227, 229, 233, 239, 241, 251, 257, 263, 269, 271, 277, 281, 283, 293, 307, 311, 313, 317, 331, 337, 347, 349, 353, 359, 367, 373, 379, 383, 389, 397, 401, 409, 419, 421, 431, 433, 439, 443, 449, 457, 461, 463, 467, 479, 487, 491, 499, 503, 509, 521, 523, 541, 547, 557, 563, 569, 571, 577, 587, 593, 599, 601, 607, 613, 617, 619, 631, 641, 643, 647, 653, 659, 661, 673, 677, 683, 691, 701, 709, 719, 727, 733, 739, 743, 751, 757, 761, 769, 773, 787, 797, 809, 811, 821, 823, 827, 829, 839, 853, 857, 859, 863, 877, 881, 883, 887, 907, 911, 919, 929, 937, 941, 947, 953, 967, 971, 977, 983, 991, 997, 1009, 1013, 1019, 1021, 1031, 1033, 1039, 1049, 1051, 1061, 1063, 1069, 1087, 1091, 1093, 1097, 1103, 1109, 1117, 1123, 1129, 1151, 1153, 1163, 1171, 1181, 1187, 1193, 1201, 1213, 1217, 1223, 1229, 1231, 1237, 1249, 1259, 1277, 1279, 1283, 1289, 1291, 1297, 1301, 1303, 1307, 1319, 1321, 1327, 1361, 1367, 1373, 1381, 1399, 1409, 1423, 1427, 1429, 1433, 1439, 1447, 1451, 1453, 1459, 1471, 1481, 1483, 1487, 1489, 1493, 1499, 1511, 1523, 1531, 1543, 1549, 1553, 1559, 1567, 1571, 1579, 1583, 1597, 1601, 1607, 1609, 1613, 1619, 1621, 1627, 1637, 1657, 1663, 1667, 1669, 1693, 1697, 1699, 1709, 1721, 1723, 1733, 1741, 1747, 1753, 1759, 1777, 1783, 1787, 1789, 1801, 1811, 1823, 1831, 1847, 1861, 1867, 1871, 1873, 1877, 1879, 1889, 1901, 1907, 1913, 1931, 1933, 1949, 1951, 1973, 1979, 1987, 1993, 1997, 1999, 2003, 2011, 2017, 2027, 2029, 2039, 2053, 2063, 2069, 2081, 2083, 2087, 2089, 2099, 2111, 2113, 2129, 2131, 2137, 2141, 2143, 2153, 2161, 2179, 2203, 2207, 2213, 2221, 2237, 2239, 2243, 2251, 2267, 2269, 2273, 2281, 2287, 2293, 2297, 2309, 2311, 2333, 2339, 2341, 2347, 2351, 2357, 2371, 2377, 2381, 2383, 2389, 2393, 2399, 2411, 2417, 2423, 2437, 2441, 2447, 2459, 2467, 2473, 2477, 2503, 2521, 2531, 2539, 2543, 2549, 2551, 2557, 2579, 2591, 2593, 2609, 2617, 2621, 2633, 2647, 2657, 2659, 2663, 2671, 2677, 2683, 2687, 2689, 2693, 2699, 2707, 2711, 2713, 2719, 2729, 2731, 2741, 2749, 2753, 2767, 2777, 2789, 2791, 2797, 2801, 2803, 2819, 2833, 2837, 2843, 2851, 2857, 2861, 2879, 2887, 2897, 2903, 2909, 2917, 2927, 2939, 2953, 2957, 2963, 2969, 2971, 2999, 3001, 3011, 3019, 3023, 3037, 3041, 3049, 3061, 3067, 3079, 3083, 3089, 3109, 3119, 3121, 3137, 3163, 3167, 3169, 3181, 3187, 3191, 3203, 3209, 3217, 3221, 3229, 3251, 3253, 3257, 3259, 3271, 3299, 3301, 3307, 3313, 3319, 3323, 3329, 3331, 3343, 3347, 3359, 3361, 3371, 3373, 3389, 3391, 3407, 3413, 3433, 3449, 3457, 3461, 3463, 3467, 3469, 3491, 3499, 3511, 3517, 3527, 3529, 3533, 3539, 3541, 3547, 3557, 3559, 3571, 3581, 3583, 3593, 3607, 3613, 3617, 3623, 3631, 3637, 3643, 3659, 3671, 3673, 3677, 3691, 3697, 3701, 3709, 3719, 3727, 3733, 3739, 3761, 3767, 3769, 3779, 3793, 3797, 3803, 3821, 3823, 3833, 3847, 3851, 3853, 3863, 3877, 3881, 3889, 3907, 3911, 3917, 3919, 3923, 3929, 3931, 3943, 3947, 3967, 3989, 4001, 4003, 4007, 4013, 4019, 4021, 4027, 4049, 4051, 4057, 4073, 4079, 4091, 4093, 4099, 4111, 4127, 4129, 4133, 4139, 4153, 4157, 4159, 4177, 4201, 4211, 4217, 4219, 4229, 4231, 4241, 4243, 4253, 4259, 4261, 4271, 4273, 4283, 4289, 4297, 4327, 4337, 4339, 4349, 4357, 4363, 4373, 4391, 4397, 4409, 4421, 4423, 4441, 4447, 4451, 4457, 4463, 4481, 4483, 4493, 4507, 4513, 4517, 4519, 4523, 4547, 4549, 4561, 4567, 4583, 4591, 4597, 4603, 4621, 4637, 4639, 4643, 4649, 4651, 4657, 4663, 4673, 4679, 4691, 4703, 4721, 4723, 4729, 4733, 4751, 4759, 4783, 4787, 4789, 4793, 4799, 4801, 4813, 4817, 4831, 4861, 4871, 4877, 4889, 4903, 4909, 4919, 4931, 4933, 4937, 4943, 4951, 4957, 4967, 4969, 4973, 4987, 4993, 4999, 5003, 5009, 5011, 5021, 5023, 5039, 5051, 5059, 5077, 5081, 5087, 5099, 5101, 5107, 5113, 5119, 5147, 5153, 5167, 5171, 5179, 5189, 5197, 5209, 5227, 5231, 5233, 5237, 5261, 5273, 5279, 5281, 5297, 5303, 5309, 5323, 5333, 5347, 5351, 5381, 5387, 5393, 5399, 5407, 5413, 5417, 5419, 5431, 5437, 5441, 5443, 5449, 5471, 5477, 5479, 5483, 5501, 5503, 5507, 5519, 5521, 5527, 5531, 5557, 5563, 5569, 5573, 5581, 5591, 5623, 5639, 5641, 5647, 5651, 5653, 5657, 5659, 5669, 5683, 5689, 5693, 5701, 5711, 5717, 5737, 5741, 5743, 5749, 5779, 5783, 5791, 5801, 5807, 5813, 5821, 5827, 5839, 5843, 5849, 5851, 5857, 5861, 5867, 5869, 5879, 5881, 5897, 5903, 5923, 5927, 5939, 5953, 5981, 5987, 6007, 6011, 6029, 6037, 6043, 6047, 6053, 6067, 6073, 6079, 6089, 6091, 6101, 6113, 6121, 6131, 6133, 6143, 6151, 6163, 6173, 6197, 6199, 6203, 6211, 6217, 6221, 6229, 6247, 6257, 6263, 6269, 6271, 6277, 6287, 6299, 6301, 6311, 6317, 6323, 6329, 6337, 6343, 6353, 6359, 6361, 6367, 6373, 6379, 6389, 6397, 6421, 6427, 6449, 6451, 6469, 6473, 6481, 6491, 6521, 6529, 6547, 6551, 6553, 6563, 6569, 6571, 6577, 6581, 6599, 6607, 6619, 6637, 6653, 6659, 6661, 6673, 6679, 6689, 6691, 6701, 6703, 6709, 6719, 6733, 6737, 6761, 6763, 6779, 6781, 6791, 6793, 6803, 6823, 6827, 6829, 6833, 6841, 6857, 6863, 6869, 6871, 6883, 6899, 6907, 6911, 6917, 6947, 6949, 6959, 6961, 6967, 6971, 6977, 6983, 6991, 6997, 7001, 7013, 7019, 7027, 7039, 7043, 7057, 7069, 7079, 7103, 7109, 7121, 7127, 7129, 7151, 7159, 7177, 7187, 7193, 7207, 7211, 7213, 7219, 7229, 7237, 7243, 7247, 7253, 7283, 7297, 7307, 7309, 7321, 7331, 7333, 7349, 7351, 7369, 7393, 7411, 7417, 7433, 7451, 7457, 7459, 7477, 7481, 7487, 7489, 7499, 7507, 7517, 7523, 7529, 7537, 7541, 7547, 7549, 7559, 7561, 7573, 7577, 7583, 7589, 7591, 7603, 7607, 7621, 7639, 7643, 7649, 7669, 7673, 7681, 7687, 7691, 7699, 7703, 7717, 7723, 7727, 7741, 7753, 7757, 7759, 7789, 7793, 7817, 7823, 7829, 7841, 7853, 7867, 7873, 7877, 7879, 7883, 7901, 7907, 7919})
    {
        const auto& [p, i] = div_by_q(x, el);
        if (i > 0)
            result[p] = i;
        if (x < U128{2, 0})
        {
            return result;
        }
    }
    // Применяем метод Ферма рекурсивно.
    std::function<void(U128)> ferma_recursive;
    ferma_recursive = [&ferma_recursive, &result](U128 x) -> void
    {
        const auto& [a, b] = ferma_method(x);
        if (a == U128{1, 0})
        {
            result[b]++;
            return;
        }
        else if (b == U128{1, 0})
        {
            result[a]++;
            return;
        }
        ferma_recursive(a);
        ferma_recursive(b);
    };
    ferma_recursive(x);
    Globals::SetStop(false);
    return result;
}

/**
         * @brief Разложить на простые множители методом квадратичного решета.
         * @param x Число.
         * @param sieve_size Размер решета, больше нуля.
         * @param factor_base Фактор-база (количество простых чисел-базисов), больше нуля.
         * @return Результат разложения.
         */
inline std::map<U128, int> factor_qs(U128 x, unsigned int sieve_size, unsigned int factor_base)
{
    std::map<U128, int> result{};
    if (sieve_size == 0 || factor_base == 0)
    {
        return result;
    }
    auto find_a_divisor = [sieve_size, factor_base](U128 x) -> U128
    {
        if (x.is_zero())
            return x;
        if (x.is_unit())
            return x;
        std::vector<U128> base;
        PrimesGenerator pg;
        for (; base.size() < factor_base;)
        {
            const U128 &p = pg.next();
            if (is_quadratiq_residue(x, p))
            {
                base.push_back(p);
            }
        }
        bool is_exact;
        U128 x_sqrt = isqrt(x, is_exact);
        if (!is_exact)
        {
            x_sqrt.inc();
        }
        std::vector<U128> sieve(sieve_size);
        U128 ii{0, 0};
        for (unsigned int i = 0; i < sieve.size(); ++i)
        {
            sieve[i] = (ii + x_sqrt) * (ii + x_sqrt) - x;
            ii.inc();
        }
        std::vector<U128> sieve_original = sieve;
        for (const U128 &modulo : base)
        {
            auto [root_1, root_2] = sqrt_mod(x, modulo);
            root_1 -= x_sqrt;
            root_2 -= x_sqrt;
            if (root_1.is_negative())
            {
                U128 delta_1 = (root_1.abs() / modulo).first;
                root_1 += delta_1 * modulo;
            }
            if (root_1.is_negative())
            {
                root_1 += modulo;
            }
            if (root_2.is_negative())
            {
                U128 delta_2 = (root_2.abs() / modulo).first;
                root_2 += delta_2 * modulo;
            }
            if (root_2.is_negative())
            {
                root_2 += modulo;
            }
            unsigned int idx = root_1.mLow;
            while ((idx + 1) < sieve.size())
            {
                sieve[idx] = (sieve.at(idx) / modulo).first;
                idx += modulo.mLow;
            }
            if (root_1 != root_2)
            {
                unsigned int idx = root_2.mLow;
                while ((idx + 1) < sieve.size())
                {
                    sieve[idx] = (sieve.at(idx) / modulo).first;
                    idx += modulo.mLow;
                }
            }
        }
        std::vector<unsigned int> indices_where_unit_sieve;
        for (unsigned int i = 0; i < sieve.size(); ++i)
        {
            if (sieve.at(i).is_unit())
            {
                indices_where_unit_sieve.push_back(i);
            }
        }
        sieve.clear();
        std::vector<std::vector<int>> M;
        std::vector<U128> sieve_reduced;
        int idx = 0;
        for (const auto &index : indices_where_unit_sieve)
        {
            M.push_back({});
            const U128 &value = sieve_original.at(index);
            sieve_reduced.push_back(value);
            for (const U128 &modulo : base)
            {
                const auto &[_, r] = value / modulo;
                M[idx].push_back(r.is_zero() ? 1 : 0);
            }
            idx++;
        }
        sieve_original.clear();
        const std::vector<std::set<int>> &solved_indices = solver::GaussJordan(M);
        M.clear();
        for (const auto &indices : solved_indices)
        {
            U128 A{1};
            std::map<U128, int> B_factors;
            for (auto it = indices.begin(); it != indices.end(); it++)
            {
                const auto index = indices_where_unit_sieve.at(*it);
                std::map<U128, int> sieve_factors;
                {
                    const auto &val = sieve_reduced.at(*it);
                    for (const auto &modulo : base)
                    {
                        auto rem = (val / modulo).second;
                        if (rem.is_zero())
                        {
                            sieve_factors[modulo]++;
                        }
                    }
                }
                A = A * (x_sqrt + U128{index, 0});
                for (const auto &[prime, power] : sieve_factors)
                {
                    B_factors[prime] += power;
                }
                if (A.is_singular())
                {
                    continue;
                }
            }
            for (auto &element : B_factors)
            {
                element.second /= 2;
            }
            U128 B{1};
            for (const auto &[prime, power] : B_factors)
            {
                U128 tmp{1};
                for (int i = 0; i < power; ++i)
                    tmp = tmp * prime;
                B = B * tmp;
            }
            const U128 &C = A - B;
            const U128 &GCD = gcd(C, x);
            if (GCD < x && GCD > U128{1})
            {
                return GCD;
            }
        }
        return x;
    }; // find_a_divisor()
    U128 y{1};
    for (;;)
    {
        const auto &divisor1 = find_a_divisor(x);
        const auto &divisor2 = find_a_divisor(y);
        if (divisor1.is_unit() && divisor2.is_unit())
            break;
        if (divisor2 == y && !divisor2.is_unit())
        {
            result[divisor2]++;
        }
        if (divisor1 == x && !divisor1.is_unit())
        {
            result[divisor1]++;
            y = U128{1};
        }
        else
        {
            y = divisor1;
        }
        x = (x / divisor1).first;
    }
    return result;
}

inline U128 get_by_digit(int digit)
{
    return U128{static_cast<u128::ULOW>(digit), 0};
}

} // namespace utils

inline Globals::_gu128 Globals::global_u128;

} // namespace u128
