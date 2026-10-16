// Регрессионные тесты числового ядра (I128 / Decimal).
//
// Покрывают исправленные дефекты:
//   1. I128: знак частного хранится только в поле sign, модуль = |X|/|Y|
//      (усечение к нулю, без евклидовой коррекции Q -= 1). Из-за прежней
//      коррекции I128{-3} / 2 давало -2 вместо -1 и ломало арифметику Decimal.
//   2. I128: сравнение с нулём при разных знаках (0 == -5 и т.п.).
//   3. Decimal::operator< для отрицательных чисел.
//
// Сборка и запуск (Qt не требуется, всё заголовочное):
//   g++ -std=c++20 -O2 -I.. numeric_regression_test.cpp -o numeric_regression_test
//   ./numeric_regression_test

#include "decimal.h"

#include <cstdint>
#include <iostream>
#include <string>

using bignum::u128::U128;
using bignum::i128::I128;
using bignum::i128::Sign;
using dec_n::Decimal;

static int g_failures = 0;

#define CHECK(cond)                                                     \
    do {                                                                \
        if (!(cond)) {                                                  \
            ++g_failures;                                               \
            std::cerr << "FAIL " << __FILE__ << ":" << __LINE__ << ": " \
                      << #cond << "\n";                                 \
        }                                                               \
    } while (0)

static Decimal D(const std::string& s)
{
    Decimal d;
    d.SetStringRepresentation(s);
    return d;
}

static std::string S(const Decimal& d)
{
    return std::string(d.ValueAsStringView());
}

// 1. Деление I128: знак — только в sign, модуль = |X|/|Y|.
static void test_i128_division_sign()
{
    {
        const auto& [q, r] = I128{U128{3}, Sign{true}} / I128{U128{2}}; // -3 / 2
        CHECK(q == -I128{1ull});
        CHECK(r == -I128{1ull});
    }
    {
        const auto& [q, r] = I128{U128{7}, Sign{true}} / I128{U128{2}}; // -7 / 2
        CHECK(q == -I128{3ull});
        CHECK(r == -I128{1ull});
    }
    {
        const auto& [q, r] = I128{U128{7}} / I128{U128{2}, Sign{true}}; // 7 / -2
        CHECK(q == -I128{3ull});
        CHECK(r == I128{1ull});
    }
    {
        const auto& [q, r] = I128{U128{7}, Sign{true}} / I128{U128{2}, Sign{true}}; // -7 / -2
        CHECK(q == I128{3ull});
        CHECK(r == -I128{1ull});
    }
    {
        // Модуль частного не зависит от знаков операндов.
        const I128 d{U128{7}};
        const I128 q_pos = (I128{U128{100}} / d).first;
        const I128 q_neg = (I128{U128{100}, Sign{true}} / d).first;
        CHECK(q_pos == I128{14ull});
        CHECK(q_neg == -I128{14ull});
        CHECK(q_pos.unsigned_part() == q_neg.unsigned_part());
    }
    {
        // Полуширинное деление: знак также только в sign, остаток — по модулю.
        const auto& [q, r] = I128{U128{555}, Sign{true}} / uint64_t{7}; // -555 / 7
        CHECK(q == -I128{79ull});
        CHECK(r == 2ull);
    }
}

// 2. Сравнение I128 с нулём.
static void test_i128_zero_comparison()
{
    const I128 zero{U128{0}};
    const I128 neg{U128{5}, Sign{true}};      // -5
    const I128 neg_zero{U128{0}, Sign{true}}; // -0
    const I128 pos{U128{5}};

    CHECK(!(zero == neg));
    CHECK(zero != neg);
    CHECK(zero > neg);
    CHECK(zero >= neg);
    CHECK(!(zero < neg));
    CHECK(!(zero <= neg));

    CHECK(neg_zero < pos);
    CHECK(neg_zero <= pos);
    CHECK(!(neg_zero > pos));
    CHECK(!(neg_zero == pos));
    CHECK(neg_zero == zero);
}

// 3. Порядок Decimal (в том числе для отрицательных).
static void test_decimal_ordering()
{
    CHECK(!(D("-5,3") < D("-5,7")));
    CHECK(D("-5,7") < D("-5,3"));
    CHECK(D("-5,7") <= D("-5,3"));
    CHECK(D("-5,3") > D("-5,7"));
    CHECK(D("-5,3") >= D("-5,7"));

    CHECK(D("5,3") < D("5,7"));
    CHECK(!(D("5,7") < D("5,3")));

    CHECK(D("-0,5") < D("0,3"));
    CHECK(!(D("0,3") < D("-0,5")));
    CHECK(!(D("-5,0") < D("-5,0")));
    CHECK(D("-2,0") < D("-1,0"));
    CHECK(D("-1,0") < D("0,0"));
    CHECK(D("-1,5") < D("-0,5"));
}

// Арифметика Decimal: регрессия на отрицательные результаты.
static void test_decimal_arithmetic()
{
    CHECK(S(D("-0,9") - D("0,6")) == "-1,500000000000000000");
    CHECK(S(D("-0,5") - D("0,6")) == "-1,100000000000000000");
    CHECK(S(D("0,5") - D("1")) == "-0,500000000000000000");
    CHECK(S(D("-0,5") + D("-0,5")) == "-1,000000000000000000");
    CHECK(S(D("-0,5") + D("-1")) == "-1,500000000000000000");
    CHECK(S(D("-0,5") * D("3")) == "-1,500000000000000000");
    CHECK(S(D("0,9") + D("0,6")) == "1,500000000000000000");
}

int main()
{
    test_i128_division_sign();
    test_i128_zero_comparison();
    test_decimal_ordering();
    test_decimal_arithmetic();

    if (g_failures == 0) {
        std::cout << "All numeric regression tests passed.\n";
        return 0;
    }
    std::cerr << g_failures << " check(s) failed.\n";
    return 1;
}
