#ifndef CALCULUS_H
#define CALCULUS_H

#include "calculus_global.h"
#include "decimal.h"

namespace c {

enum Ops {
    ADD = 0, // Сложение.
    SUB,     // Вычитание.
    MULT,    // Умножение.
    DIV,     // Деление.
    NEG      // Инверсия знака.
};

enum Errors {
    NO_ERRORS = 0,  // Нет ошибок.
    UNKNOW_OP,      // Неизвестная операция.
    ZERO_DIVISION,  // Деление на ноль.
    NOT_FINITE      // Переполнение.
};

}

/**
 * @brief Выполнить арифметическую операцию.
 * @param operation Операция.
 * @param x Операнд 1.
 * @param y Операнд 2.
 * @param error_code Код ошибки.
 * @return  Реузльтат операции.
 */
CALCULUS_EXPORT dec_n::Decimal doIt(int operation, dec_n::Decimal x, dec_n::Decimal y, int& error);

/**
 * @brief Изменить количество знаков после запятой у Decimal. Несмотря на статическую переменную,
 * это требуется, потому что это код библиотеки (dll, so), который отличается от кода основного приложения.
 * @param width Количество знаков после запятой.
 */
CALCULUS_EXPORT void changeDecimalWidth(int width);

#endif // CALCULUS_H
