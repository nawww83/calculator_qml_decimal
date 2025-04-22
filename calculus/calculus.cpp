#include "calculus.h"

CALCULUS_EXPORT dec_n::Decimal doIt(int operation, dec_n::Decimal x, dec_n::Decimal y, int& error_code)
{
    error_code = calculus::NO_ERRORS;
    dec_n::Decimal result {};
    const bool x_is_neg = x.IsNegative();
    const bool y_is_neg = y.IsNegative();
    const bool both_have_the_same_sign = x_is_neg == y_is_neg;
    switch (operation) {
    case calculus::ADD: result = x + y;
        if (result.IsOverflowed()) {
            error_code = calculus::NOT_FINITE;
        } else {
            return result;
        }
        break;
    case calculus::SUB: result = x - y;
        if (result.IsOverflowed()) {
            error_code = calculus::NOT_FINITE;
        } else {
            return result;
        }
        break;
    case calculus::MULT: result = x * y;
        if (result.IsOverflowed()) {
            error_code = calculus::NOT_FINITE;
        } else {
            return result;
        }
        if (both_have_the_same_sign && result.IsNegative()) {
            error_code = calculus::NOT_FINITE;
        } else {
            return result;
        }
        break;
    case calculus::NEG: result = dec_n::Decimal{} - x;
        return result;
    case calculus::SQRT: result = dec_n::Sqrt(x);
        return result;
    case calculus::SQR: result = x * x;
        return result;
    case calculus::RECIPROC:
    {
        if (x.IsZero()) {
            error_code = calculus::ZERO_DIVISION;
        } else {
            dec_n::Decimal one;
            one.SetDecimal(u128::get_unit(), u128::get_zero());
            result = one / x;
            if (result.IsOverflowed()) {
                error_code = calculus::NOT_FINITE;
            } else {
                return result;
            }
        }
        break;
    }
    case calculus::DIV:
        if (!y.IsZero()) {
            result = x / y;
            if (result.IsOverflowed()) {
                error_code = calculus::NOT_FINITE;
            } else {
                return result;
            }
            if (both_have_the_same_sign && result.IsNegative()) {
                error_code = calculus::NOT_FINITE;
            } else {
                return result;
            }
        } else {
            error_code = calculus::ZERO_DIVISION;
        }
        break;
    case calculus::SEPARATOR:
        error_code = calculus::UNKNOW_OP;
        break;
    default: error_code = calculus::UNKNOW_OP;
    }
    return dec_n::Decimal{};
}

void changeDecimalWidth(int width, int max_width)
{
    dec_n::Decimal::SetWidth(width, max_width);
}
