#include "u128_utils.h"
#include "rand_u128.h"
#include "i128.hpp"
#include "ecm_factorizer.h"

#include <functional>
#include <list>
#include <mutex>
#include <optional>
#include <thread>
#include <vector>

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
    success = false;
    if (m == U128{1} || m == U128{0}) {
        return 0;
    }

#if defined(__SIZEOF_INT128__)
    // --- 1. УЛЬТРАБЫСТРЫЙ ПУТЬ ДЛЯ GCC / LINUX (__int128) ---
    unsigned __int128 g_a = (static_cast<unsigned __int128>(a.high()) << 64) | a.low();
    unsigned __int128 g_m = (static_cast<unsigned __int128>(m.high()) << 64) | m.low();
    unsigned __int128 m0 = g_m;

    // Коэффициенты Безу (знаковые 128-битные типы компилятора)
    __int128 y = 0;
    __int128 x = 1;

    while (g_a > 1) {
        if (g_m == 0)
            return U128{}; // Защита от деления на ноль

        // GCC объединяет деление и взятие остатка в одну ассемблерную команду
        unsigned __int128 q = g_a / g_m;
        unsigned __int128 temp_m = g_m;
        g_m = g_a % g_m;
        g_a = temp_m;

        __int128 temp_y = y;
        y = x - static_cast<__int128>(q) * y;
        x = temp_y;
    }

    if (g_a != 1) {
        return 0; // Числа не взаимно просты, обратного элемента не существует
    }

    if (x < 0) {
        x += static_cast<__int128>(m0);
    }

    success = true;
    return U128(static_cast<uint64_t>(x), static_cast<uint64_t>(x >> 64));

#else
    // --- 2. НАДЕЖНЫЙ КРОСС ПЛАТФОРМЕННЫЙ FALLBACK ДЛЯ MSVC ---
    // Считаем на чистом U128, убирая тяжелый знаковый класс I128 из деления
    U128 m0 = m;
    U128 y = 0;
    U128 x = 1;
    bool x_sign = false; // false = плюс, true = минус
    bool y_sign = false;

    while (a > 1) {
        if (m == 0)
            return U128{};

        U128 q = a / m;
        U128 temp_m = m;
        m = a % m;
        a = temp_m;

        U128 temp_y = y;
        bool temp_y_sign = y_sign;

        // Вычисляем y = x - q * y с учетом знаков
        U128 qy = q * y;
        if (x_sign == y_sign) {
            if (x >= qy) {
                y = x - qy;
                y_sign = x_sign;
            } else {
                y = qy - x;
                y_sign = !x_sign;
            }
        } else {
            y = x + qy;
            y_sign = x_sign;
        }

        x = temp_y;
        x_sign = temp_y_sign;
    }

    if (a != 1)
        return 0;

    if (x_sign && x > 0) {
        x = m0 - x;
    }

    success = true;
    return x;
#endif
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

static std::map<U128, int> factor_ecm_worker(U128 x)
{
    std::map<U128, int> result{};
    std::list<U128> found_factors;
    ecm::ECMFactorizer lenstra;

    for (;;) {
        if (Globals::LoadStop())
            break;

        if (is_prime(x, 64)) {
            found_factors.push_back(x);
            x = U128{1};
            break;
        }

        const auto &f = lenstra.factorize(x);
        if (f.has_value() && f.value() > 1 && f.value() < x) {
            found_factors.push_back(f.value());
            const auto q = x / f.value();
            x = q;
        } else {
            break; // Если Ленстра не справился на текущих лимитах, передаем остаток методу Ферма
        }
    }

    if (x > 1) {
        found_factors.push_back(x);
    }

    // Дорасщепляем оставшиеся куски методом Ферма (если Ленстра выдал составной фактор)
    std::function<void(U128)> ferma_recursive;
    ferma_recursive = [&ferma_recursive, &result](U128 n) -> void {
        if (is_prime(n, 64)) {
            result[n] += 1;
            return;
        }
        const auto &[a, b] = ferma_method(n);
        if (a == U128{1}) {
            result[b] += 1;
            return;
        } else if (b == U128{1}) {
            result[a] += 1;
            return;
        }
        ferma_recursive(a);
        ferma_recursive(b);
    };

    for (const auto &fac : found_factors) {
        ferma_recursive(fac);
    }

    return result;
}

