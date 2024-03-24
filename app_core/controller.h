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
public:
    /**
     * @brief Конструктор.
     * @param parent Родительский объект.
     */
    explicit Controller(QObject *parent = nullptr);
    void quit() {
        workerThread.quit();
        workerThread.wait();
    }

signals:
    /**
     * @brief Сигнализирует "Работнику" о запросе на выполнение.
     */
    void operate(int, QVector<dec_n::Decimal<>>);
    /**
     * @brief Сигнализирует "Наблюдателю" приложения о готовности результата.
     */
    void handle_results(int, QVector<dec_n::Decimal<>>);
};

