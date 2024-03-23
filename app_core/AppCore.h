#pragma once

#include "decimal.h"
#include <QString>
#include <QVariant>

Q_DECLARE_METATYPE(dec_n::Decimal<>);

namespace OperationEnums {
Q_NAMESPACE
enum Operations {
    CLEAR_ALL = -2, // Служебные операции.
    EQUAL     = -1,
    ADD = 0,        // Математические операции.
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
    RESETTED = -1,  // Начальное состояние "Сброс".
    EQUAL_TO_OP,    // Введена некоторая операция после "Enter".
    EQUALS_LOOP,    // Зацикливание операции: "Enter" нажата дважды или больше.
    OP_LOOP,        // Зацикливание операции: введены две или более операции без нажатия "Enter".
    OP_TO_EQUAL     // Нажата "Enter" после некоторой операции.
};
Q_ENUM_NS(States)
}

enum Errors {
    NO_ERRORS = 0,  // Нет ошибок.
    UNKNOW_OP,      // Неизвестная операция.
    ZERO_DIVISION,  // Деление на ноль.
    NOT_FINITE      // Переполнение.
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
    void process(int requested_operation, QString input_value="");
    void handle_results(int, QVector<dec_n::Decimal<> >);
    void handle_results_queue(int, QVector<dec_n::Decimal<> >, int id);

protected:
    dec_n::Decimal<> mRegister[2];
    dec_n::Decimal<> mPreviousValue;
    int mCurrentOperation;
    int mState;
    int mErrorCode;
    int mRequestIdx = 0;
    int mResultIdx = 0;

    void Reset();

    void CalculateIt(int operation) {
        switch (operation) {
        case OperationEnums::ADD:      mRegister[1] = mRegister[1] + mRegister[0]; break;
        case OperationEnums::SUB:      mRegister[1] = mRegister[1] - mRegister[0]; break;
        case OperationEnums::MULT:     mRegister[1] = mRegister[1] * mRegister[0]; break;
        case OperationEnums::NEGATION: mRegister[1] = dec_n::Decimal<>{} - mRegister[1]; break;
        case OperationEnums::DIV:
            mRegister[1] = mRegister[1] / mRegister[0];
            break;
        }
    }

    void DoWork(dec_n::Decimal<> value, int operation);
};

