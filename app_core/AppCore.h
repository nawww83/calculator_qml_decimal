#pragma once

#include "decimal.h"
#include <QString>
#include <QVariant>

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
    SEPARATOR, // Разделитель двухоперандных/однооперандных операций.
    SQRT,
    SQR,
    RECIPROC,
    NEGATION,
    MAX_INT_VALUE   // Получить наибольшее целое (положительное) число.
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
    NO_ERRORS = 0,  // Нет ошибок.
    UNKNOW_OP,      // Неизвестная операция.
    ZERO_DIVISION,  // Деление на ноль.
    NOT_FINITE      // Переполнение.
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
public:
    /**
     * @brief Конструктор.
     * @param parent Родительский объект.
     */
    explicit AppCore(QObject *parent = nullptr);
    ~AppCore();

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
    void handle_results(int, QVector< dec_n::Decimal >);

    /**
     * @brief Обработать находящийся в очереди результат: отдать в GUI.
     * @param id Идентификатор запроса/ответа в очереди.
     */
    void handle_results_queue(int, QVector< dec_n::Decimal >, int id);

    /**
     * @brief Запрос на изменение количества знаков после запятой у чисел Decimal.
     * @param width Количество знаков после запятой.
     * @param max_width Наибольшее количество знаков.
     */
    void change_decimal_width(int width, int max_width);

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
};

