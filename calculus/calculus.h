#ifndef CALCULUS_H
#define CALCULUS_H

#include "calculus_global.h"
#include "decimal.h"

namespace calculus {

enum Ops {
    ADD = 0,    // Сложение.
    SUB,        // Вычитание.
    MULT,       // Умножение.
    DIV,        // Деление.
    SEPARATOR,  // Разделитель двухоперандных/однооперандных операций.
    SQRT,       // Квадратный корень.
    SQR,        // Квадрат числа.
    RECIPROC,   // Обратное число.
    NEG,        // Инверсия знака.
    FACTOR      // Разложение на простые множители.
};

enum Errors {
    NO_ERRORS = 0,  // Нет ошибок.
    UNKNOW_OP,      // Неизвестная операция.
    ZERO_DIVISION,  // Деление на ноль.
    NOT_FINITE      // Переполнение.
};

/**
 * @brief Выполнить разложение на простые множители.
 * @param x Операнд 1.
 * @param error_code Код ошибки.
 * @return  Реузльтат операции: {простой множитель p, степень q}.
 */
CALCULUS_EXPORT std::map<u128::U128, int> factor(u128::U128 x, int& error);

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
 * @param max_width Наибольшее количество знаков.
 */
CALCULUS_EXPORT void changeDecimalWidth(int width, int max_width);

/**
 * @brief Остановить текущее вычисление разово.
 */
CALCULUS_EXPORT void stopCaclulation();

#endif // CALCULUS_H
