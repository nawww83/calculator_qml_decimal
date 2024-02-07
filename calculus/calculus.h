#ifndef CALCULUS_H
#define CALCULUS_H

#include "calculus_global.h"
#include "decimal.h"

namespace c {

enum Ops {
    ADD=0,
    SUB,
    MULT,
    DIV,
    NEG
};

enum Errors {
    NO_ERRORS,
    UNKNOW_OP,
    ZERO_DIVISION,
    NOT_FINITE
    //
};

} //

using namespace dec_n;

CALCULUS_EXPORT Decimal<> doIt(int op, Decimal<> x, Decimal<> y, int& error);
CALCULUS_EXPORT void setDelay(int t);

#endif // CALCULUS_H
