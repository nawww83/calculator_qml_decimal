#include "controller.h"
#include "worker.h"

Controller::Controller(QObject *parent) : QObject(parent)
{
    auto worker = new Worker;

    worker->moveToThread(&workerThread);

    connect(&workerThread, &QThread::finished, worker, &QObject::deleteLater);
    connect(this, &Controller::operate, worker, &Worker::do_work);
    // Требуется синхронность, т.к. это сигнал синхронизации.
    connect(this, &Controller::sync_decimal_width, worker, &Worker::sync_decimal_width, Qt::BlockingQueuedConnection);
    connect(worker, &Worker::results_ready, this, &Controller::handle_results);

    workerThread.start();
}
