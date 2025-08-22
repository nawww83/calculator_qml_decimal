#pragma once

#include "decimal.h"
#include "qthread.h"

class QThread;

/**
 * @brief Класс "Контроллер", запускающий "Работника" в отдельном потоке.
 * Является связующим звеном между "Наблюдателем" приложения и библиотечной функцией.
 */
class Controller : public QObject
{
    Q_OBJECT
    /**
     * @brief Рабочий поток.
     */
    QThread workerThread;

    /**
     * @brief Поток для остановки расчетов.
     */
    QThread stopThread;
public:
    /**
     * @brief Конструктор.
     * @param parent Родительский объект.
     */
    explicit Controller(QObject *parent = nullptr);
    void quit() {
        workerThread.quit();
        workerThread.wait();
        stopThread.quit();
        stopThread.wait();
    }

signals:
    /**
     * @brief Сигнализирует "Работнику" о запросе на выполнение.
     */
    void operate(int, QVector<dec_n::Decimal>);

    /**
     * @brief Сигнализирует "Наблюдателю" приложения о готовности результата.
     */
    void handle_results(int, int, bool, QVector<dec_n::Decimal>);

    /**
     * @brief Сигнал синхронизации количества знаков после запятой для Decimal в пространстве библиотеки.
     */
    void sync_decimal_width(int);

    /**
     * @brief Остановить текущее вычисление разово.
     */
    void stop_calculation();
};

