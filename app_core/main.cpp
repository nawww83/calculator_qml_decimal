#include <QGuiApplication>
#include <QQmlApplicationEngine>

#include "AppCore.h"
#include <QQmlContext>

using namespace dec_n;

int main(int argc, char *argv[])
{
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
#endif

    QGuiApplication app(argc, argv);

    qRegisterMetaType<QVector<dec_n::Decimal>>("QVector<dec_n::Decimal>");

    qmlRegisterUncreatableMetaObject(OperationEnums::staticMetaObject,
                                     "operation.enums",
                                     1,
                                     0,
                                     "OperationEnums",
                                     "Error: only enums");

    QQmlApplicationEngine engine;

    AppCore AppCore;
    if (! dec_n::is_two_complement()) {
        return 0;
    }

    qDebug() << "Testing...";
    bool all_is_ok = true;

    {
        const auto thousand = int_power(10, 3);
        all_is_ok &= thousand == u128::U128{1000, 0};
    }

    {
        u128::U128 result{};
        result.mLow = 1;
        result.mHigh = 0;
        result.mSign = true;
        all_is_ok &= result.is_negative();
    }

    {
        u128::U128 result{1, 0, true};
        all_is_ok &= result.is_negative();
    }

    {
        u128::U128 big_number {18'446'744'073'709'551'610ull, 0};
        u128::U128 result = big_number + u128::U128{6, 0};
        all_is_ok &= result.value().starts_with( "18446744073709551616" );
    }

    {
        u128::U128 big_number {0, 1};
        all_is_ok &= big_number.mod10() == 6;
    }

    {
        u128::U128 max_int_value = u128::get_max_value();
        all_is_ok &= max_int_value.value() == "340282366920938463463374607431768211455";
    }

    {
        u128::U128 max_int_value = u128::get_max_value();
        u128::U128 overflowed = max_int_value + u128::U128{1, 0};
        all_is_ok &= overflowed.is_overflow();
    }

    {
        u128::U128 max_int_value = u128::get_max_value();
        u128::U128 normal_value = max_int_value - u128::U128{1, 0};
        all_is_ok &= !normal_value.is_singular();
    }

    {
        u128::U128 x {1, 0};
        u128::U128 y {1, 0, true};
        const auto z = x * y;
        all_is_ok &= z.is_negative();
    }

    {
        u128::U128 x {1, 0, true};
        u128::U128 y {1, 0, true};
        const auto z = x * y;
        all_is_ok &= z.is_positive();
    }

    {
        u128::U128 x {1, 0};
        u128::U128 y {1, 0, true};
        const auto z = x / y;
        all_is_ok &= z.is_negative();
    }

    {
        u128::U128 x {1, 0, true};
        u128::U128 y {1, 0, true};
        const auto z = x / y;
        all_is_ok &= z.is_positive();
    }

    assert(all_is_ok);
    qDebug() << "Test is Ok!";

    auto ctx = engine.rootContext();
    ctx->setContextProperty("AppCore", &AppCore);

    const QUrl url(QStringLiteral("qrc:/main.qml"));
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated,
                     &app, [url](QObject *obj) {
        if (!obj)
            QCoreApplication::exit(-1);
    }, Qt::QueuedConnection);
    engine.load(url);

    return app.exec();
}
