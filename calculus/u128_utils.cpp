#include "u128_utils.h"
#include "rand_u128.h"
#include "i128.hpp"
#include "ecm_factorizer.h"

#include <list>
#include <functional>
#include <optional>

namespace u128::utils
{

std::pair<U128, int> div_by_q(U128 &x, const U128& q)
{
    auto quotient = x / q;
    auto remainder = x % q;
    int i = 0;
    while (remainder == 0)
    {
        i++;
        x = quotient;
        quotient = x / q;
        remainder = x % q;
    }
    return std::make_pair(U128{q}, i);
}

bool miller_test(U128 d, const U128& n)
{
    U128 x = get_random_value_ab(2, n - 2);
    int_power_mod(x, d, n);
    if ((x == 1) || (x == (n - 1)))
        return true;

    while (d != (n - 1))
    {
        int_power_mod(x, 2, n);
        d <<= 1;
        if (x == 1)
            return false;
        if (x == (n - 1))
            return true;
    }
    return false;
}

bool is_prime(U128 x, int k)
{
    if ((x <= 1) || (x == 4))
        return false;
    if (x <= 3)
        return true;
    if ((x & 1) == 0)
        return false;
    U128 d {x - 1};
    while ((d & 1) == 0)
        d >>= 1;
    for (int i = 0; i < k; ++i) {
        if (!miller_test(d, x))
            return false;
    }
    return true;
}

U128 modular_inverse(U128 a, U128 m, bool &success)
{
    using namespace bignum::i128;
    const I128 m0 = m;
    I128 y = 0;
    I128 x = 1;
    success = false;
    if (m == 1)
        return 0;
    while (a > 1) {
        if (m == 0)
            return U128{};
        const I128 q = a / m;
        I128 temp = m;
        m = a % m;
        a = temp.unsigned_part();
        temp = y;
        y = x - q * y;
        x = temp;
    }
    x += x < 0 ? m0 : 0;
    success = true;
    return x.unsigned_part();
}

std::pair<U128, U128> ferma_method(U128 x)
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
        y += delta;
        if (is_exact)
            return std::make_pair(x_sqrt + U128{1} - y_sqrt, x_sqrt + U128{1} + y_sqrt);
    }
    const auto &k_upper = x_sqrt;
    for (U128 k = 2;; k++)
    {
        if (((k & 65535) == 0) && Globals::LoadStop() ) // Проверка на стоп через каждые 65536 отсчетов.
            break;
        if (k > k_upper)
            return std::make_pair(x, U128{1}); // x - простое число.
        if ((k & 1) == 1)
        { // Проверка с другой стороны: ускоряет поиск.
            // Основано на равенстве, следующем из метода Ферма: индекс k = (F^2 + x) / (2F) - floor(sqrt(x)).
            // Здесь F - кандидат в множители, x - раскладываемое число.
            const auto N1 = k * k + x;
            if ((N1.low() & 1) == 0)
            {
                const auto q1 = N1 / (k + k); // Здесь k как некоторый множитель F.
                const auto remainder = N1 % (k + k);
                if ((remainder == 0) && (q1 > x_sqrt))
                {
                    const auto q2 = x / k;
                    const auto remainder = x % k;
                    if (remainder == 0) // На всякий случай оставим.
                        return std::make_pair(k, q2);
                }
            }
        }
        if (const auto r = (y % 10u).low(); (r != 1 && r != 9)) // Просеиваем заведомо лишние.
            continue;
        bool is_exact;
        const auto y_sqrt = isqrt(y, is_exact);
        const auto delta = (x_sqrt + x_sqrt) + (k + k) + U128{1};
        y += delta;
        if (!is_exact)
            continue;
        const auto first_multiplier = x_sqrt + k - y_sqrt;
        return std::make_pair(first_multiplier, x_sqrt + k + y_sqrt);
    }
    return std::make_pair(x, U128{1}); // По какой-то причине не раскладывается.
}

U128 ro_pollard(const U128& n, std::optional<U128> limit)
{
    if (n < 4) return n;
    const bool has_limit = limit.has_value();
    const U128 limit_val = has_limit ? *limit : 0;
    U128 x = get_random_value_ab(1, n - 1);
    auto y {x};
    U128 d {1};
    U128 i{0};
    U128 c = get_random_value();
    c = c % (n - 1);
    c += 1;
    while (d == 1) {
        square_add_mod(x, c, n);
        square_add_mod(y, c, n);
        square_add_mod(y, c, n);
        d = x >= y ? gcd(x - y, n) : gcd(y - x, n);
        if (((i & 256) == 0) && Globals::LoadStop() ) // Проверка на стоп через каждые 256 отсчетов.
            break;
        if (has_limit && i >= limit_val)
            break;
        i++;
    }
    if (d != n)
        return d;
    else
        return n;
}

