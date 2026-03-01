#include "AppCore.h"

#include <QRegularExpression>
#include <QTime>

#include "u128.hpp"
#include "i128.hpp"

#include <QSettings>
#include <memory>
#include <QDebug>
#include <QTimer>

/**
 * @brief Модификаторы строк.
 */
namespace modifiers {
    static const auto gray    = "\033[90m";   // Время, ID, метки x: и y:
    static const auto green   = "\033[32m";   // Исходящие запросы (-->)
    static const auto blue    = "\033[1;34m"; // Входящие ответы (<--)
    static const auto red     = "\033[1;31m"; // Ошибки и критические сбои
    static const auto reset   = "\033[0m";    // Обязательный сброс в конце строки
}

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
        case OperationEnums::SEPARATOR_OP_TYPE:     return QString::fromUtf8("Разделитель операций: недопустимая операция!");
        case OperationEnums::NEGATION:      return QString::fromUtf8("Смена знака");
        case OperationEnums::CLEAR_ALL:     return QString::fromUtf8("Сброс");
        case OperationEnums::MAX_INT_VALUE: return QString::fromUtf8("Наибольшее целое число");
        case OperationEnums::RANDINT:       return QString::fromUtf8("Случайное число");
        case OperationEnums::RANDINT64:     return QString::fromUtf8("Случайное 64-битное число");
        case OperationEnums::FACTOR:        return QString::fromUtf8("Разложить на простые множители");
        default:                            return QString::fromUtf8("Неизвестная операция");
    }
};

AppCore::AppCore(QObject *parent)
    : QObject(parent)
    , m_requests(tp::BUFFER_SIZE)
    , m_results(tp::BUFFER_SIZE)
{
    // 1. Инициализация наблюдателей (теперь QApplication точно существует)
    m_reqObs = std::make_unique<ro::RequestObserver>(
        m_requests, &m_reqUsed, &m_reqFree, &m_controller
        );

    m_resObs = std::make_unique<ro::ResultObserver>(
        m_results, &m_resUsed, &m_resFree
        );

    // 2. Соединения сигналов
    connect(&m_controller, &Controller::handle_results, this, &AppCore::handle_results);
    connect(m_resObs.get(), &ro::ResultObserver::handleResults, this, &AppCore::handle_results_queue);

    // 3. Безопасный запуск потоков через очередь событий
    QMetaObject::invokeMethod(this, [this](){
        m_resObs->start();
        m_reqObs->start();
        qDebug() << "Observers and Threads started successfully.";
    }, Qt::QueuedConnection);

    qDebug() << "AppCore initialized. Welcome!";
}

AppCore::~AppCore()
{
    qDebug() << "~AppCore: Shutting down threads...";

    // Сигнализируем контроллеру о немедленной остановке вычислений
    emit m_controller.stop_calculation();

    // Останавливаем наблюдателей
    if (m_reqObs) m_reqObs->finish();
    if (m_resObs) m_resObs->finish();

    // Дожидаемся физического завершения
    if (m_reqObs) m_reqObs->wait();
    if (m_resObs) m_resObs->wait();

    // Завершаем потоки внутри контроллера
    m_controller.quit();

    qDebug() << "~AppCore: Shutdown complete.";
}

void AppCore::Reset() {
    mRegister[0] = dec_n::Decimal{};
    mRegister[1] = dec_n::Decimal{};
    mPreviousValue = dec_n::Decimal{};
    mCurrentOperation = OperationEnums::CLEAR_ALL;
    mState = StateEnums::RESETTED;
}

