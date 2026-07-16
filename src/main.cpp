#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>

// Pull in the auto-generated registration code produced by qt_add_qml_module
#include "geomanager.h"
#include "qgisprojectgenerator.h"

int main(int argc, char *argv[])
{
    // High-DPI rendering handled automatically by Qt 6
    QGuiApplication app(argc, argv);

    app.setApplicationName(QStringLiteral("Provectos MDM Móvil"));
    app.setApplicationVersion(QStringLiteral("0.1.0"));
    app.setOrganizationName(QStringLiteral("MDM"));

    // Use the Material style for a modern look & feel
    QQuickStyle::setStyle(QStringLiteral("Material"));

    QQmlApplicationEngine engine;

    // ---------------------------------------------------------------------------
    // The QML_ELEMENT / QML_SINGLETON macros together with qt_add_qml_module
    // automatically register GeoManager and QgisProjectGenerator into the "App"
    // URI.  No manual qmlRegisterType() calls are needed; we only need to ensure
    // the module initialisation function is called.
    // ---------------------------------------------------------------------------

    const QUrl mainUrl(QStringLiteral("qrc:/App/qml/main.qml"));

    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreated,
        &app, [mainUrl](QObject *obj, const QUrl &objUrl) {
            if (!obj && objUrl == mainUrl)
                QCoreApplication::exit(-1);
        },
        Qt::QueuedConnection);

    engine.load(mainUrl);

    return app.exec();
}