std::map<U128, int> factor(U128 x)
{
    Globals::SetStop(false);

    // test_projective_geometry();

    if (x == 0)
        return {{x, 1}};
    if (x == 1)
        return {{x, 1}};
    std::map<U128, int> result{};
    { // Обязательное деление на 2 перед методом Ленстры.
        const auto& [p, i] = div_by_q(x, 2);
        if (i > 0)
            result[p] += i;
        if (x == U128{1})
            return result;
    }
    { // Обязательное деление на 3 перед методом Ленстры.
        const auto& [p, i] = div_by_q(x, 3);
        if (i > 0)
            result[p] += i;
        if (x == U128{1})
            return result;
    }
    // Проверяем не является ли число степенью некоторого числа.
    int last_p = 0; // Искомая степень.
    U128 v {x};
    for (;;) {
        const U128 v_old = v;
        for (unsigned p = 2; p <= v.bit_width(); ++p)
        {
            const U128 vr = nroot(v, p);
            const auto x_r = int_power_fast(vr, p);
            if (v == x_r)
            {
                v = vr;
                last_p = last_p == 0 ? p : last_p * p;
                break;
            }
            if (vr < 2)
                break;
        }
        if (v == v_old)
            break;
    }
    if (last_p > 0)
    {
        x = v;
        if (is_prime(x, 64)) {
            result[x] += last_p;
            return result;
        }
    }

    // Делим на первые простые числа, начиная с 5 и до некоторого небольшого предела.
    u64 divisor = 5;
    for (; divisor < 65536u;)
    {
        const auto& [p, successes] = div_by_q(x, divisor);
        if (successes > 0)
            result[p] += last_p == 0 ? successes : successes * last_p;
        if (x == U128{1})
            return result;
        divisor += 2;
        for (;;) {
            const auto& [d1, d2] = ferma_method(divisor);
            if (d1 == divisor || d2 == divisor)
                break;
            divisor += 2;
        }
    }
    if (is_prime(x, 64)) {
        result[x] += last_p == 0 ? 1 : last_p;
        return result;
    }

    std::list<U128> found_factors;

    // Пробуем метод Ленстры.
    ecm::ECMFactorizer lenstra;
    for (;;)
    {
        if (Globals::LoadStop()) break;
        if (is_prime(x, 64)) {
            result[x] += last_p == 0 ? 1 : last_p;
            x = U128{1};
            break;
        }
        const auto& f = lenstra.factorize(x);
        if (f.has_value() && f.value() > 1 && f.value() < x) {
            found_factors.push_back(f.value());
            const auto q = x / f.value();
            const auto r = x % f.value();
            assert(r == 0);
            x = q;
        }
    }

    if (x > 1)
        found_factors.push_back(x);

    // Применяем метод Ферма рекурсивно.
    std::function<void(U128)> ferma_recursive;
    ferma_recursive = [&ferma_recursive, &result, &last_p](U128 x) -> void
    {
        if (is_prime(x, 64)) {
            result[x] += last_p == 0 ? 1 : last_p;
            return;
        }
        const auto& [a, b] = ferma_method(x);
        if (a == U128{1})
        {
            result[b] += last_p == 0 ? 1 : last_p;
            return;
        }
        else if (b == U128{1})
        {
            result[a] += last_p == 0 ? 1 : last_p;
            return;
        }
        ferma_recursive(a);
        ferma_recursive(b);
    };
    for (const auto& fac : found_factors)
        ferma_recursive(fac);

    Globals::SetStop(false);
    return result;
}

U128 get_random_half_value()
{
    static u128_rand::RandomGenerator g_prng;
    U128 result {g_prng.mGenerator.next_u64(), 0};
    g_prng.mGenerator.next_u64();
    return result;
}

U128 get_random_value_ab(const U128 &a, const U128 &b)
{
    assert(b >= a);
    const U128& m = b - a + 1;
    return m != 0 ? a + (get_random_value() % m) : get_random_value();
}

U128 get_random_value()
{
    static u128_rand::RandomGenerator g_prng;
    U128 result { g_prng.mGenerator.next_u64(), g_prng.mGenerator.next_u64()};
    g_prng.mGenerator.next_u64();
    g_prng.mGenerator.next_u64();
    return result;
}

}
