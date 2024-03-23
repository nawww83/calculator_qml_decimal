#pragma once

#include <QThread>
#include <QSemaphore>
#include <QVector>

#include "types.h"
#include "controller.h"

namespace ro {

inline constexpr int REQUEST_TIME = 500;
inline constexpr int RESULT_TIME = 300;

class RequestObserver : public QThread
{
    Q_OBJECT
public:
    explicit RequestObserver(QVector<_tp::Request>& r,
                             QSemaphore* used, QSemaphore* free,
                             Controller* c) :
        mRequest(r),
        mUsed(used),
        mFree(free),
        mController(c) {}
    void run() override {
        mFinish = false;
        while (!mFinish) {
            if (!mUsed->tryAcquire(1, REQUEST_TIME)) {
                continue;
            }
            mIdx = (mIdx+1) % _tp::buff_size;
            emit mController->operate(mRequest[mIdx].op, mRequest[mIdx].xy);
            mFree->release();
        }
    }

public slots:
    void finish() {
        mFinish = true;
    }

signals:

protected:
    int mIdx = 0;
    QVector<_tp::Request>& mRequest;
    QSemaphore* mUsed;
    QSemaphore* mFree;
    Controller* mController;
    bool mFinish{};
};


class ResultObserver : public QThread
{
    Q_OBJECT
public:
    explicit ResultObserver(QVector<_tp::Result>& r,
                            QSemaphore* used, QSemaphore* free
                            /*Controller* c*/) :
        mResult(r),
        mUsed(used),
        mFree(free) {}
    void run() override {
        QVector<dec_n::Decimal<>> res(1);
        mFinish = false;
        while (!mFinish) {
            if (!mUsed->tryAcquire(1, RESULT_TIME)) {
                continue;
            }
            mIdx = (mIdx + 1) % _tp::buff_size;
            res[0] = mResult[mIdx].z;
            emit handleResults(mResult[mIdx].error, res, mIdx);
            mFree->release();
        }
    }

public slots:
    void finish() {
        mFinish = true;
    }

signals:
    void handleResults(int, QVector<dec_n::Decimal<>>, int);

protected:
    int mIdx = 0;
    QVector<_tp::Result>& mResult;
    QSemaphore* mUsed;
    QSemaphore* mFree;
    bool mFinish{};
};

} // namespace ro
