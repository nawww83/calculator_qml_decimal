#include "AppCore.h"

#include <QRegularExpression>
#include <QTime>
#include <QVariantMap>

#include "u128.hpp"
#include "i128.hpp"

#include <QSettings>
#include <memory>
#include <QTimer>

/**
 * @brief Возвращает описание ошибки по коду ошибки.
 */
static constexpr auto err_description = [](int error_code) -> QString {
    switch (error_code) {
        case Errors::ZERO_DIVISION: return QString::fromUtf8("Деление на ноль");
        case Errors::UNKNOWN_OP:
            return QString::fromUtf8("Неизвестная операция");
        case Errors::NOT_FINITE:    return QString::fromUtf8("Переполнение");
        case Errors::NOT_A_NUMBER:    return QString::fromUtf8("Не число");
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
        case OperationEnums::POW:           return QString::fromUtf8("Возведение в степень");
        case OperationEnums::EQUAL:         return QString::fromUtf8("Равно");
        case OperationEnums::SQRT:          return QString::fromUtf8("Квадратный корень");
        case OperationEnums::SQR:           return QString::fromUtf8("Квадрат числа");
        case OperationEnums::RECIPROC:      return QString::fromUtf8("Обратное число");
        case OperationEnums::LN:            return QString::fromUtf8("Натуральный логарифм");
        case OperationEnums::EXP:            return QString::fromUtf8("Экспонента");
        case OperationEnums::ROUND:         return QString::fromUtf8("Округление к целому");
        case OperationEnums::FIX_NINES:     return QString::fromUtf8("Коррекция хвоста девяток");
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
    // Инициализация наблюдателей (теперь QApplication точно существует)
    m_reqObs = std::make_unique<ro::RequestObserver>(
        m_requests, &m_reqUsed, &m_reqFree, &m_controller
        );

    m_resObs = std::make_unique<ro::ResultObserver>(
        m_results, &m_resUsed, &m_resFree
        );

    // Соединения сигналов
    connect(&m_controller, &Controller::handle_results, this, &AppCore::handle_results);
    connect(m_resObs.get(), &ro::ResultObserver::handleResults, this, &AppCore::handle_results_queue);

    // Безопасный запуск потоков через очередь событий
    QMetaObject::invokeMethod(this, [this](){
        m_resObs->start();
        m_reqObs->start();
    }, Qt::QueuedConnection);

    Reset();
}

AppCore::~AppCore()
{
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
}

void AppCore::Reset() {
    mRegister[0] = dec_n::Decimal{};
    mRegister[1] = dec_n::Decimal{};
    mPreviousValue = dec_n::Decimal{};
    mLastShownInput.clear();
    mDisplayValue = dec_n::Decimal{};
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

    // 3. Запись запроса в историю для панели QML
    {
        QString detail;
        if (operation != OperationEnums::RANDINT && operation != OperationEnums::RANDINT64) {
            auto sv1 = mRegister[1].ValueAsStringView();
            if (operation == OperationEnums::FACTOR) { // Для факторизации нет смысла выводить дробную часть.
                size_t pos = sv1.find_first_of(".,");
                if (pos != std::string_view::npos) {
                    sv1 = sv1.substr(0, pos);
                }
            }
            detail = QString::fromUtf8("x: ") + QString::fromUtf8(sv1.data(), static_cast<int>(sv1.size()));

            // Если операция бинарная (ниже сепаратора), выводим y
            if (operation < OperationEnums::SEPARATOR_OP_TYPE) {
                auto sv0 = mRegister[0].ValueAsStringView();
                detail += QString::fromUtf8("   y: ") + QString::fromUtf8(sv0.data(), static_cast<int>(sv0.size()));
            }
        }

        addHistoryEntry(QString::fromUtf8("request"),
                        QString::fromUtf8("--> [Запрос]: ") + description(operation),
                        detail, mRequestIdx);
    }

    // 4. Формирование вектора и отправка в очередь
    QVector<dec_n::Decimal> request_vector { mRegister[1], mRegister[0] };

    m_reqFree.acquire();
    m_requests[mRequestIdx] = { operation, request_vector };
    m_reqUsed.release();
}

void AppCore::process(int requested_operation, QString input_value)
{
    // --- Вспомогательные лямбды ---
    auto getStr = [this](const dec_n::Decimal& v) {
        return QString::fromStdString(v.ToFormattedString(m_userDisplayWidth));
    };

    // Показать результат в поле ввода и запомнить показанный текст:
    // пока он не изменен, следующие операции используют полное внутреннее значение.
    auto showResult = [this, &getStr](const dec_n::Decimal& v) {
        mDisplayValue = v;
        mLastShownInput = getStr(v);
        emit setInput(mLastShownInput);
    };

    // Очистить поле ввода вместе со сведениями о показанном результате.
    auto clearInput = [this]() {
        mLastShownInput.clear();
        mDisplayValue = dec_n::Decimal{};
        emit clearInputField();
    };

    auto updateUIWithError = [this, &clearInput](int err) {
        clearInput();
        emit clearCurrentOperation();
        emit showTempResult(err_description(err), false);
        Reset();
        addHistoryEntry(QString::fromUtf8("error"),
                        QString::fromUtf8("!!! [Ошибка]: ") + err_description(err));
    };

    // --- Специальные операции (CLEAR / IO) ---
    if (requested_operation == OperationEnums::CLEAR_ALL) {
        emit clearTempResult();
        emit clearCurrentOperation();
        if (mCurrentOperation != OperationEnums::FACTOR) clearInput();
        emit m_controller.stop_calculation();
        Reset();
        return;
    }

    if (requested_operation > OperationEnums::SEPARATOR_IO) {
        if (requested_operation == OperationEnums::MAX_INT_VALUE) {
            if (mState == StateEnums::RESETTED) emit clearTempResult();
            auto value = bignum::u128::U128::max();
            mLastShownInput.clear();
            mDisplayValue = dec_n::Decimal{};
            emit setInput(QString::fromStdString(value.toString()));
            return;
        }
        if (requested_operation == OperationEnums::RANDINT || requested_operation == OperationEnums::RANDINT64) {
            dec_n::Decimal dummy;
            DoWork(dummy, requested_operation);
            return;
        }
    }

    // --- Парсинг числа ---
    // Убираем разделители триад: любой пробельный символ Unicode (U+2004 U+2009 и т.п.).
    static auto space_regex = QRegularExpression{
        "\\s", QRegularExpression::UseUnicodePropertiesOption};
    input_value.remove(space_regex);
    dec_n::Decimal val;
    val.SetStringRepresentation(input_value.toStdString());

    if (val.IsOverflowed()) {
        updateUIWithError(Errors::NOT_FINITE);
        return;
    }
    if (val.IsNotANumber()) {
        updateUIWithError(Errors::NOT_A_NUMBER);
        return;
    }

    const bool is_nan = val.IsNotANumber();
    if (mState == StateEnums::RESETTED && is_nan) return;

    // --- ФАКТОРИЗАЦИЯ ---
    if (requested_operation == OperationEnums::FACTOR) {
        if (mState == StateEnums::EQUAL_TO_OP || mState == StateEnums::OP_LOOP || is_nan) return;

        // Факторизуется ровно то число, которое видно на дисплее.
        // Worker берет x из mRegister[1], а DoWork в состоянии OP_TO_EQUAL кладет
        // разобранное значение только в mRegister[0]. Без этой синхронизации
        // факторизовалось бы предыдущее число: после "8 + 8 =" в регистре
        // оставалось 16, даже если на дисплее M128/кси уже показали другое.
        mRegister[1] = val;

        emit setEnableFactorButton(false);
        mCurrentOperation = OperationEnums::FACTOR;
        emit showCurrentOperation(description(requested_operation));
        emit clearTempResult(); // Убираем разложение предыдущего числа.
        DoWork(val, requested_operation);
        return;
    }

    // --- Обработка NaN (Смена операции при пустом вводе) ---
    if (is_nan) {
        if (requested_operation == mCurrentOperation) {
            return;
        }
        if (requested_operation == OperationEnums::EQUAL && mCurrentOperation >= 0) return;

        if (requested_operation >= 0 && requested_operation < OperationEnums::SEPARATOR_OP_TYPE) {
            mCurrentOperation = requested_operation;
            emit showCurrentOperation(description(requested_operation));
        }
        return;
    }

    // --- Унарные операции ---
    // Если поле ввода не редактировалось после показа результата, работаем
    // с полным внутренним значением (18 знаков), а не с округленным текстом
    // дисплея. Иначе накапливалась бы погрешность, например:
    // 1/7 = 0,1429 -> 1/0,1429 = 6,9979 вместо 7.
    const bool input_is_untouched_result =
        !mLastShownInput.isEmpty() &&
        input_value == mLastShownInput &&
        !mDisplayValue.IsNotANumber() &&
        !mDisplayValue.IsOverflowed();
    if (input_is_untouched_result) {
        val = mDisplayValue;
    }

    const bool current_val_is_the_same = (val == mPreviousValue);
    mPreviousValue = val;
    const bool is_immediate_state = (mState == StateEnums::EQUAL_TO_OP || mState == StateEnums::OP_LOOP);

    if (requested_operation == OperationEnums::NEGATION) {
        val = -val;
        showResult(val);
        return;
    }

    if (requested_operation > OperationEnums::SEPARATOR_OP_TYPE &&
        requested_operation < OperationEnums::SEPARATOR_IO)
    {
        if (is_immediate_state) {
            if (requested_operation == OperationEnums::RECIPROC && val.IsZero()) {
                updateUIWithError(Errors::ZERO_DIVISION);
                return;
            }

            if (requested_operation == OperationEnums::LN) {
                val = dec_n::Ln(val);
            }
            else if (requested_operation == OperationEnums::EXP) {
                val = dec_n::Exp(val);
            }
            else if (requested_operation == OperationEnums::SQRT) {
                val = dec_n::Sqrt(val);
            }
            else if (requested_operation == OperationEnums::SQR) {
                val = val * val;
            }
            else if (requested_operation == OperationEnums::ROUND) {
                val = val.Round();
            }
            else if (requested_operation == OperationEnums::FIX_NINES) {
                val = val.FixNines();
            }
            else { // RECIPROCAL
                dec_n::Decimal one;
                one.SetDecimal(1, 0);
                val = one / val;
            }

            if (val.IsOverflowed()) {
                updateUIWithError(Errors::NOT_FINITE);
                return;
            }
            if (val.IsNotANumber()) {
                updateUIWithError(Errors::NOT_A_NUMBER);
                return;
            }
            // Результат унарной операции становится текущим значением дисплея:
            // следующая операция должна видеть все 18 знаков.
            showResult(val);
            return;
        } else {
            mCurrentOperation = requested_operation;
            emit showCurrentOperation(description(requested_operation));
        }
    }
    // --- Кнопка EQUAL (Равно) ---
    if (requested_operation == OperationEnums::EQUAL) {
        if (mCurrentOperation == OperationEnums::FACTOR) return;
        if (mCurrentOperation < 0 && !current_val_is_the_same) {
            emit clearTempResult();
            showResult(val);
            mCurrentOperation = requested_operation;
            return;
        }
        if (mCurrentOperation == requested_operation && current_val_is_the_same) return;
    }

    // --- Управление состояниями (FSM) ---
    const bool is_binary = (requested_operation >= 0 && requested_operation < OperationEnums::SEPARATOR_OP_TYPE);
    const bool is_unary_delayed = (requested_operation > OperationEnums::SEPARATOR_OP_TYPE && requested_operation < OperationEnums::SEPARATOR_IO);

    // Сценарий: цепочка (нажатие '+' после ввода второго операнда для предыдущей операции)
    if (is_binary && is_immediate_state) {
        emit showCurrentOperation(description(requested_operation)); // Визуализация новой операции
        mState = StateEnums::OP_LOOP;
        clearInput();
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
        clearInput();
        emit showTempResult(getStr(val), true);
    }

    // Финальный запуск (DoWork сама разберется с регистрами на основе mState)
    DoWork(val, mCurrentOperation);
}

void AppCore::handle_results(int err, int operation, QVector<dec_n::Decimal> res)
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
    m_results[mResultIdx] = { err, operation, std::move(res) };

    m_resUsed.release();
}

