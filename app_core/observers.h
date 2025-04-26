#pragma once

#include <QThread>
#include <QSemaphore>
#include <QVector>

#include "types.h"
#include "controller.h"

namespace ro {

/**
 * @brief Сколько максимально ждать захвата семафора "Используется", мс.
 * В режиме "молчания" определяет частоту опроса запросов.
 */
inline constexpr int REQUEST_TIME = 500;

/**
 * @brief Сколько максимально ждать захвата семафора "Используется", мс.
 * В режиме "молчания" определяет частоту опроса ответов.
 */
inline constexpr int RESULT_TIME = 300;

/**
 * @brief Класс "Наблюдатель" запросов.
 */
class RequestObserver : public QThread
{
    Q_OBJECT
public:
    /**
     * @brief Конструктор.
     * @param request Вектор запросов.
     * @param used Семафор "Используется".
     * @param free Семафор "Свободно".
     * @param c Контроллер.
     */
    explicit RequestObserver(QVector<tp::Request>& request,
                             QSemaphore* used, QSemaphore* free,
                             Controller* c) :
        mRequest(request),
        mUsed(used),
        mFree(free),
        mController(c) {}

    /**
     * @brief Рабочая функция потока.
     */
    void run() override {
        mFinish = false;
        while (!mFinish) {
            if (!mUsed->tryAcquire(1, REQUEST_TIME)) {
                continue;
            }
            mIdx = (mIdx + 1) % tp::BUFFER_SIZE;
            emit mController->operate(mRequest[mIdx].mOperation, mRequest[mIdx].mOperands);
            mFree->release();
        }
    }

public slots:

    /**
     * @brief Останавливает работу потока.
     */
    void finish() {
        mFinish = true;
    }

signals:

protected:

    /**
     * @brief Идентификатор запроса.
     */
    int mIdx = 0;
    QVector<tp::Request>& mRequest;
    QSemaphore* mUsed;
    QSemaphore* mFree;
    Controller* mController;

    /**
     * @brief Флаг останова.
     */
    bool mFinish;
};

/**
 * @brief Класс "Наблюдатель" ответов (результатов).
 */
class ResultObserver : public QThread
{
    Q_OBJECT
public:
    /**
     * @brief Конструктор.
     * @param result Вектор ответов.
     * @param used Семафор "Используется".
     * @param free Семафор "Свободно".
     */
    explicit ResultObserver(QVector<tp::Result>& result,
                            QSemaphore* used, QSemaphore* free) :
        mResult(result),
        mUsed(used),
        mFree(free) {}

    /**
     * @brief Рабочая функция потока.
     */
    void run() override {
        QVector<dec_n::Decimal> res(1);
        mFinish = false;
        while (!mFinish) {
            if (!mUsed->tryAcquire(1, RESULT_TIME)) {
                continue;
            }
            mIdx = (mIdx + 1) % tp::BUFFER_SIZE;
            res = mResult[mIdx].mResult;
            emit handleResults(mResult[mIdx].mErrorCode, mResult[mIdx].mOperation, res, mIdx);
            mFree->release();
        }
    }

public slots:

    /**
     * @brief Останавливает работу потока.
     */
    void finish() {
        mFinish = true;
    }

signals:
    void handleResults(int, int, QVector<dec_n::Decimal>, int);

protected:

    /**
     * @brief Идентификатор ответа.
     */
    int mIdx = 0;
    QVector<tp::Result>& mResult;
    QSemaphore* mUsed;
    QSemaphore* mFree;

    /**
     * @brief Флаг останова.
     */
    bool mFinish;
};

} // namespace ro