void AppCore::DoWork(dec_n::Decimal value, int operation)
{
    // 1. Предварительная настройка регистров (Логика состояний)
    // Рандом не зависит от текущих значений в регистрах
    if (operation != OperationEnums::RANDINT && operation != OperationEnums::RANDINT64) {
        switch (mState) {
        case StateEnums::OP_LOOP:
        case StateEnums::OP_TO_EQUAL:
            mRegister[0] = value;
            break;
        case StateEnums::EQUAL_TO_OP:
            mRegister[1] = value;
            return; // Запрос не отправляется, только обновляем регистр
        case StateEnums::EQUALS_LOOP:
        default:
            mRegister[1] = value;
            break;
        }
    }

    // 2. Инкремент индекса запроса
    mRequestIdx = (mRequestIdx + 1) % tp::BUFFER_SIZE;

    // СТАРТ ТАЙМЕРА для конкретного ID
    mTimers[mRequestIdx].start();

    // 3. Визуальное логирование (в едином стиле с ответом)
    {
        const QString timestamp = QTime::currentTime().toString("HH:mm:ss.zzz");
        auto debug = qDebug().noquote().nospace();

        // Метаданные: [Время] [ID] (Серый)
        debug << modifiers::gray << timestamp << " [ID: " << mRequestIdx << "] " << modifiers::reset;

        // Заголовок: --> [Запрос]: Название (Зеленый)
        debug << modifiers::green << "--> [Запрос]: " << description(operation) << " " << modifiers::reset;

        // Аргументы операции (если это не рандом)
        if (operation != OperationEnums::RANDINT && operation != OperationEnums::RANDINT64) {
            auto sv1 = mRegister[1].ValueAsStringView();
            if (operation == OperationEnums::FACTOR) { // Для факторизации нет смысла выводить дробную часть.
                // Ищем позицию первого вхождения точки или запятой
                size_t pos = sv1.find_first_of(".,");

                // Если разделитель найден, обрезаем строку до этой позиции
                if (pos != std::string_view::npos) {
                    sv1 = sv1.substr(0, pos);
                }
            }
            debug << modifiers::gray << "x: " << modifiers::reset
                  << QString::fromUtf8(sv1.data(), static_cast<int>(sv1.size()));

            // Если операция бинарная (ниже сепаратора), выводим y
            if (operation < OperationEnums::SEPARATOR_OP_TYPE) {
                auto sv0 = mRegister[0].ValueAsStringView();
                debug << modifiers::gray << " y: " << modifiers::reset
                      << QString::fromUtf8(sv0.data(), static_cast<int>(sv0.size()));
            }
        }
    }

    // 4. Формирование вектора и отправка в очередь
    QVector<dec_n::Decimal> request_vector { mRegister[1], mRegister[0] };

    m_reqFree.acquire();
    m_requests[mRequestIdx] = { operation, request_vector };
    m_reqUsed.release();
}

