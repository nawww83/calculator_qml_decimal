#pragma once

#include "controller.h"
#include "decimal.h"
#include <QElapsedTimer>
#include "observers.h"
#include "types.h"
#include <QString>
#include <QVariant>
#include <QVariantList>
#include <QSemaphore>

namespace dec_n { class Decimal; }

Q_DECLARE_METATYPE(dec_n::Decimal);

namespace OperationEnums {
Q_NAMESPACE
enum Operations {
    CLEAR_ALL = -2, // Служебные операции.
    EQUAL     = -1,
    ADD = 0,        // Двухоперандные операции.
    SUB,
    MULT,
    DIV,
    POW,
    SEPARATOR_OP_TYPE, // Разделитель двухоперандных/однооперандных операций.
    SQRT,
    SQR,
    RECIPROC,
    LN,           // Натуральный логарифм.
    EXP,           // Экспонента.
    ROUND,        // Округление к ближайшему целому.
    FIX_NINES,    // Схлопывание паразитных девяток на конце.
    NEGATION,
    FACTOR,         // Разложить на простые множители.
    SEPARATOR_IO, // Разделитель операций ввода
    MAX_INT_VALUE,  // Получить наибольшее целое (положительное) число.
    RANDINT,         // Случайное целое полноразрядное число.
    RANDINT64       // Случайное целое 64-битное число.
};
Q_ENUM_NS(Operations)
}

namespace StateEnums {
Q_NAMESPACE
enum States {
    RESETTED = -1,  // Начальное состояние "Сброс".
    EQUAL_TO_OP,    // Введена некоторая операция после "Enter".
    EQUALS_LOOP,    // Зацикливание операции: "Enter" нажата дважды или больше.
    OP_LOOP,        // Зацикливание операции: введены две или более операции без нажатия "Enter".
    OP_TO_EQUAL     // Нажата "Enter" после некоторой операции.
};
Q_ENUM_NS(States)
}

enum Errors {
    NO_ERRORS = 0,    // Нет ошибок.
    UNKNOWN_OP,       // Неизвестная операция.
    ZERO_DIVISION,    // Деление на ноль.
    NOT_FINITE,       // Переполнение.
    NOT_A_NUMBER      // Не число.
};

/**
 * @brief Основной класс приложения (бизнес-логика).
 * Связывается с пространством QML для отображения GUI.
 * Связывается с "Наблюдателями" для формирования/обработки потока
 * запросов/ответов в/от библиотечной функции.
 */
class AppCore : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int userDisplayWidth READ userDisplayWidth NOTIFY changeDecimalWidth)
    Q_PROPERTY(QVariantList history READ history NOTIFY historyChanged)

public:
    /**
     * @brief Конструктор.
     * @param parent Родительский объект.
     */
    explicit AppCore(QObject *parent = nullptr);
    ~AppCore();

    // Дефолтная точность задана в одном месте — здесь:
    static constexpr int DEFAULT_DISPLAY_WIDTH = 3;

    // Максимальное количество строк истории, хранимых в памяти.
    static constexpr int MAX_HISTORY_SIZE = 500;

    int userDisplayWidth() const { return m_userDisplayWidth; }

    /**
     * @brief История операций для отображения в QML.
     * Каждая запись — QVariantMap с полями time, id, kind, headline, detail.
     */
    QVariantList history() const { return m_history; }

