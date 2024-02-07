#pragma once

#include <QObject>
#include <QThread>

#include "decimal.h"


class Controller : public QObject
{
    Q_OBJECT
    QThread workerThread;
public:
    explicit Controller(QObject *parent = nullptr);
    void quit() {
        workerThread.quit();
        workerThread.wait();
    }

signals:
    void operate(int, QVector<dec_n::Decimal<>>);
    void handle_results(int, QVector<dec_n::Decimal<>>);
    void set_delay(int t);

};

