#include "u128_utils.h"

#include <functional> // std::function

namespace u128::utils
{

std::pair<U128, int> div_by_q(U128 &x, ULOW q)
{
    auto [tmp, remainder] = x / q;
    int i = 0;
    while (remainder == U128{0})
    {
        i++;
        x = tmp;
        std::tie(tmp, remainder) = x / q;
    }
    return std::make_pair(U128{q}, i);
}

bool miller_test(U128 d, U128 n)
{
    U128 a = get_random_value();
    {
        U128 tmp;
        std::tie(tmp, a) = a / (n - 3);
    }
    a += 2;
    U128 x = int_power_mod(a, d, n);
    if ((x == 1) || (x == (n - 1)))
        return true;

    while (d != (n - 1))
    {
        x = int_power_mod(x, 2, n);
        d *= U128{2};
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
    U128 d = x - 1;
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
        y = y + delta;
        if (is_exact)
            return std::make_pair(x_sqrt + U128{1} - y_sqrt, x_sqrt + U128{1} + y_sqrt);
    }
    const auto &k_upper = x_sqrt;
    for (auto k = U128{2};; k.inc())
    {
        if (((k.low() & 65535) == 0) && Globals::LoadStop() ) // Проверка на стоп через каждые 65536 отсчетов.
            break;
        if (k > k_upper)
        {
            return std::make_pair(x, U128{1}); // x - простое число.
        }
        if ((k.low() & 1) == 1)
        { // Проверка с другой стороны: ускоряет поиск.
            // Основано на равенстве, следующем из метода Ферма: индекс k = (F^2 + x) / (2F) - floor(sqrt(x)).
            // Здесь F - кандидат в множители, x - раскладываемое число.
            const auto &N1 = k * k + x;
            if ((N1.low() & 1) == 0)
            {
                auto [q1, remainder] = N1 / (k + k); // Здесь k как некоторый множитель F.
                if ((remainder == 0) && (q1 > x_sqrt))
                {
                    auto [q2, remainder] = x / k;
                    if (remainder == 0) // На всякий случай оставим.
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
}

std::map<U128, int> factor(U128 x)
{
    Globals::SetStop(false);
    if (x == 0)
    {
        return {{x, 1}};
    }
    if (x == 1)
    {
        return {{x, 1}};
    }
    std::map<U128, int> result{};
    { // Обязательное для метода Ферма деление на 2.
        const auto& [p, i] = div_by_q(x, 2);
        if (i > 0)
            result[p] = i;
        if (x == U128{1})
            return result;
    }
    // Делим на первые простые числа, начиная с 3 и до некоторого небольшого предела.
    ULOW divisor = 3;
    for (; divisor < 65536u;)
    {
        const auto& [p, successes] = div_by_q(x, divisor);
        if (successes > 0)
            result[p] = successes;
        if (x == U128{1})
            return result;
        divisor += 2;
        while (!is_prime(divisor, 64)) {
            divisor += 2;
        }
    }
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
    ferma_recursive(x);
    Globals::SetStop(false);
    return result;
}

}