signals:

    /**
     * @brief Очистить поле ввода.
     */
    void clearInputField();

    /**
     * @brief Установить результат вычисления в поле ввода.
     * @param val Строковое представление числа-результата.
     */
    void setInput(QString val);

    /**
     * @brief setEnableFactorButton
     * @param val
     */
    void setEnableFactorButton(bool val);

    /**
     * @brief Показать результат во вспомогательном поле только для чтения.
     * @param val Строковое представление результата.
     * @param is_number Результат: число или нет.
     */
    void showTempResult(QString val, bool is_number);

    /**
     * @brief Показать текущую операцию во вспомогательном поле.
     * @param operation Строковое представление операции.
     */
    void showCurrentOperation(QString operation);

    /**
     * @brief Очистить вспомогательное поле результата.
     */
    void clearTempResult();

    /**
     * @brief Очистить вспомогательное поле текущей операции.
     */
    void clearCurrentOperation();

    /**
     * @brief Установить количество знаков после запятой на валидатор формы.
     * @param width Количество знаков после запятой.
     */
    void changeDecimalWidth(int width);

    /**
     * @brief История операций изменилась.
     */
    void historyChanged();

public slots:

    /**
     * @brief Обработать операцию в соответствии с бизнес-логикой калькулятора.
     * Вызывается из пространства QML.
     * @param requested_operation Запрашиваемая операция (кнопка калькулятора).
     * @param input_value Введенное число в строковом представлении (если имеется).
     */
    void process(int requested_operation, QString input_value="");

    /**
     * @brief Обработать пришедший из контроллера результат и положить в очередь.
     */
    void handle_results(int, int, QVector< dec_n::Decimal >);

    /**
     * @brief Обработать находящийся в очереди результат: отдать в GUI.
     * @param id Идентификатор запроса/ответа в очереди.
     */
    void handle_results_queue(int, int, QVector< dec_n::Decimal >, int id);

    /**
     * @brief Запрос на изменение количества знаков после запятой у чисел Decimal.
     * @param width Количество знаков после запятой.
     */
    void change_decimal_width(int width, bool quiet = false);

    /**
     * @brief Очистить историю операций.
     */
    Q_INVOKABLE void clearHistory();

protected:

    /**
     * @brief Регистр для хранения вводимых пользователем чисел.
     */
    dec_n::Decimal mRegister[2];

    /**
     * @brief Предыдущее введенное значение числа.
     */
    dec_n::Decimal mPreviousValue;

    /**
     * @brief Текущая запрашиваемая операция.
     */
    int mCurrentOperation;

    /**
     * @brief Текущее состояние калькулятора.
     */
    int mState;

    /**
     * @brief Текущий код ошибки.
     */
    int mErrorCode;

    /**
     * @brief Текущий индекс запроса.
     */
    int mRequestIdx = 0;

    /**
     * @brief Текущий индекс ответа (результата).
     */
    int mResultIdx = 0;

    /**
     * @brief Сброс калькулятора.
     */
    void Reset();

    /**
     * @brief Обработать введенную операцию.
     * @param value Введенное число.
     * @param operation Введенная операция.
     */
    void DoWork(dec_n::Decimal value, int operation);

private:
    Q_DISABLE_COPY(AppCore);

    /**
     * @brief Добавить запись в историю операций.
     * @param kind Тип записи: request, response, error.
     * @param headline Основная строка записи.
     * @param detail Дополнительная строка (может быть пустой).
     * @param id Идентификатор запроса/ответа; -1, если его нет.
     */
    void addHistoryEntry(const QString &kind, const QString &headline,
                         const QString &detail = QString(), int id = -1);

    // Порядок объявления важен: семафоры и буферы должны быть выше обсерверов
    QSemaphore m_reqFree{tp::BUFFER_SIZE};
    QSemaphore m_reqUsed{0};
    QSemaphore m_resFree{tp::BUFFER_SIZE};
    QSemaphore m_resUsed{0};

    QVector<tp::Request> m_requests;
    QVector<tp::Result> m_results;

    // Объекты управления
    Controller m_controller;
    std::unique_ptr<ro::RequestObserver> m_reqObs;
    std::unique_ptr<ro::ResultObserver> m_resObs;

    int m_userDisplayWidth = DEFAULT_DISPLAY_WIDTH;

    /**
     * @brief История операций для QML.
     */
    QVariantList m_history;

    QElapsedTimer mTimers[tp::BUFFER_SIZE]; // Массив таймеров по размеру буфера запросов
};

