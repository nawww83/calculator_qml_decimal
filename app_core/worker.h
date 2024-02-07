#pragma once

#include "calculus.h"
#include "decimal.h"
#include "qobject.h"


class Worker : public QObject
{
    Q_OBJECT
public:
    explicit Worker(QObject *parent = nullptr);
public slots:
    void do_work(int op, QVector<dec_n::Decimal<>> xy) {
        int error_code;
        dec_n::Decimal<> result {};
        result = doIt(op, xy[0], xy[1], error_code);
        QVector<dec_n::Decimal<>> v {result};
        emit results_ready(error_code, v);
    }
signals:
    void results_ready(int, QVector<dec_n::Decimal<>>);
};

