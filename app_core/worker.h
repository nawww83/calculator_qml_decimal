#pragma once

#include "AppCore.h"
#include "calculus.h"
#include "decimal.h"
#include "qobject.h"
#include <iostream>

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
        auto start = std::chrono::high_resolution_clock::now();
        if (operation == calculus::FACTOR) {
            auto f = calculus::factor(operands[0].IntegerPart(), error_code);
            v.clear();
            dec_n::Decimal p;
            dec_n::Decimal q;
            for (auto [p_, q_] : f) {
                p.SetDecimal(p_, u128::get_zero());
                q.SetDecimal(u128::U128{static_cast<u128::ULOW>(q_), 0}, u128::get_zero());
                v.push_back(p);
                v.push_back(q);
            }
        } else {
            v.resize(1);
            // Операнды копируются.
            v[0] = doIt(operation, operands[0], operands[1], error_code);
        }
        auto stop = std::chrono::high_resolution_clock::now();
        if (operation == OperationEnums::FACTOR) {
            auto duration = duration_cast<std::chrono::seconds>(stop - start);
            g_console_output_mutex.lock();
            std::cout << "elapsed: " << duration.count() << " s" << std::endl;
            g_console_output_mutex.unlock();
        }
        emit results_ready(error_code, operation, v);
    }

    /**
     * @brief Синхронизировать количество знаков после запятой в Decimal в пространстве библиотеки.
     * @param width Количество знаков после запятой.
     * @param max_width Наибольшее количество знаков.
     */
    void sync_decimal_width(int width, int max_width) {
        changeDecimalWidth(width, max_width);
    }
signals:
    /**
     * @brief Уведомляет контроллер о готовности результата.
     */
    void results_ready(int, int, QVector<dec_n::Decimal>);
};