void AppCore::process(int requested_operation, QString input_value)
{
    // --- 1. Вспомогательные лямбды ---
    auto getStr = [](const dec_n::Decimal& v) {
        auto sv = v.ValueAsStringView();
        return QString::fromUtf8(sv.data(), static_cast<int>(sv.size()));
    };

    auto updateUIWithError = [this](int err) {
        emit clearInputField();
        emit clearCurrentOperation();
        emit showTempResult(err_description(err), false);
        Reset();
        qDebug().noquote() << modifiers::red << "!!! [Ошибка]: " << err_description(err) << modifiers::reset;
    };

    // --- 2. Специальные операции (CLEAR / IO) ---
    if (requested_operation == OperationEnums::CLEAR_ALL) {
        qDebug().noquote() << modifiers::gray << "Операция: " << modifiers::reset << description(requested_operation);
        emit clearTempResult();
        emit clearCurrentOperation();
        if (mCurrentOperation != OperationEnums::FACTOR) emit clearInputField();
        emit m_controller.stop_calculation();
        Reset();
        return;
    }

    if (requested_operation > OperationEnums::SEPARATOR_IO) {
        bignum::u128::U128 value = (requested_operation == OperationEnums::MAX_INT_VALUE)
        ? bignum::u128::U128::max() : bignum::u128::U128{0};
        if (mState == StateEnums::RESETTED) emit clearTempResult();
        emit setInput(QString::fromStdString(value.toString()));
        return;
    }

    // --- 3. РАНДОМ (Вне очереди парсинга) ---
    if (requested_operation == OperationEnums::RANDINT || requested_operation == OperationEnums::RANDINT64) {
        qDebug().noquote() << modifiers::green << "--> [Запрос]: " << description(requested_operation) << modifiers::reset;
        dec_n::Decimal dummy;
        DoWork(dummy, requested_operation);
        return;
    }

    // --- 4. Парсинг числа ---
    input_value.remove(QRegularExpression{"\\s"});
    dec_n::Decimal val;
    val.SetStringRepresentation(input_value.toStdString());

    if (val.IsOverflowed()) {
        updateUIWithError(Errors::NOT_FINITE);
        return;
    }

    const bool is_nan = val.IsNotANumber();
    if (mState == StateEnums::RESETTED && is_nan) return;

    // --- 5. ФАКТОРИЗАЦИЯ ---
    if (requested_operation == OperationEnums::FACTOR) {
        if (mState == StateEnums::EQUAL_TO_OP || mState == StateEnums::OP_LOOP || is_nan) return;
        emit setEnableFactorButton(false);
        mCurrentOperation = OperationEnums::FACTOR;
        emit showCurrentOperation(description(requested_operation));
        DoWork(val, requested_operation);
        return;
    }

    // --- 6. Обработка NaN (Смена операции при пустом вводе) ---
    if (is_nan) {
        if (requested_operation == mCurrentOperation) {
            qDebug().noquote() << modifiers::red << "Повтор операции." << modifiers::reset;
            return;
        }
        if (requested_operation == OperationEnums::EQUAL && mCurrentOperation >= 0) return;

        if (requested_operation >= 0 && requested_operation < OperationEnums::SEPARATOR_OP_TYPE) {
            mCurrentOperation = requested_operation;
            emit showCurrentOperation(description(requested_operation));
        }
        return;
    }

    // --- 7. Унарные операции ---
    const bool current_val_is_the_same = (val == mPreviousValue);
    mPreviousValue = val;
    const bool is_immediate_state = (mState == StateEnums::EQUAL_TO_OP || mState == StateEnums::OP_LOOP);

    if (requested_operation == OperationEnums::NEGATION) {
        val = -val;
        emit setInput(getStr(val));
        return;
    }

    if (requested_operation == OperationEnums::SQRT || requested_operation == OperationEnums::SQR || requested_operation == OperationEnums::RECIPROC) {
        if (is_immediate_state) {
            if (requested_operation == OperationEnums::RECIPROC && val.IsZero()) { updateUIWithError(Errors::ZERO_DIVISION); return; }
            if (requested_operation == OperationEnums::SQRT) { bool exact; val = dec_n::Sqrt(val, exact); }
            else if (requested_operation == OperationEnums::SQR) { val = val * val; }
            else { dec_n::Decimal one; one.SetDecimal(1, 0); val = one / val; }

            if (val.IsOverflowed()) { updateUIWithError(Errors::NOT_FINITE); return; }
            emit setInput(getStr(val));
            return;
        } else {
            mCurrentOperation = requested_operation;
            emit showCurrentOperation(description(requested_operation));
        }
    }

    // --- 8. Кнопка EQUAL (Равно) ---
    if (requested_operation == OperationEnums::EQUAL) {
        if (mCurrentOperation == OperationEnums::FACTOR) return;
        if (mCurrentOperation < 0 && !current_val_is_the_same) {
            emit clearTempResult();
            emit setInput(getStr(val));
            mCurrentOperation = requested_operation;
            return;
        }
        if (mCurrentOperation == requested_operation && current_val_is_the_same) return;
    }

    // --- 9. Управление состояниями (FSM) ---
    const bool is_binary = (requested_operation >= 0 && requested_operation < OperationEnums::SEPARATOR_OP_TYPE);
    const bool is_unary_delayed = (requested_operation > OperationEnums::SEPARATOR_OP_TYPE && requested_operation < OperationEnums::SEPARATOR_IO);

    qDebug().noquote() << modifiers::gray << "Операция: " << modifiers::reset << description(requested_operation);

    // Сценарий: цепочка (нажатие '+' после ввода второго операнда для предыдущей операции)
    if (is_binary && is_immediate_state) {
        emit showCurrentOperation(description(requested_operation)); // Визуализация новой операции
        mState = StateEnums::OP_LOOP;
        emit clearInputField();
        emit showTempResult(getStr(val), true);
        DoWork(val, mCurrentOperation); // Выполняем накопленную операцию
        mCurrentOperation = requested_operation;
        return;
    }

    // Переходы для EQUAL и отложенных унарных
    if (requested_operation == OperationEnums::EQUAL || is_unary_delayed) {
        mState = is_immediate_state ? StateEnums::OP_TO_EQUAL : StateEnums::EQUALS_LOOP;
    }

    // Настройка для бинарных
    if (is_binary) {
        emit showCurrentOperation(description(requested_operation)); // Визуализация

        if (mState == StateEnums::EQUALS_LOOP || mState == StateEnums::OP_TO_EQUAL) {
            mState = StateEnums::EQUAL_TO_OP;
        }
        if (mState == StateEnums::RESETTED) {
            mRegister[1] = val; // Восстановлен индекс для базового операнда
            mState = StateEnums::EQUAL_TO_OP;
        }
        mCurrentOperation = requested_operation;
        emit clearInputField();
        emit showTempResult(getStr(val), true);
    }

    // Финальный запуск (DoWork сама разберется с регистрами на основе mState)
    DoWork(val, mCurrentOperation);
}