void AppCore::handle_results_queue(int err, int operation, QVector<dec_n::Decimal> res, int id)
{
    // 1. ОБРАБОТКА ОШИБОК
    if (err != Errors::NO_ERRORS) {
        addHistoryEntry(QString::fromUtf8("error"),
                        QString::fromUtf8("!!! [Ошибка]: ") + err_description(err),
                        QString(), id);

        emit clearInputField();
        emit clearCurrentOperation();
        emit showTempResult(err_description(err), false);
        Reset();
        return;
    }

    const bool state_is_the_equal = (mState == StateEnums::EQUALS_LOOP) || (mState == StateEnums::OP_TO_EQUAL);

    // 2. КВАДРАТНЫЙ КОРЕНЬ
    if (operation == OperationEnums::SQRT) {
        emit showCurrentOperation(description(OperationEnums::SQRT));
    }

    if (operation == OperationEnums::FACTOR) {
        // Время вычисления показывается отдельным полем записи истории.
        qint64 nanoseconds = mTimers[id].nsecsElapsed();
        double milliseconds = nanoseconds / 1000000.0; // Конвертируем в мс
        const QString duration = QString::number(milliseconds, 'f', 3)
                                 + QString::fromUtf8(" ms");

        // Множители в привычной математической записи: степень 1 не пишем,
        // множители разделяем знаком умножения.
        QStringList factors;
        factors.reserve(res.size() / 2);
        for (int i = 0; i + 1 < res.size(); i += 2) {
            const QString prime = QString::fromStdString(res[i].IntegerPart().toString());
            const QString power =
                QString::fromStdString(res[i + 1].IntegerPart().unsigned_part().toString());
            factors << (power == QLatin1String("1")
                            ? prime
                            : QString("%1^%2").arg(prime, power));
        }

        // Ответ оформляется в том же стиле, что и ответы остальных операций.
        // Для простого числа разложение совпадает с самим числом, поэтому
        // поясняем результат, иначе он выглядит как эхо запроса.
        QString answer = factors.isEmpty() ? QString::fromUtf8("—")
                                           : factors.join(QString::fromUtf8(" · "));
        if (res.size() == 2) {
            const auto only_factor = res[0].IntegerPart().unsigned_part();
            const auto only_power = res[1].IntegerPart().unsigned_part();
            if (only_factor <= 1)
                answer += QString::fromUtf8(" — не раскладывается");
            else if (only_power == 1)
                answer += QString::fromUtf8(" — простое число");
        }

        addHistoryEntry(QString::fromUtf8("response"),
                        QString::fromUtf8("<-- [Ответ]: ") + answer,
                        QString(), id, duration);

        // Дублируем разложение в нижней строке ЖК: само поле ввода после
        // факторизации не меняется, иначе результат виден только в журнале.
        emit showFactorResult(QString::fromUtf8("= ") + answer);

        mCurrentOperation = OperationEnums::CLEAR_ALL;
        mState = StateEnums::RESETTED;
        emit setEnableFactorButton(true);
    }
    // 4. РАНДОМ И ОСТАЛЬНЫЕ ОПЕРАЦИИ
    else {
        QString res_qs = QString::fromStdString(res[0].ToFormattedString(m_userDisplayWidth));
        static auto dot_comma_regex = QRegularExpression("[.,]");
        if (operation == OperationEnums::RANDINT || operation == OperationEnums::RANDINT64) {
            int pos = res_qs.indexOf(dot_comma_regex); // Это целочисленные операции.
            if (pos != -1) {
                res_qs.truncate(pos); // Убираем состоящую из нулей дробную часть.
            }
            // Случайное число не обновляет mRegister[1], но mDisplayValue
            // хранит его точно, поэтому запоминаем показанный результат.
            mDisplayValue = res[0];
            mLastShownInput = res_qs;
            emit setInput(res_qs);
        }
        else {
            if (state_is_the_equal || mState == StateEnums::RESETTED) {
                mDisplayValue = res[0];
                mLastShownInput = res_qs;
                emit setInput(res_qs);
                emit clearTempResult();
            } else {
                QString reg_qs = QString::fromStdString(mRegister[1].ToFormattedString(m_userDisplayWidth));
                emit showTempResult(reg_qs, true);
            }
        }

        // Результат в историю операций.
        addHistoryEntry(QString::fromUtf8("response"),
                        QString::fromUtf8("<-- [Ответ]: ") + res_qs,
                        QString(), id);
    }
}

