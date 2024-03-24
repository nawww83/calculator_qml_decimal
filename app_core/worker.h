#pragma once

#include "calculus.h"
#include "decimal.h"
#include "qobject.h"

/**
 * @brief Класс "Работник", живущий в отдельном потоке и вызывающий библиотечную функцию расчета.
 */
class Worker : public QObject
{
    Q_OBJECT
private:
    QVector<dec_n::Decimal<>> v {dec_n::Decimal<>()};
public:
    /**
     * @brief Конструктор.
     * @param parent Родительский объект.
     */
    explicit Worker(QObject *parent = nullptr) {Q_UNUSED(parent)};
public slots:
    /**
     * @brief Выполняет вызов библиотечной функции. Вызывается контроллером.
     * Уведомляет контроллер о готовности результата.
     * @param operation Код операции.
     * @param operands Операнды.
     */
    void do_work(int operation, QVector<dec_n::Decimal<>> operands) {
        int error_code;
        v[0] = doIt(operation, operands[0], operands[1], error_code);
        emit results_ready(error_code, v);
    }
signals:
    /**
     * @brief Уведомляет контроллер о готовности результата.
     */
    void results_ready(int, QVector<dec_n::Decimal<>>);
};