std::map<U128, int> factor_internal(U128 x)
{
    if (x == 0)
        return {{x, 1}};
    if (x == 1)
        return {{x, 1}};

    std::map<U128, int> result{};

    // Быстрый статический кэш простых чисел до 65536
    static const std::vector<unsigned> trial_primes = []() { return primes(65536u); }();

    // Выделяем чистую степень числа
    int global_power = 1;
    U128 v = x;
    for (;;) {
        const U128 v_old = v;
        for (unsigned p = 2; p <= v.bit_width(); ++p) {
            const U128 vr = nroot(v, p);
            if (v == int_power_fast(vr, p)) {
                v = vr;
                global_power *= p;
                break;
            }
            if (vr < 2)
                break;
        }
        if (v == v_old)
            break;
    }
    x = v;

    // Быстрое пробное деление
    for (unsigned p : trial_primes) {
        if (U128(p) * U128(p) > x)
            break;

        const auto &[div_p, successes] = div_by_q(x, p);
        if (successes > 0) {
            result[div_p] += successes;
        }
        if (x == U128{1})
            break;
    }

    if (x > 1 && is_prime(x, 64)) {
        result[x] += 1;
        x = U128{1};
    }

    // ВНИМАНИЕ: Если x всё еще > 1, значит это тяжелое составное число.
    // Вместо запуска Ленстры прямо здесь, мы возвращаем структуру, где
    // остаточный элемент x помечен специальным флагом (например, power = 0),
    // чтобы factor_parallel понял, что его нужно распараллелить.
    if (x > 1) {
        result[x] = 0;
    }

    // Применяем локальную степень к уже найденным на этом этапе делителям
    if (global_power > 1) {
        for (auto &[factor, power] : result) {
            if (power > 0)
                power *= global_power;
        }
    }

    return result;
}

// Функция предварительной обработки (выполняется в один поток)
static PreprocessResult factor_preprocess(U128 x)
{
    if (x == 0)
        return {{{x, 1}}, 0, 1};
    if (x == 1)
        return {{{x, 1}}, 0, 1};

    PreprocessResult res;
    static const std::vector<unsigned> trial_primes = []() { return primes(65536u); }();

    // Выделяем степень
    for (;;) {
        const U128 v_old = x;
        for (unsigned p = 2; p <= x.bit_width(); ++p) {
            const U128 vr = nroot(x, p);
            if (x == int_power_fast(vr, p)) {
                x = vr;
                res.global_power *= p;
                break;
            }
            if (vr < 2)
                break;
        }
        if (x == v_old)
            break;
    }

    // Пробное деление
    for (unsigned p : trial_primes) {
        if (U128(p) * U128(p) > x)
            break;
        const auto &[div_p, successes] = div_by_q(x, p);
        if (successes > 0) {
            res.factors[div_p] += successes;
        }
        if (x == U128{1})
            break;
    }

    if (x > 1 && is_prime(x, 64)) {
        res.factors[x] += 1;
        x = U128{1};
    }

    if (x > 1) {
        res.composite_remainder = x;
    }

    // Применяем степень к найденным на этапе препроцесса
    if (res.global_power > 1) {
        for (auto &[factor, power] : res.factors) {
            power *= res.global_power;
        }
    }

    return res;
}

std::map<bignum::u128::U128, int> factor_parallel(bignum::u128::U128 x)
{
    // Запускаем детерминированную предобработку в вызывающем потоке
    PreprocessResult prep = factor_preprocess(x);

    // Если тяжелого остатка нет, число полностью разложилось в препроцессоре.
    // Выходим сразу, экономя время на создании потоков ОС.
    if (prep.composite_remainder == 0) {
        return prep.factors;
    }

    // Если есть тяжелый остаток, отдаем его на растерзание параллельному ECM
    std::mutex mtx;
    std::map<bignum::u128::U128, int> ecm_result;
    bool has_result = false;

    Globals::SetStop(false);

    unsigned int num_threads = std::thread::hardware_concurrency();
    if (num_threads == 0) {
        num_threads = 4;
    }

    std::vector<std::jthread> workers;
    workers.reserve(num_threads);

    for (unsigned int i = 0; i < num_threads; ++i) {
        workers.emplace_back(
            [&](bignum::u128::U128 val) {
                // Потоки бьют сразу в Ленстру, не дублируя препроцессинг
                auto result = factor_ecm_worker(val);

                std::lock_guard<std::mutex> lock(mtx);
                if (!has_result && !result.empty()) {
                    ecm_result = std::move(result);
                    has_result = true;
                    Globals::SetStop(true);
                }
            },
            prep.composite_remainder);
    }

    workers.clear(); // Дожидаемся завершения всех потоков (join)

    // Итоговое слияние: ко всем факторам, которые вернул ECM,
    // честно применяем исходную глобальную степень числа.
    for (const auto &[factor, power] : ecm_result) {
        prep.factors[factor] += (power * prep.global_power);
    }

    return prep.factors;
}

U128 get_random_half_value()
{
    thread_local static u128_rand::RandomGenerator g_prng;
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
    thread_local static u128_rand::RandomGenerator g_prng;
    U128 result { g_prng.mGenerator.next_u64(), g_prng.mGenerator.next_u64()};
    g_prng.mGenerator.next_u64();
    g_prng.mGenerator.next_u64();
    return result;
}
}
