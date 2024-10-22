#include <QGuiApplication>
#include <QQmlApplicationEngine>

#include "AppCore.h"
#include <QQmlContext>

using namespace dec_n;

bool test_decimal_additives(Decimal<> dec1, Decimal<> dec2) {
    using namespace std;
    auto dec3 = dec1 + dec2;
    auto dec4 = dec1 - dec2;
    auto dec5 = dec2 - dec1;

    Decimal<> zero;
    zero.SetDecimal(0, 0, 1);
    bool is_ok = true;
    is_ok &= ((dec4 + dec5) == zero);
    is_ok &= ((dec5 + dec4) == zero);
    is_ok &= ((dec3 - dec1) == dec2);
    is_ok &= ((dec3 - dec2) == dec1);
    is_ok &= ((dec2 + dec4) == dec1);
    is_ok &= ((dec4 + dec2) == dec1);
    is_ok &= ((dec1 + dec5) == dec2);
    is_ok &= ((dec5 + dec1) == dec2);
    is_ok &= ((dec3 + dec4 + dec5) == (dec1 + dec2));
    return is_ok;
}

bool test_decimal_multiplicatives(Decimal<> dec1, Decimal<> dec2) {
    using namespace std;

    assert(dec2.IsInteger() && "Dec2 must be an integer!");

    auto dec3 = dec1 * dec2;
    auto dec4 = dec2 * dec1;

    Decimal<> sum;
    sum.SetDecimal(0, 0, 1);
    if (dec2.IntegerPart() >= 0) {
        for (long long x=0; x<std::abs(dec2.IntegerPart()); ++x) {
            sum = sum + dec1;
        }
    } else {
        for (long long x=0; x<std::abs(dec2.IntegerPart()); ++x) {
            sum = sum - dec1;
        }
    }
    bool is_ok = ((sum == dec3) && (sum == dec4) && (dec3 == dec4));
    return is_ok;
}


int main(int argc, char *argv[])
{
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
#endif

    QGuiApplication app(argc, argv);

    qRegisterMetaType<QVector<dec_n::Decimal<>>>("QVector<dec_n::Decimal<>>");

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
    //
    qDebug() << "Testing...";
    bool all_is_ok = true;
    Decimal dec1;
    Decimal dec2;

    for (long long i=-15ll; i<15ll; ++i) {
        for (long long j=-15ll; j<15ll; ++j) {
            if ((i != 0) && (j < 0)) {
                continue;
            }
            dec1.SetDecimal(i, j, 15ll);
            if (! all_is_ok) {
                break;
            }
            for (long long k=-15ll; k<15ll; ++k) {
                for (long long l=-15ll; l<15ll; ++l) {
                    if ((k != 0) && (l < 0)) {
                        continue;
                    }
                    dec2.SetDecimal(k, l, 15ll);
                    all_is_ok &= test_decimal_additives(dec1, dec2);
                    if (! all_is_ok) {
                        break;
                    }
                }
            }
        }
    }
    for (long long i=-15ll; i<15ll; ++i) {
        for (long long j=-15ll; j<15ll; ++j) {
            if ((i != 0) && (j < 0)) {
                continue;
            }
            dec1.SetDecimal(i, j, 15ll);
            if (! all_is_ok) {
                break;
            }
            for (long long k=-15ll; k<15ll; ++k) {
                dec2.SetDecimal(k, 0, 15ll);
                all_is_ok &= test_decimal_multiplicatives(dec1, dec2);
                if (! all_is_ok) {
                    break;
                }
            }
        }
    }
    {
        Decimal res;
        dec1.SetDecimal(9223372036854775807ll, 50ll, 100ll);
        dec2.SetDecimal(9223372036854775807ll, 50ll, 100ll);
        res = dec1 + dec2;
        assert(res.IsOverflowed() && "Must be overflow!");

        dec1.SetDecimal(9223372036854775807ll, 0, 100ll);
        dec2.SetDecimal(1ll, 0, 100ll);
        res = dec1 + dec2;
        assert(res.IsOverflowed() && "Must be overflow!");

        dec1.SetDecimal(4611686018427387904ll, 0, 100ll);
        dec2.SetDecimal(2ll, 0, 100ll);
        res = dec1 * dec2;
        assert(res.IsOverflowed() && "Must be overflow!");

        dec1.SetDecimal(4611686018427387903ll, 0, 100ll);
        dec2.SetDecimal(2ll, 0, 100ll);
        res = dec1 * dec2;
        assert(!res.IsOverflowed() && "Must not be overflow!");

        dec1.SetDecimal(-4611686018427387904ll, 0, 100ll);
        dec2.SetDecimal(2ll, 0, 100ll);
        res = dec1 * dec2;
        assert(res.IsOverflowed() && "Must be overflow!");

        dec1.SetDecimal(-4611686018427387905ll, 0, 100ll);
        dec2.SetDecimal(2ll, 0, 100ll);
        res = dec1 * dec2;
        assert(res.IsOverflowed() && "Must be overflow!");

        dec1.SetDecimal(0, 0, 0);
        assert(dec1.IsNotANumber() && "Must be NAN!");

        dec2.SetDecimal(-1, -1, 1);
        assert(dec2.IsOverflowed() && "Must be overflow!");

        dec1.SetStringRepresentation("");
        assert(dec1.IsNotANumber() && "Must be NAN!");

        dec2.SetStringRepresentation("inf");
        assert(dec2.IsOverflowed() && "Must be overflow!");
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
