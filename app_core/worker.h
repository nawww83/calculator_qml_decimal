#pragma once

#include "AppCore.h"
#include "calculus.h"
#include "decimal.h"
#include "qobject.h"
#include <iostream>
#include "i128.hpp"

/**
 * @brief Класс "Работник", живущий в отдельном потоке и вызывающий библиотечную функцию расчета.
 */
class Worker : public QObject
{
    Q_OBJECT
private:
    QVector<dec_n::Decimal> v {dec_n::Decimal()};
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
    void do_work(int operation, QVector<dec_n::Decimal> operands) {
        int error_code;
        bool exact_sqrt = false;
        auto start = std::chrono::high_resolution_clock::now();
        if (operation == calculus::FACTOR) {
            auto f = calculus::factor(operands[0].IntegerPart().unsigned_part(), error_code);
            v.clear();
            dec_n::Decimal p;
            dec_n::Decimal q;
            for (auto [p_, q_] : f) {
                p.SetDecimal(p_, bignum::i128::I128{0});
                q.SetDecimal(bignum::i128::I128{static_cast<bignum::i128::ULOW>(q_), 0}, bignum::u128::U128{0});
                v.push_back(p);
                v.push_back(q);
            }
        } else {
            v.resize(1);
            // Операнды копируются.
            v[0] = doIt(operation, operands[0], operands[1], error_code, exact_sqrt);
        }
        auto stop = std::chrono::high_resolution_clock::now();
        if (operation == OperationEnums::FACTOR) {
            auto duration = duration_cast<std::chrono::seconds>(stop - start);
            g_console_output_mutex.lock();
            std::cout << "elapsed: " << duration.count() << " s" << std::endl;
            g_console_output_mutex.unlock();
        }
        emit results_ready(error_code, operation, exact_sqrt, v);
    }

    /**
     * @brief Синхронизировать количество знаков после запятой в Decimal в пространстве библиотеки.
     * @param width Количество знаков после запятой.
     */
    void sync_decimal_width(int width) {
        changeDecimalWidth(width);
    }
signals:
    /**
     * @brief Уведомляет контроллер о готовности результата.
     */
    void results_ready(int, int, bool, QVector<dec_n::Decimal>);
};