void AppCore::change_decimal_width(int width, bool quiet)
{
    Q_UNUSED(quiet);

    const int new_width = std::clamp(width, 0, 18);
    const bool is_changed = (m_userDisplayWidth != new_width);
    m_userDisplayWidth = new_width;

    // Всегда уведомляем UI
    emit changeDecimalWidth(m_userDisplayWidth);

    // Показ памяти зависит от текущей точности дисплея.
    if (m_memoryUsed) {
        m_memoryText = QString::fromStdString(mMemoryValue.ToFormattedString(m_userDisplayWidth));
        emit memoryChanged();
    }

    if (is_changed) {
        Reset();
        emit clearTempResult();
        emit clearCurrentOperation();
        emit clearInputField();
        QSettings settings("MyHome", "DecimalCalculator");
        settings.setValue("DecimalWidth", m_userDisplayWidth);
    }
}

void AppCore::addHistoryEntry(const QString &kind, const QString &headline,
                              const QString &detail, int id,
                              const QString &duration)
{
    QVariantMap entry;
    entry.insert(QStringLiteral("time"), QTime::currentTime().toString("HH:mm:ss.zzz"));
    entry.insert(QStringLiteral("id"), id);
    entry.insert(QStringLiteral("kind"), kind);
    entry.insert(QStringLiteral("headline"), headline);
    entry.insert(QStringLiteral("detail"), detail);
    entry.insert(QStringLiteral("duration"), duration);

    m_history.append(entry);
    while (m_history.size() > MAX_HISTORY_SIZE)
        m_history.removeFirst();

    emit historyChanged();
}

