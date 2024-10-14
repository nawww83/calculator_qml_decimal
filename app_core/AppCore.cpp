#include "AppCore.h"

#include <QSemaphore>
#include <QRegularExpression>
#include "observers.h"

#include <QDebug>

/**
 * @brief Модификаторы строк: зеленый, синий, красный и "отмена раскраски".
 */
namespace modifiers {
    static const auto green = "\033[32m";
    static const auto blue = "\033[34m";
    static const auto red = "\033[31m";
    static const auto esc_colorization = "\033[0m";
}

/**
 * @brief Семафоры запросов и ответов.
 */
QSemaphore requests_free(tp::BUFFER_SIZE);
QSemaphore requests_used(0);
QSemaphore results_free(tp::BUFFER_SIZE);
QSemaphore results_used(0);

/**
 * @brief Буферы запросов и ответов.
 */
QVector<tp::Request> requests(tp::BUFFER_SIZE);
QVector<tp::Result> results(tp::BUFFER_SIZE);

/**
 * @brief Контроллер.
 */
Controller controller;

/**
 * @brief Наблюдатели очередей запросов и ответов.
 */
ro::RequestObserver reqObs(requests,
                           &requests_used, &requests_free,
                           &controller);
ro::ResultObserver resObs(results,
                          &results_used, &results_free);

/**
 * @brief Возвращает описание ошибки по коду ошибки.
 */
static constexpr auto err_description = [](int error_code) -> QString {
    switch (error_code) {
        case Errors::ZERO_DIVISION: return QString::fromUtf8("Деление на ноль");
        case Errors::UNKNOW_OP:     return QString::fromUtf8("Неизвестная операция");
        case Errors::NOT_FINITE:    return QString::fromUtf8("Переполнение");
        default:                    return QString::fromUtf8("Нет ошибок");
    }
};

/**
 * @brief Возвращает описание операции по ее коду.
 */
static constexpr auto description = [](int operation) -> QString {
    switch (operation) {
        case OperationEnums::ADD:       return QString::fromUtf8("Сложение");
        case OperationEnums::SUB:       return QString::fromUtf8("Вычитание");
        case OperationEnums::MULT:      return QString::fromUtf8("Умножение");
        case OperationEnums::DIV:       return QString::fromUtf8("Деление");
        case OperationEnums::EQUAL:     return QString::fromUtf8("Равно");
        case OperationEnums::NEGATION:  return QString::fromUtf8("Смена знака");
        case OperationEnums::CLEAR_ALL: return QString::fromUtf8("Сброс");
        default:                        return QString::fromUtf8("Неизвестная операция");
    }
};

AppCore::AppCore(QObject *parent) : QObject(parent)
{
    Reset();

    connect(&controller, &Controller::handle_results, this, &AppCore::handle_results);
    connect(&resObs, &ro::ResultObserver::handleResults, this, &AppCore::handle_results_queue);

    resObs.start();
    reqObs.start();

    qDebug() << "Welcome!";
}

AppCore::~AppCore()
{
    qDebug() << "~AppCore: stop all threads...";
    reqObs.finish();
    resObs.finish();
    reqObs.wait();
    resObs.wait();
    controller.quit();
    qDebug() << "~AppCore: quit!";
}

void AppCore::Reset() {
    mRegister[0] = dec_n::Decimal<>{};
    mRegister[1] = dec_n::Decimal<>{};
    mPreviousValue = dec_n::Decimal<>{};
    mCurrentOperation = OperationEnums::CLEAR_ALL;
    mState = StateEnums::RESETTED;
}

