#include "controller.h"
#include "worker.h"


Controller::Controller(QObject *parent) : QObject(parent)
{
    auto worker = new Worker;

    worker->moveToThread(&workerThread);

    connect(&workerThread, &QThread::finished, worker, &QObject::deleteLater);
    connect(this, &Controller::operate, worker, &Worker::do_work);
    connect(worker, &Worker::results_ready, this, &Controller::handle_results);

    workerThread.start();
}