void AppCore::clearHistory()
{
    if (m_history.isEmpty())
        return;

    m_history.clear();
    emit historyChanged();
}

void AppCore::memoryStore(const QString &input_value)
{
    // Разделители триад могут быть любыми пробельными символами Unicode.
    static auto space_regex = QRegularExpression{
        "\\s", QRegularExpression::UseUnicodePropertiesOption};

    QString text = input_value;
    text.remove(space_regex);
    if (text.isEmpty())
        return;

    dec_n::Decimal value;
    value.SetStringRepresentation(text.toStdString());
    if (value.IsNotANumber() || value.IsOverflowed())
        return;

    // Пока дисплей не редактировался после результата, в поле ввода лежит
    // округленный текст, а полное значение хранится в mDisplayValue.
    if (!mLastShownInput.isEmpty() && text == mLastShownInput
        && !mDisplayValue.IsNotANumber() && !mDisplayValue.IsOverflowed()) {
        value = mDisplayValue;
    }

    mMemoryValue = value;
    m_memoryUsed = true;
    m_memoryText = QString::fromStdString(value.ToFormattedString(m_userDisplayWidth));
    emit memoryChanged();
}

void AppCore::memoryRecall()
{
    if (!m_memoryUsed)
        return;

    // Кладем на дисплей округленный текст, но запоминаем полное значение:
    // следующая операция возьмет его, а не показанные знаки.
    mDisplayValue = mMemoryValue;
    mLastShownInput = m_memoryText;
    emit setInput(m_memoryText);
}