void AppCore::DoWork(dec_n::Decimal<> value, int operation) {
    // Послать запрос в очередь.
    auto push_request = [this, operation]() {
        QVector<dec_n::Decimal<>> v {mRegister[1], mRegister[0]};
        std::string_view sv1 = mRegister[1].ValueAsStringView();
        std::string_view sv0 = mRegister[0].ValueAsStringView();
        mRequestIdx = (mRequestIdx + 1) % tp::BUFFER_SIZE;
        qDebug().noquote() << modifiers::green << QString::fromUtf8("Запрос:")
                           << description(operation) << "x:" << QString::fromStdString({sv1.data(), sv1.size()}).toUtf8()
                           << "y:" << QString::fromStdString({sv0.data(), sv0.size()}).toUtf8()
                           << "ID:" << mRequestIdx
                 << modifiers::esc_colorization;
        requests_free.acquire();
        requests[mRequestIdx] = {operation, v};
        requests_used.release();
    };
    switch (mState) {
        case StateEnums::EQUALS_LOOP:
        mRegister[1] = value;
            push_request();
            break;
        case StateEnums::EQUAL_TO_OP:
            mRegister[1] = value;
            break;
        case StateEnums::OP_LOOP:
        case StateEnums::OP_TO_EQUAL:
            mRegister[0] = value;
            push_request();
            break;
        default: ;
    }
}

void AppCore::process(int requested_operation, QString input_value)
{
    // Операция сброса.
    if (requested_operation == OperationEnums::CLEAR_ALL) {
        qDebug().noquote() << QString::fromUtf8("Операция:") << description(requested_operation);
        Reset();
        emit clearTempResult();
        emit clearInputField();
        return;
    }
    // По введенной строке сконструировать число Decimal.
    auto construct_decimal = [&input_value]() {
        static auto re = QRegularExpression{"(\\s)"};
        input_value.remove(re);
        auto s = input_value.toStdString();
        dec_n::Decimal<> val;
        val.SetStringRepresentation(s);
        return val;
    };
    auto val = construct_decimal();
    if (val.IsOverflowed()) {
        int err = Errors::NOT_FINITE;
        emit clearInputField();
        emit showTempResult(err_description(err), false);
        Reset();
        qDebug().noquote() << modifiers::red << QString::fromUtf8("Ошибка:") << err_description(err) << modifiers::esc_colorization;
        return;
    }
    //
    const bool is_not_a_number = val.IsNotANumber();
    // Игнорирование запросов при сброшенном состоянии и пустом поле ввода.
    if ((mState == StateEnums::RESETTED) && is_not_a_number) {
        return;
    }
    // Обработка запроса при пустом поле ввода и существовании математической операции.
    if (is_not_a_number) {
        if (requested_operation == mCurrentOperation) {
            qDebug().noquote() << modifiers::red << QString::fromUtf8("Повтор операции.") << modifiers::esc_colorization;
            return;
        }
        // Сохранить текущую математическую операцию при нажатии Enter и пустом поле ввода.
        if ((requested_operation == OperationEnums::EQUAL) && (mCurrentOperation >= 0)) {
            qDebug().noquote() << QString::fromUtf8("Операция:") << description(requested_operation);
            return;
        }
        // Смена текущей математической операции пользователем.
        {
            qDebug().noquote() << QString::fromUtf8("Операция:") << description(requested_operation);
            mCurrentOperation = requested_operation;
            return;
        }
    }
    const bool current_val_is_the_same = (val == mPreviousValue);
    mPreviousValue = val;
    // Обработка частичных операций.
    // Операция смены знака.
    if (requested_operation == OperationEnums::NEGATION) {
        qDebug().noquote() << QString::fromUtf8("Операция:") << description(requested_operation);
        val = dec_n::Decimal<>{} - val;
        std::string_view sv = val.ValueAsStringView();
        emit setInput(QString::fromStdString({sv.data(), sv.size()}));
        return;
    }
    // При нажатии Enter и отсутствии математической операции при изменении числа обновить введенное значение форматным.
    if ((requested_operation == OperationEnums::EQUAL) && (mCurrentOperation < 0) && (!current_val_is_the_same)) {
        qDebug().noquote() << QString::fromUtf8("Операция:") << description(requested_operation);
        std::string_view sv = val.ValueAsStringView();
        emit clearTempResult();
        emit setInput(QString::fromStdString({sv.data(), sv.size()}));
        mCurrentOperation = requested_operation;
        return;
    }
    // При повторном нажатии Enter и неизменности вводимого числа игнорировать запрос.
    if ((requested_operation == OperationEnums::EQUAL) && (mCurrentOperation == requested_operation) && (current_val_is_the_same)) {
        qDebug().noquote() << modifiers::red << QString::fromUtf8("Повтор операции.") << modifiers::esc_colorization;
        return;
    }
    // Обработка полноценных операций.
    //
    const bool current_is_math_operation = (requested_operation >= 0);
    const bool state_is_operation = (mState == StateEnums::EQUAL_TO_OP) || (mState == StateEnums::OP_LOOP);
    const bool state_is_the_equal = (mState == StateEnums::EQUALS_LOOP) || (mState == StateEnums::OP_TO_EQUAL);
    const bool state_is_resetted = (mState == StateEnums::RESETTED);
    // Запрашиваемая операция математическая и калькулятор в состоянии "После ввода операции".
    if (current_is_math_operation && state_is_operation) {
        qDebug().noquote() << QString::fromUtf8("Операция:") << description(requested_operation);
        mState = StateEnums::OP_LOOP;
        emit clearInputField();
        std::string_view sv = val.ValueAsStringView();
        emit showTempResult(QString::fromStdString({sv.data(), sv.size()}), true);
        // Выполнить операцию.
        DoWork(val, mCurrentOperation);
        mCurrentOperation = requested_operation;
        return;
    }
    qDebug().noquote() << QString::fromUtf8("Операция:") << description(requested_operation);
    // Запрашиваемая операция "Равно"
    if (! current_is_math_operation) {
        if (state_is_operation) {
            mState = StateEnums::OP_TO_EQUAL;
        } else {
            mState = StateEnums::EQUALS_LOOP;
        }
    }
    // Запрашиваемая операция математическая и калькулятор в состоянии "После Enter" или сброшен.
    if (current_is_math_operation) {
        if (state_is_the_equal) {
            mState = StateEnums::EQUAL_TO_OP;
        }
        if (state_is_resetted) {
            mRegister[1] = val;
            mState = StateEnums::EQUAL_TO_OP;
        }
        mCurrentOperation = requested_operation;
        // Очистить поле ввода и показать введеное число в поле "Только для чтения" ниже.
        {
            emit clearInputField();
            std::string_view sv = val.ValueAsStringView();
            emit showTempResult(QString::fromStdString({sv.data(), sv.size()}), true);
        }
    }
    // Выполнить основную операцию.
    DoWork(val, mCurrentOperation);
}

