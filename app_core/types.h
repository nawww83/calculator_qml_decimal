#pragma once

#include "decimal.h"
#include <QVector>


namespace tp {

/**
 * @brief Размер очередей запросов/ответов.
 */
constexpr inline int BUFFER_SIZE = 256;

/**
 * @brief Запрос.
 */
struct Request {
    /**
     * @brief Код операции.
     */
    int mOperation;
    /**
     * @brief Операнды.
     * Обернуты в Qt вектор из-за обеспечения безопасной передачи из потока в поток.
     */
    QVector<dec_n::Decimal> mOperands;
};

/**
 * @brief Ответ (результат запроса).
 */
struct Result {
    /**
     * @brief Код ошибки.
     */
    int mErrorCode;

    /**
     * @brief Код операции.
     */
    int mOperation;

    /**
     * @brief Точно ли извлекся квадратный корень.
     */
    bool mExactSqrt;

    /**
     * @brief Результат операции.
     * Обернут в Qt вектор из-за обеспечения безопасной передачи из потока в поток.
     */
    QVector<dec_n::Decimal> mResult;
};

}
