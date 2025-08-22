#include "AppCore.h"

#include <QSemaphore>
#include <QRegularExpression>
#include "observers.h"

#include <QSettings>
#include <QDebug>

/**
 * @brief Модификаторы строк: зеленый, синий, красный и "отмена раскраски".
 */
namespace modifiers {
    static const auto green = "\033[32m";
    static const auto bright_blue = "\033[94m";
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
        case OperationEnums::ADD:           return QString::fromUtf8("Сложение");
        case OperationEnums::SUB:           return QString::fromUtf8("Вычитание");
        case OperationEnums::MULT:          return QString::fromUtf8("Умножение");
        case OperationEnums::DIV:           return QString::fromUtf8("Деление");
        case OperationEnums::EQUAL:         return QString::fromUtf8("Равно");
        case OperationEnums::SQRT:          return QString::fromUtf8("Квадратный корень");
        case OperationEnums::SQR:           return QString::fromUtf8("Квадрат числа");
        case OperationEnums::RECIPROC:      return QString::fromUtf8("Обратное число");
        case OperationEnums::SEPARATOR:     return QString::fromUtf8("Разделитель операций: недопустимая операция!");
        case OperationEnums::NEGATION:      return QString::fromUtf8("Смена знака");
        case OperationEnums::CLEAR_ALL:     return QString::fromUtf8("Сброс");
        case OperationEnums::MAX_INT_VALUE: return QString::fromUtf8("Наибольшее целое число");
        case OperationEnums::RANDINT:       return QString::fromUtf8("Случайное число");
        case OperationEnums::RANDINT64:     return QString::fromUtf8("Случайное 64-битное число");
        case OperationEnums::FACTOR:        return QString::fromUtf8("Разложить на простые множители целую часть числа");
        default:                            return QString::fromUtf8("Неизвестная операция");
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
    emit controller.stop_calculation();
    qDebug() << "~AppCore: stop all threads...";
    reqObs.finish();
    resObs.finish();
    reqObs.wait();
    resObs.wait();
    controller.quit();
    qDebug() << "~AppCore: quit!";
}

void AppCore::Reset() {
    mRegister[0] = dec_n::Decimal{};
    mRegister[1] = dec_n::Decimal{};
    mPreviousValue = dec_n::Decimal{};
    mCurrentOperation = OperationEnums::CLEAR_ALL;
    mState = StateEnums::RESETTED;
}

void AppCore::DoWork(dec_n::Decimal value, int operation) {
    // Послать запрос в очередь.
    auto push_request = [this, operation]() {
        QVector<dec_n::Decimal> v {mRegister[1], mRegister[0]};
        mRequestIdx = (mRequestIdx + 1) % tp::BUFFER_SIZE;
        if (operation < OperationEnums::SEPARATOR) {
            std::string_view sv0 = mRegister[0].ValueAsStringView();
            std::string_view sv1 = mRegister[1].ValueAsStringView();
            qDebug().noquote() << modifiers::green << QString::fromUtf8("Запрос:")
                               << description(operation) << "x:" << QString::fromStdString({sv1.data(), sv1.size()}).toUtf8()
                               << "y:" << QString::fromStdString({sv0.data(), sv0.size()}).toUtf8()
                               << "ID:" << mRequestIdx
                     << modifiers::esc_colorization;
        } else {
            std::string_view sv1 = mRegister[1].ValueAsStringView();
            qDebug().noquote() << modifiers::green << QString::fromUtf8("Запрос:")
                               << description(operation) << "x:" << QString::fromStdString({sv1.data(), sv1.size()}).toUtf8()
                               << "ID:" << mRequestIdx
                               << modifiers::esc_colorization;
        }
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
        default:
            mRegister[1] = value;
            push_request();
    }
}