void AppCore::handle_results(int err, QVector<dec_n::Decimal<>> res)
{
    // Поместить результат в регистр для дальнейших операций (цепочка операций).
    mRegister[1] = res.first();

    auto push_result = [this, err, res]() {
        results_free.acquire();
        mResultIdx = (mResultIdx + 1) % tp::BUFFER_SIZE;
        results[mResultIdx] = {err, res.first()};
        results_used.release();
    };

    push_result();
}

void AppCore::handle_results_queue(int err, QVector<dec_n::Decimal<>> res, int id)
{
    if (err == Errors::NO_ERRORS) {
        const bool state_is_the_equal =
            (mState == StateEnums::EQUALS_LOOP) ||
                                        (mState == StateEnums::OP_TO_EQUAL);
        // Показать результат в поле ввода, если нажата "Enter".
        if (state_is_the_equal) {
            std::string_view sv = res[0].ValueAsStringView().data();
            emit setInput(QString::fromStdString({sv.data(), sv.size()}));
            emit clearTempResult();
        } else
        // Показать результат в поле "Только для чтения" ниже, если выполняется цепочка операций.
        {
            std::string_view sv = mRegister[1].ValueAsStringView();
            emit showTempResult(QString::fromStdString({sv.data(), sv.size()}), true);
        }
        // Отобразить операцию в истории.
        {
            std::string_view sv = res[0].ValueAsStringView();
            qDebug().noquote() << modifiers::blue << QString::fromUtf8("Ответ ID:") << id << QString::fromUtf8("результат:") <<
                                QString::fromStdString({sv.data(), sv.size()}) << modifiers::esc_colorization;
        }
    } else
    // Отобразить ошибку.
    {
        emit clearInputField();
        emit showTempResult(err_description(err), false);
        Reset();
        qDebug().noquote() << modifiers::red << QString::fromUtf8("Ошибка:") << err_description(err) << modifiers::esc_colorization;
    }
}