void AppCore::handle_results(int err, int operation, bool exact_sqrt, QVector<dec_n::Decimal> res)
{
    // 1. Обновляем регистр (Guard Clause для читаемости)
    if (operation != OperationEnums::RANDINT && operation != OperationEnums::RANDINT64 && !res.isEmpty()) {
        mRegister[1] = res.first();
    }

    // 2. Управление индексами и семафорами
    m_resFree.acquire();

    mResultIdx = (mResultIdx + 1) % tp::BUFFER_SIZE;

    // Используем std::move, если QVector больше не нужен в этой области видимости,
    // чтобы не копировать глубокие данные Decimal.
    m_results[mResultIdx] = { err, operation, exact_sqrt, std::move(res) };

    m_resUsed.release();
}

void AppCore::handle_results_queue(int err, int operation, bool exact_sqrt, QVector<dec_n::Decimal> res, int id)
{
    // Подготовка метаданных (Время и ID) серым цветом
    const QString timestamp = QTime::currentTime().toString("HH:mm:ss.zzz");
    const QString meta = QString("%1%2 [ID: %3]%4 ")
                             .arg(modifiers::gray, timestamp, QString::number(id), modifiers::reset);

    // 1. ОБРАБОТКА ОШИБОК
    if (err != Errors::NO_ERRORS) {
        qDebug().noquote().nospace()
        << meta << modifiers::red << "!!! [Ошибка]: " << err_description(err) << modifiers::reset;

        emit clearInputField();
        emit clearCurrentOperation();
        emit showTempResult(err_description(err), false);
        Reset();
        return;
    }

    const bool state_is_the_equal = (mState == StateEnums::EQUALS_LOOP) || (mState == StateEnums::OP_TO_EQUAL);

    // 2. КВАДРАТНЫЙ КОРЕНЬ (обработка мета-информации в UI)
    if (operation == OperationEnums::SQRT) {
        QString suffix = exact_sqrt ? QString::fromUtf8(": точно.") : QString::fromUtf8(": приближенно.");
        emit showCurrentOperation(description(OperationEnums::SQRT) + suffix);
    }

    if (operation == OperationEnums::FACTOR) {
        // Вычисляем прошедшее время
        qint64 nanoseconds = mTimers[id].nsecsElapsed();
        double milliseconds = nanoseconds / 1000000.0; // Конвертируем в мс

        QStringList factors;
        for (int i = 0; i + 1 < res.size(); i += 2) {
            QString prime = QString::fromStdString(res[i].IntegerPart().toString());
            QString power = QString::fromStdString(res[i + 1].IntegerPart().unsigned_part().toString());
            factors << QString("%1^%2").arg(prime, power);
        }

        // Вывод с бенчмарком (серым цветом)
        qDebug().noquote().nospace()
            << meta << modifiers::blue << "<-- [Факторы]: {" << factors.join("; ") << "} "
            << modifiers::gray << "[" << QString::number(milliseconds, 'f', 3) << " ms]"
            << modifiers::reset;

        mCurrentOperation = OperationEnums::CLEAR_ALL;
        mState = StateEnums::RESETTED;
        emit setEnableFactorButton(true);
    }
    // 4. РАНДОМ И ОСТАЛЬНЫЕ ОПЕРАЦИИ
    else {
        auto res_sv = res[0].ValueAsStringView();
        QString res_qs = QString::fromUtf8(res_sv.data(), static_cast<int>(res_sv.size()));

        if (operation == OperationEnums::RANDINT || operation == OperationEnums::RANDINT64) {
            emit setInput(res_qs);
        }
        else {
            if (state_is_the_equal || mState == StateEnums::RESETTED) {
                emit setInput(res_qs);
                emit clearTempResult();
            } else {
                auto reg_sv = mRegister[1].ValueAsStringView();
                emit showTempResult(QString::fromUtf8(reg_sv.data(), static_cast<int>(reg_sv.size())), true);
            }
        }

        // Универсальный вывод результата в консоль
        qDebug().noquote().nospace()
            << meta << modifiers::blue << "<-- [Ответ]: " << res_qs << modifiers::reset;
    }
}

void AppCore::change_decimal_width(int width, bool quiet)
{
    const bool is_changed = dec_n::Decimal::SetWidth(width);
    emit m_controller.sync_decimal_width(width);
    if (is_changed) {
        Reset();
        emit clearTempResult();
        emit clearCurrentOperation();
        emit clearInputField();
        emit changeDecimalWidth(dec_n::Decimal::GetWidth());
        if (!quiet)
            qDebug().noquote() << modifiers::red << QString::fromUtf8("Изменено количество знаков после запятой: ") <<
                            dec_n::Decimal::GetWidth() << modifiers::reset;
        QSettings settings("MyHome", "DecimalCalculator");
        settings.setValue("DecimalWidth", width);
    }
}
