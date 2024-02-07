#pragma once

#include "decimal.h"
#include "qvector.h"

namespace _tp {

constexpr inline int buff_size = 256;

struct Request {
    int op;
    QVector<dec_n::Decimal<>> xy;
};


struct Result {
    int error;
    dec_n::Decimal<> z;
};

}
