#include "calculus.h"


CALCULUS_EXPORT dec_n::Decimal<> doIt(int operation, dec_n::Decimal<> x, dec_n::Decimal<> y, int& error_code)
{
    error_code = c::NO_ERRORS;
    dec_n::Decimal<> tmp {};
    const bool x_is_neg = x.IsNegative();
    const bool y_is_neg = y.IsNegative();
    const bool both_have_the_same_sign = !(x_is_neg ^ y_is_neg);
    switch (operation) {
    case c::ADD: tmp = x + y;
        if (tmp.IsOverflowed()) {
            error_code = c::NOT_FINITE;
        } else {
            return tmp;
        }
        break;
    case c::SUB: tmp = x - y;
        if (tmp.IsOverflowed()) {
            error_code = c::NOT_FINITE;
        } else {
            return tmp;
        }
        break;
    case c::MULT: tmp = x * y;
        if (tmp.IsOverflowed()) {
            error_code = c::NOT_FINITE;
        } else {
            return tmp;
        }
        if (both_have_the_same_sign && tmp.IsNegative()) {
            error_code = c::NOT_FINITE;
        } else {
            return tmp;
        }
        break;
    case c::NEG: tmp = dec_n::Decimal<>{} - x;
        return tmp;
    case c::DIV:
        if (!y.IsZero()) {
            tmp = x / y;
            if (tmp.IsOverflowed()) {
                error_code = c::NOT_FINITE;
            } else {
                return tmp;
            }
            if (both_have_the_same_sign && tmp.IsNegative()) {
                error_code = c::NOT_FINITE;
            } else {
                return tmp;
            }
        } else {
            error_code = c::ZERO_DIVISION;
        }
        break;
    default: error_code = c::UNKNOW_OP;
    }
    return dec_n::Decimal<> {};
}
