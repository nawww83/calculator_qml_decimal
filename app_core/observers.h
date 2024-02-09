#pragma once

#include <QThread>
#include <QSemaphore>
#include <QVector>

#include "types.h"
#include "controller.h"

namespace ro {

inline constexpr int request_time = 500;
inline constexpr int result_time = 300;

class RequestObserver : public QThread
{
    Q_OBJECT
public:
    explicit RequestObserver(QVector<_tp::Request>& r,
                             QSemaphore* used, QSemaphore* free,
                             Controller* c) :
        _r(r),
        _used(used),
        _free(free),
        _c(c) {}
    void run() override {
        _finish = false;
        while (!_finish) {
            if (!_used->tryAcquire(1, request_time)) {
                continue;
            }
            _index = (_index+1) % _tp::buff_size;
            emit _c->operate(_r[_index].op, _r[_index].xy);
            _free->release();
        }
    }

public slots:
    void finish() {
        _finish = true;
    }

signals:

protected:
    int _index = 0;
    QVector<_tp::Request>& _r;
    QSemaphore* _used;
    QSemaphore* _free;
    Controller* _c;
    bool _finish{};
};


class ResultObserver : public QThread
{
    Q_OBJECT
public:
    explicit ResultObserver(QVector<_tp::Result>& r,
                            QSemaphore* used, QSemaphore* free
                            /*Controller* c*/) :
       _r(r),
       _used(used),
       _free(free) {}
    void run() override {
        QVector<dec_n::Decimal<>> res(1);
        _finish = false;
        while (!_finish) {
            if (!_used->tryAcquire(1, result_time)) {
                continue;
            }
            _index = (_index+1) % _tp::buff_size;
            res[0] = _r[_index].z;
            emit handleResults(_r[_index].error, res, _index);
            _free->release();
        }
    }

public slots:
    void finish() {
        _finish = true;
    }

signals:
    void handleResults(int, QVector<dec_n::Decimal<>>, int);

protected:
    int _index = 0;
    QVector<_tp::Result>& _r;
    QSemaphore* _used;
    QSemaphore* _free;
    bool _finish{};
};

} // namespace ro
