#ifndef CALCULUS_H
#define CALCULUS_H

#include "calculus_global.h"
#include "decimal.h"

namespace c {

enum Ops {
    ADD = 0,
    SUB,
    MULT,
    DIV,
    NEG
};

enum Errors {
    NO_ERRORS = 0,
    UNKNOW_OP,
    ZERO_DIVISION,
    NOT_FINITE
};

}

CALCULUS_EXPORT dec_n::Decimal<> doIt(int operation, dec_n::Decimal<> x, dec_n::Decimal<> y, int& error);

#endif // CALCULUS_H
