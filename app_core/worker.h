#pragma once

#include "calculus.h"
#include "decimal.h"
#include "qobject.h"


class Worker : public QObject
{
    Q_OBJECT
private:
    QVector<dec_n::Decimal<>> v {dec_n::Decimal<>()};
public:
    explicit Worker(QObject *parent = nullptr) {Q_UNUSED(parent)};
public slots:
    // process the Request
    void do_work(int op, QVector<dec_n::Decimal<>> xy) {
        int error_code;
        v[0] = doIt(op, xy[0], xy[1], error_code);
        emit results_ready(error_code, v);
    }
signals:
    void results_ready(int, QVector<dec_n::Decimal<>>);
};

