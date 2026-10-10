#include "controller.h"
#include "worker.h"
#include "stopper.h"

Controller::Controller(QObject *parent) : QObject(parent)
{
    auto worker = new Worker;
    worker->moveToThread(&workerThread);

    auto stopper = new Stopper;
    stopper->moveToThread(&stopThread);

    connect(&workerThread, &QThread::finished, worker, &QObject::deleteLater);
    connect(&stopThread, &QThread::finished, stopper, &QObject::deleteLater);
    connect(this, &Controller::operate, worker, &Worker::do_work);
    connect(this, &Controller::stop_calculation, stopper, &Stopper::stop_calculation);
    connect(worker, &Worker::results_ready, this, &Controller::handle_results);

    workerThread.start();
    stopThread.start();
}
