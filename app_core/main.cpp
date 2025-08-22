#include <QGuiApplication>
#include <QQmlApplicationEngine>

#include "AppCore.h"
#include <QQmlContext>
#include <QSettings>

using namespace dec_n;

#ifdef QT_DEBUG
static void run_unit_tests() {
    bool all_is_ok = true;

    { // max_int / 10
        u128::U128 x = u128::U128::get_max_value();
        x = x.div10();
        all_is_ok &= !x.is_singular();
        all_is_ok &= x.value().starts_with("34028236692093846346337460743176821145");
        assert(all_is_ok);
    }

    { // max_int / max_int
        u128::U128 x = u128::U128::get_max_value();
        u128::U128 y = u128::U128::get_max_value();
        const auto [z, remainder] = x / y;
        all_is_ok &= z == u128::U128{1};
        assert(all_is_ok);
    }

    { // max_int * 1
        u128::U128 x = u128::U128::get_max_value();
        const auto z = x * u128::U128{1};
        all_is_ok &= !x.is_singular();
        all_is_ok &= x == z;
        assert(all_is_ok);
    }

    { // max_int * -1 == -max_int
        u128::U128 x = u128::U128::get_max_value();
        const auto z = x * (-u128::U128{1});
        all_is_ok &= !x.is_singular();
        all_is_ok &= -x == z;
        assert(all_is_ok);
    }

    { // max_int / 1
        u128::U128 x = u128::U128::get_max_value();
        const auto [z, remainder] = x / u128::U128{1};
        all_is_ok &= !x.is_singular();
        all_is_ok &= x == z;
        assert(all_is_ok);
    }

    { // max_int / -1 == -max_int
        u128::U128 x = u128::U128::get_max_value();
        const auto [z, remainder] = x / (-u128::U128{1});
        all_is_ok &= !x.is_singular();
        all_is_ok &= -x == z;
        assert(all_is_ok);
    }

    {
        Decimal d1; d1.SetDecimal(u128::U128{1}, u128::U128{0} );
        const auto z = -d1;
        all_is_ok &= z.ValueAsStringView().starts_with("-1");
        assert(all_is_ok);
    }

    { // max_int * 1,00...01
        Decimal d1; d1.SetDecimal(u128::U128::get_max_value(), u128::U128{0} );
        Decimal d2; d2.SetDecimal(u128::U128{1}, u128::U128{1} );
        assert(d2.GetWidth() > 0);
        const auto z = d1 * d2;
        all_is_ok &= z.IsOverflowed();
        assert(all_is_ok);
    }

    { // max_int * 0,5
        Decimal d1; d1.SetDecimal(u128::U128::get_max_value(), u128::U128{0} );
        Decimal d2; d2.SetDecimal(u128::U128{0}, u128::U128{1}, u128::U128{2} );
        assert(d2.GetWidth() > 0);
        const auto z = d1 * d2;
        all_is_ok &= !z.IsOverflowed();
        all_is_ok &= d2.ValueAsStringView().starts_with("0,5");
        all_is_ok &= z.ValueAsStringView().starts_with("170141183460469231731687303715884105727");
        assert(all_is_ok);
    }

    { // 0,5 * max_int
        Decimal d1; d1.SetDecimal(u128::U128::get_max_value(), u128::U128{0} );
        Decimal d2; d2.SetDecimal(u128::U128{0}, u128::U128{1}, u128::U128{2} );
        assert(d2.GetWidth() > 0);
        const auto z = d2 * d1;
        all_is_ok &= !z.IsOverflowed();
        all_is_ok &= d2.ValueAsStringView().starts_with("0,5");
        all_is_ok &= z.ValueAsStringView().starts_with("170141183460469231731687303715884105727");
        assert(all_is_ok);
    }

    { // max_int / 1,999: correct all 9's to all 0's with corresponding integer part adjusting.
        Decimal d1; d1.SetDecimal(u128::U128::get_max_value(), u128::U128{0} );
        Decimal d2; d2.SetDecimal(u128::U128{1},  u128::U128{999} );
        assert(d2.GetWidth() == 3);
        const auto z = d1 / d2;
        all_is_ok &= !z.IsOverflowed();
        all_is_ok &= z.ValueAsStringView().starts_with("170141183460469231731687303715884105727,500");
        assert(all_is_ok);
    }

    { // max_int / -1,999
        Decimal d1; d1.SetDecimal(u128::U128::get_max_value(), u128::U128{0} );
        Decimal d2; d2.SetDecimal(-u128::U128{1},  u128::U128{999} );
        assert(d2.GetWidth() == 3);
        const auto z = d1 / d2;
        all_is_ok &= !z.IsOverflowed();
        all_is_ok &= z.ValueAsStringView().starts_with("-170141183460469231731687303715884105727,500");
        assert(all_is_ok);
    }

    { // -max_int / 1,999
        Decimal d1; d1.SetDecimal(-u128::U128::get_max_value(), u128::U128{0} );
        Decimal d2; d2.SetDecimal(u128::U128{1},  u128::U128{999} );
        assert(d2.GetWidth() == 3);
        const auto z = d1 / d2;
        all_is_ok &= !z.IsOverflowed();
        all_is_ok &= z.ValueAsStringView().starts_with("-170141183460469231731687303715884105727,500");
        assert(all_is_ok);
    }

    { // -max_int / -1,999
        Decimal d1; d1.SetDecimal(-u128::U128::get_max_value(), u128::U128{0} );
        Decimal d2; d2.SetDecimal(-u128::U128{1},  u128::U128{999} );
        assert(d2.GetWidth() == 3);
        const auto z = d1 / d2;
        all_is_ok &= !z.IsOverflowed();
        all_is_ok &= z.ValueAsStringView().starts_with("170141183460469231731687303715884105727,500");
        assert(all_is_ok);
    }

    { // (max_int / 1,9) * 1,9
        Decimal d1; d1.SetDecimal(u128::U128::get_max_value(), u128::U128{0} );
        Decimal d2; d2.SetDecimal(u128::U128{1},  u128::U128{900} );
        assert(d2.GetWidth() == 3);
        auto z = d1 / d2;
        z = z * d2;
        const auto error = z - d1;
        all_is_ok &= !z.IsOverflowed();
        all_is_ok &= error.Abs().IntegerPart().is_zero();
        assert(all_is_ok);
    }

    { // (max_int * 0,9) / 0,9
        Decimal d1; d1.SetDecimal(u128::U128::get_max_value(), u128::U128{0} );
        Decimal d2; d2.SetDecimal(u128::U128{0},  u128::U128{900} );
        assert(d2.GetWidth() == 3);
        auto z = d1 * d2;
        z = z / d2;
        const auto error = z - d1;
        all_is_ok &= !z.IsOverflowed();
        all_is_ok &= error.Abs().IntegerPart().is_zero();
        assert(all_is_ok);
    }

    { // max_int / 0,999
        Decimal d1; d1.SetDecimal(u128::U128::get_max_value(), u128::U128{0} );
        Decimal d2; d2.SetDecimal(u128::U128{0},  u128::U128{999} );
        assert(d2.GetWidth() == 3);
        const auto z = d1 / d2;
        all_is_ok &= !z.IsOverflowed();
        assert(all_is_ok);
    }

    { // -max_int / 0,999
        Decimal d1; d1.SetDecimal(-u128::U128::get_max_value(), u128::U128{0} );
        Decimal d2; d2.SetDecimal(u128::U128{0},  u128::U128{999} );
        assert(d2.GetWidth() == 3);
        const auto z = d1 / d2;
        all_is_ok &= !z.IsOverflowed();
        assert(all_is_ok);
    }

    { // -max_int / -0,999
        Decimal d1; d1.SetDecimal(-u128::U128::get_max_value(), u128::U128{0} );
        Decimal d2; d2.SetDecimal(u128::U128{0},  u128::U128{999} );
        assert(d2.GetWidth() == 3);
        const auto z = d1 / -d2;
        all_is_ok &= !z.IsOverflowed();
        assert(all_is_ok);
    }

    { // max_int,999 + 0,001
        Decimal d1; d1.SetDecimal(u128::U128::get_max_value(), u128::U128{999} );
        Decimal d2; d2.SetDecimal(u128::U128{0}, u128::U128{1} );
        assert(d2.GetWidth() == 3);
        const auto z = d1 + d2;
        all_is_ok &= z.IsOverflowed();
        assert(all_is_ok);
    }

    { // -max_int,999 - 0,001
        Decimal d1; d1.SetDecimal(-u128::U128::get_max_value(), u128::U128{999} );
        Decimal d2; d2.SetDecimal(u128::U128{0}, u128::U128{1} );
        assert(d2.GetWidth() == 3);
        const auto z = d1 - d2;
        all_is_ok &= z.IsOverflowed();
        assert(all_is_ok);
    }

    { // max_int,999 + 0,000
        Decimal d1; d1.SetDecimal(u128::U128::get_max_value(), u128::U128{999} );
        Decimal d2; d2.SetDecimal(u128::U128{0}, u128::U128{0} );
        assert(d2.GetWidth() == 3);
        const auto z = d1 + d2;
        all_is_ok &= z.IsOverflowed();
        assert(all_is_ok);
    }

    { // -max_int,999 - 0,000
        Decimal d1; d1.SetDecimal(-u128::U128::get_max_value(), u128::U128{999} );
        Decimal d2; d2.SetDecimal(u128::U128{0}, u128::U128{0} );
        assert(d2.GetWidth() == 3);
        const auto z = d1 - d2;
        all_is_ok &= z.IsOverflowed();
        assert(all_is_ok);
    }

    { // -X * -Y
        Decimal d1; d1.SetDecimal(-u128::U128{55}, u128::U128{550} );
        Decimal d2; d2.SetDecimal(-u128::U128{44}, u128::U128{440} );
        assert(d2.GetWidth() == 3);
        const auto z = d1 * d2;
        all_is_ok &= z.ValueAsStringView() == "2468,642";
        assert(all_is_ok);
    }

    { // -X * Y
        Decimal d1; d1.SetDecimal(-u128::U128{55}, u128::U128{550} );
        Decimal d2; d2.SetDecimal(u128::U128{44}, u128::U128{440} );
        assert(d2.GetWidth() == 3);
        const auto z = d1 * d2;
        all_is_ok &= z.ValueAsStringView() == "-2468,642";
        assert(all_is_ok);
    }

    { // X * -Y
        Decimal d1; d1.SetDecimal(u128::U128{55}, u128::U128{550} );
        Decimal d2; d2.SetDecimal(-u128::U128{44}, u128::U128{440} );
        assert(d2.GetWidth() == 3);
        const auto z = d1 * d2;
        all_is_ok &= z.ValueAsStringView() == "-2468,642";
        assert(all_is_ok);
    }

    { // X * Y
        Decimal d1; d1.SetDecimal(u128::U128{55}, u128::U128{550} );
        Decimal d2; d2.SetDecimal(u128::U128{44}, u128::U128{440} );
        assert(d2.GetWidth() == 3);
        const auto z = d1 * d2;
        all_is_ok &= z.ValueAsStringView() == "2468,642";
        assert(all_is_ok);
    }

    { // X + Y
        Decimal d1; d1.SetDecimal(u128::U128{55}, u128::U128{555} );
        Decimal d2; d2.SetDecimal(u128::U128{44}, u128::U128{445} );
        assert(d2.GetWidth() == 3);
        const auto z = d1 + d2;
        all_is_ok &= z.ValueAsStringView() == "100,000";
        assert(all_is_ok);
    }

    { // X - Y
        Decimal d1; d1.SetDecimal(u128::U128{55}, u128::U128{555} );
        Decimal d2; d2.SetDecimal(-u128::U128{44}, u128::U128{445} );
        assert(d2.GetWidth() == 3);
        const auto z = d1 + d2;
        all_is_ok &= z.ValueAsStringView() == "11,110";
        assert(all_is_ok);
    }

    { // -X + Y
        Decimal d1; d1.SetDecimal(-u128::U128{55}, u128::U128{555} );
        Decimal d2; d2.SetDecimal(u128::U128{44}, u128::U128{445} );
        assert(d2.GetWidth() == 3);
        const auto z = d1 + d2;
        all_is_ok &= z.ValueAsStringView() == "-11,110";
        assert(all_is_ok);
    }

    { // -X - Y
        Decimal d1; d1.SetDecimal(-u128::U128{55}, u128::U128{555} );
        Decimal d2; d2.SetDecimal(-u128::U128{44}, u128::U128{445} );
        assert(d2.GetWidth() == 3);
        const auto z = d1 + d2;
        all_is_ok &= z.ValueAsStringView() == "-100,000";
        assert(all_is_ok);
    }

    {
        const auto thousand = u128::utils::int_power(10, 3);
        all_is_ok &= thousand == u128::U128{1000};
        assert(all_is_ok);
    }

    {
        const auto ten_to_36 = u128::utils::int_power(10, 36);
        const auto thousand = u128::utils::int_power(10, 3);
        const auto z = ten_to_36 * thousand;
        all_is_ok &= z.is_overflow();
        assert(all_is_ok);
    }

    {
        const auto ten_to_36 = u128::utils::int_power(10, 36);
        Decimal d1; d1.SetDecimal(ten_to_36, u128::U128{0} );
        Decimal d2; d2.SetDecimal(u128::U128{0}, u128::U128{1} );
        const auto z = d1 / d2;
        assert(d2.GetWidth() == 3);
        all_is_ok &= z.IsOverflowed();
        assert(all_is_ok);
    }

    {
        Decimal half; half.SetDecimal(u128::U128{0}, u128::U128{500});
        dec_n::Decimal one;
        one.SetDecimal(u128::U128{1}, u128::U128{0});
        const auto& two = one / half;
        assert(half.GetWidth() == 3);
        all_is_ok &= two.ValueAsStringView() == "2,000";
        assert(all_is_ok);
    }

    {
        Decimal two; two.SetDecimal(u128::U128{2}, u128::U128{0});
        bool exact;
        const auto& y = Sqrt(two, exact);
        assert(y.GetWidth() == 3);
        all_is_ok &= y.ValueAsStringView() == "1,414";
        assert(all_is_ok);
    }

    {
        u128::U128 result{};
        result.mLow = 1;
        result.mHigh = 0;
        result.mSign = true;
        all_is_ok &= result.is_negative();
        assert(all_is_ok);
    }

    {
        u128::U128 result{1, 0, true};
        all_is_ok &= result.is_negative();
        assert(all_is_ok);
    }

    {
        u128::U128 big_number {18'446'744'073'709'551'610ull};
        u128::U128 result = big_number + u128::U128{6};
        all_is_ok &= result.value().starts_with( "18446744073709551616" );
        assert(all_is_ok);
    }

    {
        u128::U128 big_number = u128::U128::shl64(u128::U128{1});
        all_is_ok &= big_number.mod10() == 6; // 2**64 % 10
        assert(all_is_ok);
    }

    {
        u128::U128 max_int_value = u128::U128::get_max_value();
        all_is_ok &= max_int_value.value() == "340282366920938463463374607431768211455";
        assert(all_is_ok);
    }

    {
        u128::U128 max_int_value = u128::U128::get_max_value();
        u128::U128 overflowed = max_int_value + u128::U128{1};
        all_is_ok &= overflowed.is_overflow();
        assert(all_is_ok);
    }

    {
        u128::U128 max_int_value = u128::U128::get_max_value();
        u128::U128 overflowed = max_int_value * u128::U128{2};
        all_is_ok &= overflowed.is_overflow();
        assert(all_is_ok);
    }

    {
        u128::U128 max_int_value = u128::U128::get_max_value();
        u128::U128 normal_value = max_int_value - u128::U128{1};
        all_is_ok &= !normal_value.is_singular();
        assert(all_is_ok);
    }

    {
        u128::U128 x {1};
        u128::U128 y {1, 0, true};
        const auto z = x * y;
        all_is_ok &= z.is_negative();
        assert(all_is_ok);
    }

    {
        u128::U128 x {1, 0, true};
        u128::U128 y {1, 0, true};
        const auto z = x * y;
        all_is_ok &= z.is_positive();
        assert(all_is_ok);
    }

    {
        u128::U128 x {1};
        u128::U128 y {1, 0, true};
        const auto [z, remainder] = x / y;
        all_is_ok &= z.is_negative();
        assert(all_is_ok);
    }

    {
        u128::U128 x {1, 0, true};
        u128::U128 y {1, 0, true};
        const auto [z, remainder] = x / y;
        all_is_ok &= z.is_positive();
        assert(all_is_ok);
    }
}
#endif


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

    QSettings settings("MyHome", "DecimalCalculator");
    bool ok;
    int width = settings.value("DecimalWidth").toInt(&ok);
    if (ok) {
        const bool quiet = true;
        AppCore.change_decimal_width(width, quiet);
    }

    #ifdef QT_DEBUG
    qDebug() << "Test...";
    run_unit_tests();
    qDebug() << "Test is Ok!";
    #endif

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
