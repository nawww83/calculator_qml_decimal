#include "AppCore.h"

#include <QSemaphore>
#include "observers.h"

#include <QDebug>

static constexpr auto green_modifier = "\033[32m";
static constexpr auto blue_modifier = "\033[34m";
static constexpr auto red_modifier = "\033[31m";
static constexpr auto esc_colorization = "\033[0m";


QSemaphore requests_free(_tp::buff_size);
QSemaphore requests_used(0);
QSemaphore results_free(_tp::buff_size);
QSemaphore results_used(0);

QVector<_tp::Request> requests(_tp::buff_size);
QVector<_tp::Result> results(_tp::buff_size);

Controller controller;

ro::RequestObserver reqObs(requests,
                           &requests_used, &requests_free,
                           &controller);
ro::ResultObserver resObs(results,
                          &results_used, &results_free);

auto err_description = [](int err) {
    switch (err) {
    case Errors::ZERO_DIVISION: return "zero division";
    case Errors::UNKNOW_OP: return "unknow operation";
    case Errors::NOT_FINITE: return "overflow";
    default: return "no errors";
    }
};

// State machine:
// input 1st operand, input operation, input 2nd operand:
//   - input "="
//       -- input operation
//       -- input 1st operand
//   - input operation
//      the operation the same OR the operation is another one
//       -- input 2nd operand
//       -- input "="
// All operations don't have priority! Do it directly, as is
auto description = [](int op) {
    switch (op) {
    case OperationEnums::ADD: return "addition";
    case OperationEnums::SUB: return "subtraction";
    case OperationEnums::MULT: return "multiplication";
    case OperationEnums::DIV: return "division";
    case OperationEnums::EQUAL: return "equal";
    case OperationEnums::NEGATION: return "negation";
    case OperationEnums::CLEAR_ALL: return "clear all";
    default: return "unknown";
    }
};


