#include "ecm_factorizer.h"
#include "u128_utils.h"

namespace ecm {

using namespace u128::utils;

// --- Быстрые операции в проективных координатах ---

static ProjPoint projective_double(const ProjPoint& P, const U128& a, const U128& m) {
    if (P.Z == 0 || P.Y == 0) return {0, 1, 0};

    // Временные переменные (не используем P.X/Y/Z после инициализации)
    U128 X = P.X, Y = P.Y, Z = P.Z;
    U128 w, s, B, h, resX, resY, resZ;

    // 1. w = 3*X^2 + a*Z^2
    U128 X2 = X; square_mod(X2, m);
    U128 X2_3 = X2; add_mod(X2_3, X2, m); add_mod(X2_3, X2, m); // 3X^2
    U128 Z2 = Z; square_mod(Z2, m);
    U128 aZ2 = Z2; mult_mod(aZ2, a, m);
    w = X2_3; add_mod(w, aZ2, m); // w

    // 2. s = Y * Z
    s = Y; mult_mod(s, Z, m); // s

    // 3. B = X * Y * s
    B = X; mult_mod(B, Y, m); mult_mod(B, s, m); // B

    // 4. h = w^2 - 8*B
    h = w; square_mod(h, m);
    U128 B8 = B; mult_mod(B8, U128(8), m);
    sub_mod(h, B8, m); // h

    // 5. resX = 2 * h * s
    resX = h; mult_mod(resX, s, m);
    add_mod(resX, resX, m);

    // 6. resZ = 8 * s^3
    U128 s2 = s; square_mod(s2, m);
    resZ = s2; mult_mod(resZ, s, m);
    mult_mod(resZ, U128(8), m);

    // 7. resY = w * (4*B - h) - 8 * Y^2 * s^2
    U128 B4 = B; mult_mod(B4, U128(4), m);
    U128 B4_h = B4; sub_mod(B4_h, h, m);
    resY = w; mult_mod(resY, B4_h, m);

    U128 Y2 = Y; square_mod(Y2, m);
    U128 term2 = Y2; mult_mod(term2, s2, m);
    mult_mod(term2, U128(8), m);
    sub_mod(resY, term2, m);

    return {resX, resY, resZ};
}

static ProjPoint projective_add(const ProjPoint& P, const ProjPoint& Q, const U128& a, const U128& m) {
    if (P.Z == 0) return Q;
    if (Q.Z == 0) return P;

    U128 U1, U2, S1, S2;
    // 1. Приведение к общему знаменателю (Z1 * Z2)
    U1 = P.X; mult_mod(U1, Q.Z, m); // U1 = X1 * Z2
    U2 = Q.X; mult_mod(U2, P.Z, m); // U2 = X2 * Z1
    S1 = P.Y; mult_mod(S1, Q.Z, m); // S1 = Y1 * Z2
    S2 = Q.Y; mult_mod(S2, P.Z, m); // S2 = Y2 * Z1

    if (U1 == U2) {
        if (S1 != S2) return {0, 1, 0}; // P = -Q
        return projective_double(P, a, m); // P = Q
    }

    U128 H, R, H2, H3, Z1Z2, V, resX, resY, resZ;
    H = U2; sub_mod(H, U1, m);          // H = U2 - U1
    R = S2; sub_mod(R, S1, m);          // R = S2 - S1

    H2 = H; square_mod(H2, m);          // H^2
    H3 = H2; mult_mod(H3, H, m);        // H^3
    Z1Z2 = P.Z; mult_mod(Z1Z2, Q.Z, m); // Z1 * Z2
    V = U1; mult_mod(V, H2, m);         // V = U1 * H^2

    // 2. A = R^2 * Z1 * Z2 - H^3 - 2*V
    U128 A = R; square_mod(A, m);
    mult_mod(A, Z1Z2, m);               // R^2 * Z1 * Z2
    sub_mod(A, H3, m);                  // - H^3
    U128 V2 = V; add_mod(V2, V, m);
    sub_mod(A, V2, m);                  // A

    // 3. resX = H * A
    resX = A; mult_mod(resX, H, m);

    // 4. resZ = Z1 * Z2 * H^3
    resZ = Z1Z2; mult_mod(resZ, H3, m);

    // 5. resY = R * (V - A) - S1 * H^3
    resY = V; sub_mod(resY, A, m);      // V - A
    mult_mod(resY, R, m);               // R * (V - A)
    U128 S1H3 = S1; mult_mod(S1H3, H3, m);
    sub_mod(resY, S1H3, m);             // - S1 * H^3

    return {resX, resY, resZ};
}

// Быстрое возведение в степень (scalar multiplication)
static ProjPoint projective_mul(U128 k, ProjPoint p, const U128& a, const U128& m) {
    ProjPoint r{0, 1, 0}; // Infinity
    while (k > 0) {
        if (k % 2 == 1) {
            r = projective_add(r, p, a, m);
        }
        p = projective_double(p, a, m);
        k /= 2;
    }
    return r;
}

std::optional<U128> ecm::ECMFactorizer::factorize(const U128 &n)
{
    // План стратегии: {B1, количество_попыток_на_этом_B1}
    struct Level { unsigned b1; int curves; };
    // Расширенная стратегия для U128
    std::vector<Level> strategy = {
        { 500,    25   },
        { 2000,   100  },
        { 10000,  250  },
        { 50000,  600  },
        { 110000, 1500 }  // Оптимальный предел для 128-битных чисел
    };

    for (const auto& level : strategy) {
        unsigned B1 = level.b1;
        unsigned B2 = B1 * 50;
        auto p1 = primes(B1);
        auto p2 = primes(B2);

        if (u128::Globals::LoadStop()) break;

        for (int i = 0; i < level.curves; ++i) {
            if (u128::Globals::LoadStop()) break;
            auto res = try_one_curve(n, B1, B2, p1, p2);
            if (res) return res;
        }
    }

    // Если ничего не помогло, можно продолжать на макс. B1 бесконечно
    return std::nullopt;
}

std::optional<U128> ecm::ECMFactorizer::try_one_curve(const U128 &n, unsigned int B1, unsigned int /*B2*/, const std::vector<unsigned int> &p1, const std::vector<unsigned int> &p2)
{
    // Генерация параметров кривой Вейерштрасса и начальной точки
    U128 x0 = get_random_value_ab(1, n - 1);
    U128 y0 = get_random_value_ab(1, n - 1);
    U128 a = get_random_value_ab(1, n - 1);

    U128 b = y0; square_mod(b, n);
    U128 x0_3 = x0; square_mod(x0_3, n); mult_mod(x0_3, x0, n);
    U128 ax0 = a; mult_mod(ax0, x0, n);
    sub_mod(b, x0_3, n); sub_mod(b, ax0, n);

    ProjPoint Q{x0, y0, 1};

    // --- STAGE 1 ---
    for (const auto& p : p1) {
        U128 pp = U128(p);
        while (pp <= U128(B1) / U128(p)) pp *= U128(p);

        Q = projective_mul(pp, Q, a, n);

        // Проверка GCD в Stage 1 (можно раз в 32 шага для скорости)
        if (Q.Z != 0) {
            U128 d = gcd(Q.Z, n);
            if (d > 1) return (d < n) ? std::optional<U128>(d) : std::nullopt;
        }
    }

    // --- STAGE 2 ---
    return run_stage2(Q, n, a, B1, p2);
}

std::optional<U128> ecm::ECMFactorizer::run_stage2(ProjPoint Q, const U128 &n, const U128 &a, unsigned int B1, const std::vector<unsigned int> &p2)
{
    if (Q.is_inf()) return std::nullopt;

    // Таблица шагов для разрывов между простыми
    std::vector<ProjPoint> steps;
    ProjPoint Q2 = projective_double(Q, a, n);
    steps.push_back(Q2); // 2Q
    for (int i = 1; i < 128; ++i) {
        steps.push_back(projective_add(steps.back(), Q2, a, n));
    }

    // Индекс первого простого > B1
    size_t p_idx = 0;
    while (p_idx < p2.size() && p2[p_idx] <= B1) p_idx++;
    if (p_idx >= p2.size()) return std::nullopt;

    ProjPoint T = projective_mul(U128(p2[p_idx]), Q, a, n);
    U128 accum_Z = 1;
    int batch = 0;

    for (; p_idx + 1 < p2.size(); ++p_idx) {
        unsigned diff = p2[p_idx + 1] - p2[p_idx];
        unsigned step_idx = (diff / 2) - 1;

        if (step_idx < steps.size()) {
            T = projective_add(T, steps[step_idx], a, n);
        } else {
            T = projective_add(T, projective_mul(U128(diff), Q, a, n), a, n);
        }

        // Накопление Z для редкого GCD (Batch GCD)
        using U256 = bignum::UBig<U128>;
        U256 prod = U256::mult_ext(accum_Z, T.Z);
        accum_Z = (prod / U256{n}).second.low();

        if (++batch % 64 == 0) {
            U128 d = gcd(accum_Z, n);
            if (d > 1) return (d < n) ? std::optional<U128>(d) : std::nullopt;
            accum_Z = 1;
        }
    }

    U128 d_final = gcd(accum_Z, n);
    return (d_final > 1 && d_final < n) ? std::optional<U128>(d_final) : std::nullopt;
}
}
