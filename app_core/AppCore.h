#pragma once

#include <QObject>
#include <QVariant>

#include "decimal.h"

Q_DECLARE_METATYPE(dec_n::Decimal<>);

namespace OperationEnums {
Q_NAMESPACE
enum Operations {
    CLEAR_ALL=-2,
    EQUAL,
    ADD,
    SUB,
    MULT,
    DIV,
    NEGATION
};
Q_ENUM_NS(Operations)
}

namespace StateEnums {
Q_NAMESPACE
enum States {
    RESETTED=-1,  // initial state
    EQUAL_TO_OP, // press an operation after "="
    EQUALS_LOOP, // press "=" twice or more
    OP_LOOP,     // press operations twice or more (without "=")
    OP_TO_EQUAL  // press "=" after some operations
};
Q_ENUM_NS(States)
}

enum Errors {
    NO_ERRORS,
    UNKNOW_OP,
    ZERO_DIVISION,
    NOT_FINITE
    //
};

class AppCore : public QObject
{
    Q_OBJECT
public:
    explicit AppCore(QObject *parent = nullptr);
    ~AppCore();

signals:
    void clearInputField();
    void setInput(QString val);
    void showTempResult(QString val, bool is_number);
    void clearTempResult();

public slots:
    void cppSlot(int op, QString v={});
    void handle_results(int, QVector<dec_n::Decimal<> >);
    void handle_results_queue(int, QVector<dec_n::Decimal<> >, int id);

protected:
    dec_n::Decimal<> _reg[2];
    dec_n::Decimal<> _prev_value;
    int _op;
    int _state;

    int _error_code;

    int _i_req = 0;
    int _i_res = 0;

    void _reset();

    void _f(int op) {
        switch (op) {
        case OperationEnums::ADD: _reg[1] = _reg[1] + _reg[0]; break;
        case OperationEnums::SUB: _reg[1] = _reg[1] - _reg[0]; break;
        case OperationEnums::MULT: _reg[1] = _reg[1] * _reg[0]; break;
        case OperationEnums::NEGATION: _reg[1] = dec_n::Decimal<>{} - _reg[1]; break;
        case OperationEnums::DIV:
            _reg[1] = _reg[1] / _reg[0];
            break;
        }
    }

    void _do(dec_n::Decimal<> val, int op);
};

