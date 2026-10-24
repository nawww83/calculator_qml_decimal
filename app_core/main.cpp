#include <QGuiApplication>
#include <QQmlApplicationEngine>

#include "AppCore.h"
#include <QQmlContext>
#include <QQuickStyle>
#include <QSettings>

using namespace dec_n;

int main(int argc, char *argv[])
{
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
#endif

    QGuiApplication app(argc, argv);

    // Принудительно устанавливаем стиль, поддерживающий кастомизацию
    QQuickStyle::setStyle("Basic");

    qRegisterMetaType<QVector<dec_n::Decimal>>("QVector<dec_n::Decimal>");

    qmlRegisterUncreatableMetaObject(OperationEnums::staticMetaObject,
                                     "operation.enums",
                                     1,
                                     0,
                                     "OperationEnums",
                                     "Error: only enums");

    AppCore appCore; // Экземпляр с маленькой буквы во избежание конфликтов имен

    // Регистрируем как синглтон для QML
    qmlRegisterSingletonInstance("AppCoreBackend", 1, 0, "AppCore", &appCore);

    QQmlApplicationEngine engine;

    // Читаем сохраненную точность из QSettings, дефолт берем из C++
    QSettings settings("MyHome", "DecimalCalculator");
    bool ok = false;
    int saved_width = settings.value("DecimalWidth", AppCore::DEFAULT_DISPLAY_WIDTH).toInt(&ok);
    int initial_width = ok ? std::clamp(saved_width, 0, 18) : AppCore::DEFAULT_DISPLAY_WIDTH;

    // Инициализируем AppCore пользовательской точностью
    appCore.change_decimal_width(initial_width, true);

    auto ctx = engine.rootContext();
    ctx->setContextProperty("AppCore", &appCore);

    const QUrl url(QStringLiteral("qrc:/main.qml"));
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated,
                     &app, [url](QObject *obj) {
                         if (!obj)
                             QCoreApplication::exit(-1);
                     }, Qt::QueuedConnection);

    engine.load(url);

    return app.exec();
}