void AppCore::process(int requested_operation, QString input_value)
{
    // Операция сброса|остановки вычислений.
    if (requested_operation == OperationEnums::CLEAR_ALL) {
        g_console_output_mutex.lock();
        qDebug().noquote() << QString::fromUtf8("Операция:") << description(requested_operation);
        g_console_output_mutex.unlock();
        emit clearTempResult();
        emit clearCurrentOperation();
        if (mCurrentOperation != OperationEnums::FACTOR)
            emit clearInputField();
        emit controller.stop_calculation();
        Reset();
        return;
    }
    if (requested_operation == OperationEnums::MAX_INT_VALUE) {
        u128::U128 max_value = u128::U128::get_max_value();
        emit setInput(QString::fromStdString(max_value.value()));
        return;
    }
    if (requested_operation == OperationEnums::RANDINT) {
        u128::U128 value = u128::utils::get_random_value();
        emit setInput(QString::fromStdString(value.value()));
        return;
    }
    if (requested_operation == OperationEnums::RANDINT64) {
        u128::U128 value = u128::utils::get_random_half_value();
        emit setInput(QString::fromStdString(value.value()));
        return;
    }
    // По введенной строке сконструировать число Decimal.
    auto construct_decimal = [&input_value]() {
        static auto re = QRegularExpression{"(\\s)"};
        input_value.remove(re);
        const auto str = input_value.toStdString();
        dec_n::Decimal val;
        val.SetStringRepresentation(str);
        return val;
    };
    auto val = construct_decimal();
    if (val.IsOverflowed()) {
        int err = Errors::NOT_FINITE;
        emit clearInputField();
        emit clearCurrentOperation();
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
    // Запрет факторизации если вводится второй операнд.
    if (requested_operation == OperationEnums::FACTOR && ((mState == StateEnums::EQUAL_TO_OP) || (mState == StateEnums::OP_LOOP))) {
        return;
    }
    if (!is_not_a_number && requested_operation == OperationEnums::FACTOR) {
        emit setEnableFactorButton(false);
        qDebug().noquote() << QString::fromUtf8("Операция:") << description(requested_operation);
        mCurrentOperation = OperationEnums::FACTOR;
        emit showCurrentOperation(description(requested_operation));
        DoWork(val, requested_operation);
        return;
    }
    if (requested_operation < OperationEnums::SEPARATOR &&
        requested_operation != OperationEnums::EQUAL) {
        emit showCurrentOperation(description(requested_operation));
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
        // Игнорирование однооперандной операции
        if (requested_operation > OperationEnums::SEPARATOR) {
            return;
        } else
        // Смена текущей двухоперандной операции пользователем.
        if (requested_operation >= 0 && requested_operation < OperationEnums::SEPARATOR) {
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
        val = -val;
        std::string_view sv = val.ValueAsStringView();
        emit setInput(QString::fromStdString({sv.data(), sv.size()}));
        return;
    }
    if (requested_operation == OperationEnums::SQRT) {
        if ((mState == StateEnums::EQUAL_TO_OP) || (mState == StateEnums::OP_LOOP)) {
            qDebug().noquote() << QString::fromUtf8("Операция:") << description(requested_operation) << QString::fromUtf8("из") << val.ValueAsStringView();
            bool exact;
            val = dec_n::Sqrt(val, exact);
            std::string_view sv = val.ValueAsStringView();
            emit setInput(QString::fromStdString({sv.data(), sv.size()}));
            return;
        } else {
            mCurrentOperation = requested_operation;
            emit showCurrentOperation(description(OperationEnums::SQRT));
        }
    }
    if (requested_operation == OperationEnums::SQR) {
        if ((mState == StateEnums::EQUAL_TO_OP) || (mState == StateEnums::OP_LOOP)) {
            qDebug().noquote() << QString::fromUtf8("Операция:") << description(requested_operation) << val.ValueAsStringView();
            val = val * val;
            if (val.IsOverflowed()) {
                int err = Errors::NOT_FINITE;
                emit clearInputField();
                emit clearCurrentOperation();
                emit showTempResult(err_description(err), false);
                Reset();
                qDebug().noquote() << modifiers::red << QString::fromUtf8("Ошибка:") << err_description(err) << modifiers::esc_colorization;
                return;
            }
            std::string_view sv = val.ValueAsStringView();
            emit setInput(QString::fromStdString({sv.data(), sv.size()}));
            return;
        } else {
            mCurrentOperation = requested_operation;
            emit showCurrentOperation(description(OperationEnums::SQR));
        }
    }
    if (requested_operation == OperationEnums::RECIPROC) {
        if ((mState == StateEnums::EQUAL_TO_OP) || (mState == StateEnums::OP_LOOP)) {
            qDebug().noquote() << QString::fromUtf8("Операция:") << description(requested_operation) << QString::fromUtf8("от") << val.ValueAsStringView();
            if (val.IsZero()) {
                int err = Errors::ZERO_DIVISION;
                emit clearInputField();
                emit clearCurrentOperation();
                emit showTempResult(err_description(err), false);
                Reset();
                qDebug().noquote() << modifiers::red << QString::fromUtf8("Ошибка:") << err_description(err) << modifiers::esc_colorization;
                return;
            }
            dec_n::Decimal one;
            one.SetDecimal(u128::U128{1}, u128::U128{0});
            val = one / val;
            if (val.IsOverflowed()) {
                int err = Errors::NOT_FINITE;
                emit clearInputField();
                emit clearCurrentOperation();
                emit showTempResult(err_description(err), false);
                Reset();
                qDebug().noquote() << modifiers::red << QString::fromUtf8("Ошибка:") << err_description(err) << modifiers::esc_colorization;
                return;
            }
            std::string_view sv = val.ValueAsStringView();
            emit setInput(QString::fromStdString({sv.data(), sv.size()}));
            return;
        } else {
            mCurrentOperation = requested_operation;
            emit showCurrentOperation(description(OperationEnums::RECIPROC));
        }
    }
    if (requested_operation == OperationEnums::EQUAL && mCurrentOperation == OperationEnums::FACTOR) {
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
    //
    const bool current_is_two_operand_operation = (requested_operation >= 0 && requested_operation < OperationEnums::SEPARATOR);
    const bool current_is_one_operand_operation = requested_operation > OperationEnums::SEPARATOR;
    const bool state_is_operation = (mState == StateEnums::EQUAL_TO_OP) || (mState == StateEnums::OP_LOOP);
    const bool state_is_the_equal = (mState == StateEnums::EQUALS_LOOP) || (mState == StateEnums::OP_TO_EQUAL);
    const bool state_is_resetted = (mState == StateEnums::RESETTED);
    // Запрашиваемая операция двухоперандная, а калькулятор в состоянии "После ввода операции".
    if (current_is_two_operand_operation && state_is_operation) {
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
    // Запрашиваемая операция "Равно" или однооперандная.
    if (requested_operation == OperationEnums::EQUAL || current_is_one_operand_operation) {
        if (state_is_operation) {
            mState = StateEnums::OP_TO_EQUAL;
        } else {
            mState = StateEnums::EQUALS_LOOP;
        }
    }
    // Запрашиваемая операция двухоперандная, а калькулятор находится в состоянии "После ввода Enter" или сброшен.
    if (current_is_two_operand_operation) {
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

void AppCore::handle_results(int err, int operation, bool exact_sqrt, QVector<dec_n::Decimal> res)
{
    // Поместить результат в регистр для дальнейших операций (цепочка операций).
    mRegister[1] = res.first();

    auto push_result = [this, err, operation, exact_sqrt, res]() {
        results_free.acquire();
        mResultIdx = (mResultIdx + 1) % tp::BUFFER_SIZE;
        results[mResultIdx] = {err, operation, exact_sqrt, res};
        results_used.release();
    };

    push_result();
}

void AppCore::handle_results_queue(int err, int operation, bool exact_sqrt, QVector<dec_n::Decimal> res, int id)
{
    if (err == Errors::NO_ERRORS) {
        QDebug deb(QtDebugMsg);
        const bool state_is_the_equal =
            (mState == StateEnums::EQUALS_LOOP) ||
                                        (mState == StateEnums::OP_TO_EQUAL);
        if (operation == OperationEnums::SQRT) {
            emit showCurrentOperation(description(OperationEnums::SQRT) + QString::fromUtf8(exact_sqrt ? ": точно." : ": приближенно."));
        }
        if (operation == OperationEnums::FACTOR) {
            // Отобразить операцию в истории.
            deb.noquote().nospace() << modifiers::bright_blue << QString::fromUtf8("Ответ: ID: ") << id <<
                QString::fromUtf8(", результат: ");
            u128::U128 prime;
            deb.noquote().nospace() << "{";
            for (int i = 0; const auto& el : res) {
                i++;
                if (i % 2)
                    prime = el.IntegerPart();
                else {
                    int power = el.IntegerPart().mLow;
                    const std::string& prime_str = prime.value();
                    deb.noquote().nospace() << QString::fromStdString({prime_str.data(), prime_str.size()}) << "^" << power;
                    if (i == res.size()) {
                        ;
                    } else {
                        deb.noquote().nospace() << "; ";
                    }
                }
            }
            deb.noquote().nospace() << "}" << modifiers::esc_colorization;
            mCurrentOperation = OperationEnums::CLEAR_ALL;
            mState = StateEnums::RESETTED;
            emit setEnableFactorButton(true);
        } else {
            // Показать результат в поле ввода, если нажата "Enter".
            if (state_is_the_equal || mState == StateEnums::RESETTED) {
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
                deb.noquote().nospace() << modifiers::bright_blue << QString::fromUtf8("Ответ: ID: ") << id << QString::fromUtf8(", результат: ") <<
                    QString::fromStdString({sv.data(), sv.size()}) << modifiers::esc_colorization;
            }
        }
    } else
    // Отобразить ошибку.
    {
        emit clearInputField();
        emit clearCurrentOperation();
        emit showTempResult(err_description(err), false);
        Reset();
        qDebug().noquote() << modifiers::red << QString::fromUtf8("Ошибка:") << err_description(err) << modifiers::esc_colorization;
    }
}

void AppCore::change_decimal_width(int width, bool quiet)
{
    const bool is_changed = dec_n::Decimal::SetWidth(width);
    emit controller.sync_decimal_width(width);
    if (is_changed) {
        Reset();
        emit clearTempResult();
        emit clearCurrentOperation();
        emit clearInputField();
        emit changeDecimalWidth(dec_n::Decimal::GetWidth());
        if (!quiet)
            qDebug().noquote() << modifiers::red << QString::fromUtf8("Изменено количество знаков после запятой: ") <<
                            dec_n::Decimal::GetWidth() << modifiers::esc_colorization;
        QSettings settings("MyHome", "DecimalCalculator");
        settings.setValue("DecimalWidth", width);
    }
}
