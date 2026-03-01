#pragma once

#include <optional>
#include <vector>
#include "u128.hpp"

namespace ecm {

using namespace bignum::u128;

/**
 * @brief Точка в проективных координатах (X:Y:Z).
 * Обычные координаты x = X/Z, y = Y/Z.
 */
struct ProjPoint {
    U128 X, Y, Z;
    bool is_inf() const { return Z == 0; }
};

/**
 * @brief Класс-контейнер для алгоритма Ленстры.
 * Реализует многоуровневую стратегию поиска делителей.
 */
class ECMFactorizer {
public:
    /**
     * @brief Основной метод факторизации.
     * @param n Число для факторизации.
     */
    static std::optional<U128> factorize(const U128& n);

private:
    /**
     * @brief Попытка факторизации на одной случайной кривой.
     */
    static std::optional<U128> try_one_curve(const U128& n, unsigned B1, unsigned B2,
                                             const std::vector<unsigned>& p1,
                                             const std::vector<unsigned>& p2);

    /**
     * @brief Реализация Baby-Step Giant-Step в Stage 2.
     */
    static std::optional<U128> run_stage2(ProjPoint Q, const U128& n, const U128& a,
                                          unsigned B1, const std::vector<unsigned>& p2);
};

}
