#pragma once

#include "calculus.h"
#include "qobject.h"

/**
 * @brief Класс "Стопер", живущий в отдельном потоке и останавливающий расчет при необходимости.
 */
class Stopper : public QObject
{
    Q_OBJECT
private:
    ;
public:
    /**
     * @brief Конструктор.
     * @param parent Родительский объект.
     */
    explicit Stopper(QObject *parent = nullptr) {Q_UNUSED(parent)};
public slots:
    /**
     * @brief Остановить текущее вычисление разово.
     */
    void stop_calculation() {
        stopCaclulation();
    }
signals:
    ;
};