AppCore::AppCore(QObject *parent) : QObject(parent)
{
    _reset();

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

void AppCore::_reset() {
    _reg[0] = _reg[1] = {};
    _prev_value = {};
    _op = OperationEnums::CLEAR_ALL;
    _state = StateEnums::RESETTED;
}

void AppCore::_do(dec_n::Decimal<> val, int op) {
    auto push_request = [this, op]() {
        _i_req = (_i_req+1) % _tp::buff_size;
        std::string_view sv1 = _reg[1].value();
        std::string_view sv0 = _reg[0].value();
        qDebug().noquote() << green_modifier << " push request:"
                           << description(op) << "x:" << QString::fromStdString({sv1.data(), sv1.size()}).toUtf8()
                           << "y:" << QString::fromStdString({sv0.data(), sv0.size()}).toUtf8()
                 << "id:" << _i_req
                 << esc_colorization;
        requests_free.acquire();
        QVector<dec_n::Decimal<>> v {_reg[1], _reg[0]};
        requests[_i_req] = {op, v};
        requests_used.release();
    };
    switch (_state) {
    case StateEnums::EQUALS_LOOP:
        if (op == OperationEnums::EQUAL) {
            if (val.value() == "") {
                val = {};
            }
            _reg[1] = val;
        } else {
            _reg[1] = val; // if value was changed by user
            push_request();
        }
        break;
    case StateEnums::EQUAL_TO_OP:
        _reg[1] = val; // if value was changed by minus sign
        break;
    case StateEnums::OP_LOOP:
        if (val.value() == "") {
            val = {};
        }
        _reg[0] = val;
        push_request();
        break;
    case StateEnums::OP_TO_EQUAL: // "=" is pressed
            _reg[0] = val;
            push_request();
//        }
        break;
    default: ;
    }
}

void AppCore::cppSlot(int op, QString v)
{
    if (op == OperationEnums::CLEAR_ALL) {
        qDebug() << "Operation:" << description(op);
        _reset();
        emit clearTempResult();
        emit clearInputField();
        return;
    }
    dec_n::Decimal<> val {};
    v = v.remove(QRegExp("(\\s)"));
    auto str = v.toStdString();
    val.set_str(str);
    //
    if (val.is_overflow()) {
        int err = Errors::NOT_FINITE;
        emit clearInputField();
        emit showTempResult(QString("%1").arg(err_description(err)), false);
        _reset();
        qDebug() << red_modifier <<  "error:" << err_description(err) << esc_colorization;
        return;
    }
    //
    const bool state_is_op = (_state == StateEnums::EQUAL_TO_OP) or (_state == StateEnums::OP_LOOP);
    const bool state_is_eq = (_state == StateEnums::EQUALS_LOOP) or (_state == StateEnums::OP_TO_EQUAL);

    const bool current_is_math_op = (op >= 0);

    std::string_view sv = val.value();
    const bool current_val_is_the_same = (sv == _prev_value.value());
    _prev_value = val;

    const bool is_nan = (sv == "");
    if ((_state == StateEnums::RESETTED) && (is_nan)) { // no feedback if nothing to do
        return;
    }
    //
    if (is_nan) {
        if (op == _op) {
            qDebug() << red_modifier << "Operation is repeated!" << esc_colorization;
            return;
        }
        if ((op == OperationEnums::EQUAL) && (_op >= 0)) { // to save math operation
            qDebug() << "Operation:" << description(op);
            return;
        }
        {
            qDebug() << "Operation:" << description(op);
            _op = op;
            return;
        }
    }
    if (op == OperationEnums::NEGATION) {
        qDebug() << "Operation:" << description(op);
        val = dec_n::Decimal<>{} - val;
        std::string_view sv = val.value();
        emit setInput(QString::fromStdString({sv.data(), sv.size()}));
        return;
    }
    if ((op == OperationEnums::EQUAL) && (_op < 0) && (!current_val_is_the_same)) {
        qDebug() << "Operation:" << description(op);
        std::string_view sv = val.value();
        emit clearTempResult();
        emit setInput(QString::fromStdString({sv.data(), sv.size()}));
        _op = op;
        return;
    }
    if ((op == OperationEnums::EQUAL) && (_op == op) && (current_val_is_the_same)) {
        qDebug() << red_modifier << "Operation is repeated!" << esc_colorization;
        return;
    }

    if (current_is_math_op and state_is_op) {
        if (! is_nan) {
            if ((op != _op) && (op != OperationEnums::NEGATION)) { // operation is modified
                qDebug() << "Operation:" << description(op);
                _state = StateEnums::OP_LOOP;
                emit clearInputField();
                std::string_view sv = val.value();
                emit showTempResult(QString::fromStdString({sv.data(), sv.size()}), true);
                _do(val, _op); // do old operation, store result to reg[1]
                _op = op;
                return;
            }
        }
    }

    qDebug() << "Operation:" << description(op);

    if (! current_is_math_op) {
        if (state_is_op) {
            _state = StateEnums::OP_TO_EQUAL;
        } else {
            _state = StateEnums::EQUALS_LOOP;
        }
    } else {
        if (state_is_eq) {
            _state = StateEnums::EQUAL_TO_OP;
        } else {
            if (_state == StateEnums::RESETTED) {
                _reg[1] = val;
                _state = StateEnums::EQUAL_TO_OP;
            } else {
                _state = StateEnums::OP_LOOP;
            }
        }
        _op = op;
        {
            emit clearInputField();
            std::string_view sv = val.value();
            emit showTempResult(QString::fromStdString({sv.data(), sv.size()}), true);
        }
        // clear it for convenient operand input
    }

    _do(val, _op); // use built-in function or library
}

void AppCore::handle_results(int err, QVector<dec_n::Decimal<>> res)
{
    _reg[1] = res.first();

    auto push_result = [this, err, res]() {
        results_free.acquire();
        _i_res = (_i_res+1) % _tp::buff_size;
        results[_i_res] = {err, res.first()};
        results_used.release();
    };

    push_result();
}

void AppCore::handle_results_queue(int err, QVector<dec_n::Decimal<>> res, int id)
{
    if (err == Errors::NO_ERRORS) {
        const bool state_is_eq =
                (_state == StateEnums::EQUALS_LOOP) or
                (_state == StateEnums::OP_TO_EQUAL);
        if (state_is_eq) { // display result if "=" was pressed
            std::string_view sv = res[0].value().data();
            emit setInput(QString::fromStdString({sv.data(), sv.size()}));
            emit clearTempResult();
        } else {
            std::string_view sv = _reg[1].value();
            emit showTempResult(QString::fromStdString({sv.data(), sv.size()}), true);
        }
        {
            std::string_view sv = res[0].value();
            qDebug().noquote() << blue_modifier << "pop result: id:" << id << "res:" << QString::fromStdString({sv.data(), sv.size()}) << esc_colorization;
        }
    } else {
        emit clearInputField();
        emit showTempResult(QString("%1").arg(err_description(err)), false);
        _reset();
        qDebug() << red_modifier <<  "error:" << err_description(err) << esc_colorization;
    }
}
