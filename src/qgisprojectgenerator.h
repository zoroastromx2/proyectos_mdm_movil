#pragma once

#include <QObject>
#include <QString>
#include <QtQml/qqmlregistration.h>
#include <qqml.h>

/**
 * @brief QgisProjectGenerator reads a GeoPackage and produces a QGIS 3.40
 *        project file (.qgz).
 *
 * A .qgz file is simply a ZIP archive containing a single .qgs XML file.
 * The XML follows the QGIS 3.40 project format and correctly references
 * all vector layers found in the GeoPackage via the "ogr" provider.
 *
 * Usage from QML:
 * @code
 *   QgisProjectGenerator.generate("/path/to/data.gpkg", "/output/project.qgz")
 * @endcode
 */
class QgisProjectGenerator : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)
    Q_PROPERTY(bool    busy      READ busy      NOTIFY busyChanged)

public:
    explicit QgisProjectGenerator(QObject *parent = nullptr);
    ~QgisProjectGenerator() override;

    QString lastError() const;
    bool    busy()      const;

    /**
     * @brief Generate a .qgz project file from a GeoPackage.
     *
     * @param gpkgPath   Absolute path to the source GeoPackage file.
     * @param outputPath Absolute path for the output .qgz file (will be
     *                   created or overwritten).
     * @param projectName Human-readable name embedded in the QGIS project.
     *                    Defaults to the output file's base name.
     * @return true on success, false on failure (inspect lastError).
     */
    Q_INVOKABLE bool generate(const QString &gpkgPath,
                              const QString &outputPath,
                              const QString &projectName = QString{});

signals:
    void lastErrorChanged();
    void busyChanged();
    void generationSucceeded(const QString &outputPath);

private:
    // Build the complete QGIS 3.40 XML document as a UTF-8 byte array.
    QByteArray buildQgsXml(const QString &gpkgPath,
                           const QString &projectName);

    // Write xmlContent into a ZIP archive at zipPath under the entry name
    // "project.qgs".
    bool writeZip(const QString    &zipPath,
                  const QByteArray &xmlContent);

    void setLastError(const QString &error);
    void setBusy(bool busy);

    QString m_lastError;
    bool    m_busy{false};
};
