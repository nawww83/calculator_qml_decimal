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
    zero.set(0, 0, 1);
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

    assert(dec2.is_an_integer() && "Dec2 must be an integer!");

    auto dec3 = dec1 * dec2;
    auto dec4 = dec2 * dec1;

    Decimal<> sum;
    sum.set(0, 0, 1);
    if (dec2.integer_part() >= 0) {
        for (long long x=0; x<std::abs(dec2.integer_part()); ++x) {
            sum = sum + dec1;
        }
    } else {
        for (long long x=0; x<std::abs(dec2.integer_part()); ++x) {
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

    // Full coverage deterministic test
    // qDebug() << "Full coverage test...";
    for (long long i=-15ll; i<15ll; ++i) {
        for (long long j=-15ll; j<15ll; ++j) {
            if ((i != 0) && (j < 0)) {
                continue;
            }
            dec1.set(i, j, 15ll);
            if (! all_is_ok) {
                break;
            }
            for (long long k=-15ll; k<15ll; ++k) {
                for (long long l=-15ll; l<15ll; ++l) {
                    if ((k != 0) && (l < 0)) {
                        continue;
                    }
                    dec2.set(k, l, 15ll);
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
            dec1.set(i, j, 15ll);
            if (! all_is_ok) {
                break;
            }
            for (long long k=-15ll; k<15ll; ++k) {
                dec2.set(k, 0, 15ll);
                all_is_ok &= test_decimal_multiplicatives(dec1, dec2);
                if (! all_is_ok) {
                    break;
                }
            }
        }
    }
    // Static test
    // qDebug() << "Static test...";
    dec1.set(1, 100, 100);
    dec2.set(0, -100, 100);
    // cout << dec1.value() << "\n";
    assert(dec1.value() == "2,00");
    assert(dec2.value() == "-1,00");
    dec1.set(0, -99, 100);
    dec2.set(0, 100, 100);
    assert(dec1.value() == "-0,99");
    assert(dec2.value() == "1,00");
    dec1.set(-1, 100, 100);
    dec2.set(-1, 99, 100);
    assert(dec1.value() == "-2,00");
    assert(dec2.value() == "-1,99");
    dec1.set(-1, 0, 100);
    dec2.set(0, -1, 100);
    assert(dec1.value() == "-1,00");
    assert(dec2.value() == "-0,01");

    dec1.set(0, 0, 100);
    dec2.set(1, 0, 100);
    all_is_ok &= test_decimal_additives(dec1, dec2);

    dec1.set(0, 0, 100);
    dec2.set(0, 0, 100);
    all_is_ok &= test_decimal_additives(dec1, dec2);

    dec1.set(1, 0, 100);
    dec2.set(0, 0, 100);
    all_is_ok &= test_decimal_additives(dec1, dec2);

    dec1.set(1, 0, 100);
    dec2.set(1, 0, 100);
    all_is_ok &= test_decimal_additives(dec1, dec2);

    dec1.set(5, 5, 100);
    dec2.set(5, 5, 100);
    all_is_ok &= test_decimal_additives(dec1, dec2);

    dec1.set(0, 5, 100);
    dec2.set(0, 5, 100);
    all_is_ok &= test_decimal_additives(dec1, dec2);

    dec1.set(9, 5, 100);
    dec2.set(9, 5, 100);
    all_is_ok &= test_decimal_additives(dec1, dec2);

    dec1.set(10, 5, 100);
    dec2.set(9, 99, 100);
    all_is_ok &= test_decimal_additives(dec1, dec2);

    dec1.set(99, 45, 100);
    dec2.set(9, 89, 100);
    all_is_ok &= test_decimal_additives(dec1, dec2);

    dec1.set(99, 25, 100);
    dec2.set(1, 85, 100);
    all_is_ok &= test_decimal_additives(dec1, dec2);

    dec1.set(99, 45, 100);
    dec2.set(99, 89, 100);
    all_is_ok &= test_decimal_additives(dec1, dec2);

    dec1.set(99, 85, 100);
    dec2.set(99, 39, 100);
    all_is_ok &= test_decimal_additives(dec1, dec2);

    dec1.set(9, 85, 100);
    dec2.set(9, 89, 100);
    all_is_ok &= test_decimal_additives(dec1, dec2);

    dec1.set(-9, 85, 100);
    dec2.set(-9, 89, 100);
    all_is_ok &= test_decimal_additives(dec1, dec2);

    dec1.set(-9, 85, 100);
    dec2.set(9, 89, 100);
    all_is_ok &= test_decimal_additives(dec1, dec2);

    dec1.set(9, 85, 100);
    dec2.set(-9, 89, 100);
    all_is_ok &= test_decimal_additives(dec1, dec2);

    dec1.set(1, 1, 100);
    dec2.set(1, 1, 100);
    all_is_ok &= test_decimal_additives(dec1, dec2);

    dec1.set(0, -1, 100);
    dec2.set(0, 1, 100);
    all_is_ok &= test_decimal_additives(dec1, dec2);

    dec1.set(0, 1, 100);
    dec2.set(0, -1, 100);
    all_is_ok &= test_decimal_additives(dec1, dec2);

    dec1.set(0, -1, 100);
    dec2.set(0, -1, 100);
    all_is_ok &= test_decimal_additives(dec1, dec2);

    dec1.set(0, -2, 100);
    dec2.set(0, 1, 100);
    all_is_ok &= test_decimal_additives(dec1, dec2);

    dec1.set(0, -1, 100);
    dec2.set(0, 2, 100);
    all_is_ok &= test_decimal_additives(dec1, dec2);


    // Mult. * and div
    {
        Decimal mul;
        dec1.set(14, 88, 100ll);
        dec2.set(-66, 74, 100ll);
        mul = dec1 * dec2;
        // cout << dec1.value() << " * " << dec2.value() << " = " << mul.value() << "\n";
        assert( mul.value() == "-993,09" );

        dec1.set(14, 88, 100ll);
        dec2.set(0, -74, 100ll);
        mul = dec1 * dec2;
        // cout << dec1.value() << " * " << dec2.value() << " = " << mul.value() << "\n";
        assert( mul.value() == "-11,01" );

        dec1.set(0, 88, 100ll);
        dec2.set(0, -74, 100ll);
        mul = dec1 * dec2;
        // cout << dec1.value() << " * " << dec2.value() << " = " << mul.value() << "\n";
        assert( mul.value() == "-0,65" );

        dec1.set(0, 88, 100ll);
        dec2.set(0, 74, 100ll);
        mul = dec1 * dec2;
        // cout << dec1.value() << " * " << dec2.value() << " = " << mul.value() << "\n";
        assert( mul.value() == "0,65" );


        dec1.set(-14, 88, 100ll);
        dec2.set(66, 74, 100ll);
        mul = dec1 * dec2;
        // cout << dec1.value() << " * " << dec2.value() << " = " << mul.value() << "\n";
        assert( mul.value() == "-993,09");

        dec1.set(-14, 88, 100ll);
        dec2.set(0, 74, 100ll);
        mul = dec1 * dec2;
        // cout << dec1.value() << " * " << dec2.value() << " = " << mul.value() << "\n";
        assert( mul.value() == "-11,01" );

        dec1.set(0, 74, 100ll);
        dec2.set(-14, 88, 100ll);
        mul = dec1 * dec2;
        // cout << dec1.value() << " * " << dec2.value() << " = " << mul.value() << "\n";
        assert( mul.value() == "-11,01" );

        dec1.set(0, -88, 100ll);
        dec2.set(0, 74, 100ll);
        mul = dec1 * dec2;
        // cout << dec1.value() << " * " << dec2.value() << " = " << mul.value() << "\n";
        assert( mul.value() == "-0,65" );

        dec1.set(-14, 88, 100ll);
        dec2.set(-66, 74, 100ll);
        mul = dec1 * dec2;
        // cout << dec1.value() << " * " << dec2.value() << " = " << mul.value() << "\n";
        assert( mul.value() == "993,09");

        dec1.set(-14, 88, 100ll);
        dec2.set(0, -74, 100ll);
        mul = dec1 * dec2;
        // cout << dec1.value() << " * " << dec2.value() << " = " << mul.value() << "\n";
        assert( mul.value() == "11,01" );

        dec1.set(0, -88, 100ll);
        dec2.set(0, -74, 100ll);
        mul = dec1 * dec2;
        // cout << dec1.value() << " * " << dec2.value() << " = " << mul.value() << "\n";
        assert( mul.value() == "0,65" );

        dec1.set(5, 0, 100ll);
        dec2.set(0, -74, 100ll);
        mul = dec1 * dec2;
        // cout << dec1.value() << " * " << dec2.value() << " = " << mul.value() << "\n";
        assert( mul.value() == "-3,70" );

        dec1.set(-5, 0, 100ll);
        dec2.set(0, -74, 100ll);
        mul = dec1 * dec2;
        // cout << dec1.value() << " * " << dec2.value() << " = " << mul.value() << "\n";
        assert( mul.value() == "3,70" );

        dec1.set(-5, 0, 100ll);
        dec2.set(0, 74, 100ll);
        mul = dec1 * dec2;
        // cout << dec1.value() << " * " << dec2.value() << " = " << mul.value() << "\n";
        assert( mul.value() == "-3,70" );

        dec1.set(0, 0, 100ll);
        dec2.set(0, 0, 100ll);
        mul = dec1 * dec2;
        // cout << dec1.value() << " * " << dec2.value() << " = " << mul.value() << "\n";
        assert( mul.value() == "0,00" );

        dec1.set(-7, 0, 100ll);
        dec2.set(-7, 0, 100ll);
        mul = dec1 * dec2;
        // cout << dec1.value() << " * " << dec2.value() << " = " << mul.value() << "\n";
        assert( mul.value() == "49,00" );

        Decimal div;

        dec1.set(555543345675ll, 78ll, 100ll);
        dec2.set(11ll, 0, 100ll); // denominator is an integer
        mul = dec1 * dec2;
        div = mul / dec2;
        assert(div.value() == dec1.value()); // exactly

        dec1.set(555543345675ll, 78ll, 100ll);
        dec2.set(-11ll, 0, 100ll); // denominator is an integer
        mul = dec1 * dec2;
        div = mul / dec2;
        assert(div.value() == dec1.value()); // exactly

        dec1.set(-555543345675ll, 78ll, 100ll);
        dec2.set(11ll, 0, 100ll); // denominator is an integer
        mul = dec1 * dec2;
        div = mul / dec2;
        assert(div.value() == dec1.value()); // exactly

        dec1.set(-555543345675ll, 78ll, 100ll);
        dec2.set(-11ll, 0, 100ll); // denominator is an integer
        mul = dec1 * dec2;
        div = mul / dec2;
        assert(div.value() == dec1.value()); // exactly

        dec1.set(99999ll, 0, 100ll);
        dec2.set(0, 99ll, 100ll);
        div = dec1 / dec2;
        assert(div.value() == "101009,09");

        dec1.set(99999ll, 99ll, 100ll);
        dec2.set(0, 99ll, 100ll);
        div = dec1 / dec2;
        assert(div.value() == "101010,09");

        dec1.set(99999ll, 0, 100ll);
        dec2.set(0, -99ll, 100ll);
        div = dec1 / dec2;
        assert(div.value() == "-101009,09");

        dec1.set(-99999ll, 99ll, 100ll);
        dec2.set(0, 99ll, 100ll);
        div = dec1 / dec2;
        assert(div.value() == "-101010,09");

        dec1.set(1234567ll, 0, 100ll);
        dec2.set(89ll, 0, 100ll);
        div = dec1 / dec2;
        assert(div.value() == "13871,53");

        dec1.set(1234567ll, 89ll, 100ll);
        dec2.set(12ll, 34ll, 100ll);
        div = dec1 / dec2;
        assert(div.value() == "100046,02");

        dec1.set(-1234567ll, 89ll, 100ll);
        dec2.set(-12ll, 34ll, 100ll);
        div = dec1 / dec2;
        assert(div.value() == "100046,02");

        dec1.set(1234567ll, 89ll, 100ll);
        dec2.set(-12ll, 34ll, 100ll);
        div = dec1 / dec2;
        assert(div.value() == "-100046,02");

        dec1.set(666666666666666666ll, 0ll, 100ll);
        dec2.set(7ll, 0, 100ll);
        div = dec1 / dec2;
        assert(div.value() == "95238095238095238,00");
    }

    assert(all_is_ok);
    // qDebug() << (all_is_ok ? "Decimal testing is Ok!" : "Failed!");
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
