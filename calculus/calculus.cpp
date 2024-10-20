#include "calculus.h"

CALCULUS_EXPORT dec_n::Decimal<> doIt(int operation, dec_n::Decimal<> x, dec_n::Decimal<> y, int& error_code)
{
    error_code = c::NO_ERRORS;
    dec_n::Decimal<> result {};
    const bool x_is_neg = x.IsNegative();
    const bool y_is_neg = y.IsNegative();
    const bool both_have_the_same_sign = x_is_neg == y_is_neg;
    switch (operation) {
    case c::ADD: result = x + y;
        if (result.IsOverflowed()) {
            error_code = c::NOT_FINITE;
        } else {
            return result;
        }
        break;
    case c::SUB: result = x - y;
        if (result.IsOverflowed()) {
            error_code = c::NOT_FINITE;
        } else {
            return result;
        }
        break;
    case c::MULT: result = x * y;
        if (result.IsOverflowed()) {
            error_code = c::NOT_FINITE;
        } else {
            return result;
        }
        if (both_have_the_same_sign && result.IsNegative()) {
            error_code = c::NOT_FINITE;
        } else {
            return result;
        }
        break;
    case c::NEG: result = dec_n::Decimal<>{} - x;
        return result;
    case c::DIV:
        if (!y.IsZero()) {
            result = x / y;
            if (result.IsOverflowed()) {
                error_code = c::NOT_FINITE;
            } else {
                return result;
            }
            if (both_have_the_same_sign && result.IsNegative()) {
                error_code = c::NOT_FINITE;
            } else {
                return result;
            }
        } else {
            error_code = c::ZERO_DIVISION;
        }
        break;
    default: error_code = c::UNKNOW_OP;
    }
    return dec_n::Decimal<> {};
}
