#include "calculus.h"


CALCULUS_EXPORT Decimal<> doIt(int op, Decimal<> x, Decimal<> y, int& error) {
    error = 0;
    Decimal<> tmp {};
    const bool x_is_neg = x.is_negative();
    const bool y_is_neg = y.is_negative();
    switch (op) {
    case c::ADD: tmp = x+y;
        if (tmp.is_overflow()) {
            error = c::NOT_FINITE;
        } else {
            return tmp;
        }
        break;
    case c::SUB: tmp = x-y;
        if (tmp.is_overflow()) {
            error = c::NOT_FINITE;
        } else {
            return tmp;
        }
        break;
    case c::MULT: tmp = x*y;
        if (tmp.is_overflow()) {
            error = c::NOT_FINITE;
        } else {
            return tmp;
        }
        if (!(x_is_neg ^ y_is_neg) && tmp.is_negative()) {
            error = c::NOT_FINITE;
        } else {
            return tmp;
        }
        break;
    case c::NEG: tmp = Decimal<>{} - x;
        return tmp;
    case c::DIV:
        if (!y.is_zero()) {
            tmp = x/y;
            if (tmp.is_overflow()) {
                error = c::NOT_FINITE;
            } else {
                return tmp;
            }
            if (!(x_is_neg ^ y_is_neg) && tmp.is_negative()) {
                error = c::NOT_FINITE;
            } else {
                return tmp;
            }
        } else {
            error = c::ZERO_DIVISION;
        }
        break;
    default: error = c::UNKNOW_OP;
    }
    return Decimal<> {};
}
