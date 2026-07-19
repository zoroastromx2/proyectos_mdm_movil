#include <QCoreApplication>
#include <QDir>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>

// Kept intentionally: ensures translation units with QML type registration
// metadata are linked into the final binary.
// Pull in the auto-generated registration code produced by qt_add_qml_module
#include "geomanager.h"
#include "qgisprojectgenerator.h"

// ---------------------------------------------------------------------------
// Automatically set GDAL/PROJ runtime environment variables if not already
// set.  The vcpkg post-build step deploys all DLLs next to the executable;
// the data directories are inside the same vcpkg_installed tree.  We locate
// them relative to the executable so the app works without any manual env
// setup on any machine.
// ---------------------------------------------------------------------------
static void setupGdalEnvironment()
{
    // appDir is the directory that contains proyectos_mdm_movil.exe.
    // vcpkg installs data files into  <build_dir>/vcpkg_installed/x64-windows/
    // which is one level above appDir (appDir IS the build output dir).
    const QString appDir = QCoreApplication::applicationDirPath();
    const QString vcpkgShare = QDir(appDir).filePath(
        QStringLiteral("vcpkg_installed/x64-windows/share"));
    const QString vcpkgLib   = QDir(appDir).filePath(
        QStringLiteral("vcpkg_installed/x64-windows/lib"));

    auto setIfEmpty = [](const char *var, const QString &path) {
        if (qEnvironmentVariableIsEmpty(var)) {
            const QString native = QDir::toNativeSeparators(QDir::cleanPath(path));
            qputenv(var, native.toUtf8());
        }
    };

    setIfEmpty("GDAL_DATA",        vcpkgShare + QStringLiteral("/gdal"));
    setIfEmpty("GDAL_DRIVER_PATH", vcpkgLib   + QStringLiteral("/gdalplugins"));
    setIfEmpty("PROJ_LIB",         vcpkgShare + QStringLiteral("/proj"));
}

int main(int argc, char *argv[])
{
    // High-DPI rendering handled automatically by Qt 6
    QGuiApplication app(argc, argv);

    setupGdalEnvironment();

    app.setApplicationName(QStringLiteral("Proyectos MDM Móvil"));
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
