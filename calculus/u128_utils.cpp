#include "u128_utils.h"
#include <list>
#include <functional>
#include "i128.hpp"

namespace u128::utils
{

std::pair<U128, int> div_by_q(U128 &x, const U128& q)
{
    auto [quotient, remainder] = x / q;
    int i = 0;
    while (remainder == 0)
    {
        i++;
        x = quotient;
        std::tie(quotient, remainder) = x / q;
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
    for (U128 k = 2;; k.inc())
    {
        if (((k & 65535) == 0) && Globals::LoadStop() ) // Проверка на стоп через каждые 65536 отсчетов.
            break;
        if (k > k_upper)
            return std::make_pair(x, U128{1}); // x - простое число.
        if ((k & 1) == 1)
        { // Проверка с другой стороны: ускоряет поиск.
            // Основано на равенстве, следующем из метода Ферма: индекс k = (F^2 + x) / (2F) - floor(sqrt(x)).
            // Здесь F - кандидат в множители, x - раскладываемое число.
            const auto &N1 = k * k + x;
            if ((N1.low() & 1) == 0)
            {
                const auto& [q1, remainder] = N1 / (k + k); // Здесь k как некоторый множитель F.
                if ((remainder == 0) && (q1 > x_sqrt))
                {
                    const auto& [q2, remainder] = x / k;
                    if (remainder == 0) // На всякий случай оставим.
                        return std::make_pair(k, q2);
                }
            }
        }
        if (const auto &r = y.mod10(); (r != 1 && r != 9)) // Просеиваем заведомо лишние.
            continue;
        bool is_exact;
        const auto& y_sqrt = isqrt(y, is_exact);
        const auto& delta = (x_sqrt + x_sqrt) + (k + k) + U128{1};
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
        i.inc();
    }
    if (d != n)
        return d;
    else
        return n;
}

std::map<U128, int> factor(U128 x)
{
    Globals::SetStop(false);

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
    // Проверяем не является ли число степенью единственного простого числа.
    int last_p = 0;
    U128 v {x};
    for (;;) {
        const U128 v_old = v;
        for (int p = 2; p <= v.bit_length(); ++p)
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
        assert(is_prime(v, 64));
        result[v] += last_p;
        return result;
    }

    // Делим на первые простые числа, начиная с 5 и до некоторого небольшого предела.
    ULOW divisor = 5;
    for (; divisor < 65536u;)
    {
        const auto& [p, successes] = div_by_q(x, divisor);
        if (successes > 0)
            result[p] += successes;
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
        result[x]++;
        return result;
    }

    std::list<U128> found_factors;

    // // Пробуем метод Полларда.
    // U128 iters;
    // iters = isqrt(x);
    // iters = isqrt(iters);
    // for (;;)
    // {
    //     if (is_prime(x, 64)) {
    //         result[x]++;
    //         x = U128{1};
    //         break;
    //     }
    //     const auto& d = ro_pollard(x, iters);
    //     if (d != x && d != 1) {
    //         found_factors.push_back(d);
    //         const auto& [q, r] = x / d;
    //         assert(r == 0);
    //         x = q;
    //         iters = isqrt(x);
    //         iters = isqrt(iters);
    //         continue;
    //     }
    //     break;
    // }

    // Пробуем метод Ленстры.
    for (;;)
    {
        if (Globals::LoadStop()) break;
        if (is_prime(x, 64)) {
            result[x]++;
            x = U128{1};
            break;
        }
        const auto limit = 100'000ull;
        const auto& f = lenstra(x, limit);
        if (f.has_value() && f.value() > 1 && f.value() < x) {
            found_factors.push_back(f.value());
            const auto& [q, r] = x / f.value();
            assert(r == 0);
            x = q;
        }
    }

    if (x > 1)
        found_factors.push_back(x);

    // Применяем метод Ферма рекурсивно.
    std::function<void(U128)> ferma_recursive;
    ferma_recursive = [&ferma_recursive, &result](U128 x) -> void
    {
        if (is_prime(x, 64)) {
            result[x]++;
            return;
        }
        const auto& [a, b] = ferma_method(x);
        if (a == U128{1})
        {
            result[b]++;
            return;
        }
        else if (b == U128{1})
        {
            result[a]++;
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
        const I128 q = (a / m).first;
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

TripleU128 elliptic_add(const TripleU128& p, const TripleU128& q, const U128& a, const U128& /*b*/, const U128& m) {
    if (p.mCell[2] == 0) {
        return q;
    }
    if (q.mCell[2] == 0) {
        return p;
    }
    U128 num;
    U128 denom;
    if (p.mCell[0] == q.mCell[0]) {
        {
            U128 sum1 = p.mCell[1];
            add_mod(sum1, q.mCell[1], m);
            if (sum1 == 0)
                return TripleU128{0, 1, 0}; // Infinity.
        }
        {
            num = p.mCell[0];
            square_mod(num, m);
            mult_add_mod(num, 3, a, m);
            denom = p.mCell[1];
            mult_mod(denom, 2, m);
        }
    }
    else
    {
        num = q.mCell[1];
        sub_mod(num, p.mCell[1], m);
        denom = q.mCell[0];
        sub_mod(denom, p.mCell[0], m);
    }
    bool is_ok;
    U128 inv = modular_inverse(denom, m, is_ok);
    if (!is_ok)
        return TripleU128{0, 0, denom}; // Failure.
    U128 inv_num = inv;
    mult_mod(inv_num, num, m);
    U128 z = inv_num;
    square_mod(z, m);
    sub_mod(z, p.mCell[0], m);
    sub_mod(z, q.mCell[0], m);
    U128 w = p.mCell[0];
    sub_mod(w, z, m);
    mult_mod(w, inv_num, m);
    sub_mod(w, p.mCell[1], m);
    return TripleU128{z, w, 1};
}

TripleU128 elliptic_mul(U128 k, TripleU128 p, const U128 &a, const U128 &b, const U128 &m)
{
    TripleU128 r{0, 1, 0}; // Infinity.
    while (k != 0) {
        if (p.mCell[2] > 1)
            return p;
        if ((k % 2) == 1)
            r = elliptic_add(p, r, a, b, m);
        k /= 2;
        p = elliptic_add(p, p, a, b, m);
    }
    return r;
}

std::optional<U128> lenstra(const U128 &n, unsigned limit)
{
    using namespace bignum::ubig;
    using U256 = UBig<U128, 256>;
    using U512 = UBig<U256, 512>;
    const U512 n_ext = U256{n};
    U512 g = n_ext;
    TripleU128 q;
    U128 a;
    U128 b;
    while (g == n_ext)
    {
        q.mCell[0] = get_random_value_ab(0, n-1);
        q.mCell[1] = get_random_value_ab(0, n-1);
        q.mCell[2] = 1;
        a = get_random_value_ab(0, n-1);
        b = q.mCell[1];
        square_mod(b, n);
        U128 q0qubic = q.mCell[0];
        square_mod(q0qubic, n);
        mult_mod(q0qubic, q.mCell[0], n);
        U128 aq0 = a;
        mult_mod(aq0, q.mCell[0], n);
        sub_mod(b, q0qubic, n);
        sub_mod(b, aq0, n);
        const U256& a4 = U256::mult_ext(4, a);
        const U512& a4_3 = U512::mult_ext(a4, U256::square_ext(a));
        const U256& c27 = 27;
        const U512& b27_2 = U512::mult_ext(c27, U256::square_ext(b));
        const U512& poly = a4_3 + b27_2;
        g = gcd(poly, n_ext);
        if (Globals::LoadStop() ) // Проверка на стоп.
            return {};
    }
    if (g > 1)
        return g.low().low();
    const U128 limit_ext {limit};
    for (const auto& p : primes(limit)) {
        const U128 p_ext {p};
        auto pp = p_ext;
        while (pp < limit_ext) {
            q = elliptic_mul(p_ext, q, a, b, n);
            if (q.mCell[2] > 1)
                return gcd(q.mCell[2], n);
            pp *= p_ext;
        }
        if (Globals::LoadStop() ) // Проверка на стоп.
            return {};
    }
    return std::nullopt;
}

}
