#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

/**
 * @brief GeoManager handles all GeoPackage (GPKG) CRUD operations via GDAL/OGR.
 *
 * This class is exposed as a QML singleton so that the UI can:
 *   - Create a new GeoPackage from one or more Shapefiles.
 *   - Open an existing GeoPackage and inspect its layers.
 *   - Add new Shapefile layers to an open GeoPackage.
 *   - Remove individual layers from a GeoPackage.
 *
 * All file paths accepted/returned by this class use the local filesystem
 * notation understood by GDAL (i.e. plain Windows/Unix absolute paths, NOT
 * QUrl "file:///" URIs – the QML side must strip the scheme before calling).
 */
class GeoManager : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QString  activeGpkgPath READ activeGpkgPath WRITE setActiveGpkgPath
               NOTIFY activeGpkgPathChanged)
    Q_PROPERTY(QStringList layerNames  READ layerNames     NOTIFY layerNamesChanged)
    Q_PROPERTY(QString  lastError      READ lastError      NOTIFY lastErrorChanged)
    Q_PROPERTY(bool     busy           READ busy           NOTIFY busyChanged)

public:
    explicit GeoManager(QObject *parent = nullptr);
    ~GeoManager() override;

    // ----- Properties -----
    QString     activeGpkgPath() const;
    QStringList layerNames()     const;
    QString     lastError()      const;
    bool        busy()           const;

    void setActiveGpkgPath(const QString &path);

    // ----- Invokable API (called from QML) -----

    /**
     * @brief Create a brand-new GeoPackage at @p gpkgPath and import all
     *        Shapefiles in @p shpPaths as individual layers.
     * @return true on success, false on failure (see lastError for details).
     */
    Q_INVOKABLE bool createGeoPackage(const QStringList &shpPaths,
                                      const QString     &gpkgPath);

    /**
     * @brief Open an existing GeoPackage and populate the layerNames list.
     * @return true on success, false on failure.
     */
    Q_INVOKABLE bool openGeoPackage(const QString &gpkgPath);

    /**
     * @brief Append new layers from @p shpPaths into the currently active GPKG.
     * @return true on success, false on failure.
     */
    Q_INVOKABLE bool addLayers(const QStringList &shpPaths);

    /**
     * @brief Delete the layer named @p layerName from the active GPKG.
     * @return true on success, false on failure.
     */
    Q_INVOKABLE bool removeLayer(const QString &layerName);

    /**
     * @brief Refresh the layerNames list from the file on disk.
     */
    Q_INVOKABLE void refreshLayers();

    /**
     * @brief Strip the "file:///" (or "file://") scheme from a QUrl string
     *        returned by QML's FileDialog so GDAL can open it directly.
     */
    Q_INVOKABLE static QString urlToPath(const QString &urlString);

    /**
     * @brief Return file-level metadata about the active GPKG as a map.
     *        Keys: path, sizeBytes, lastModified, totalLayers.
     */
    Q_INVOKABLE QVariantMap getGpkgFileInfo();

    /**
     * @brief Return detailed info for every layer in the active GPKG.
     *        Each entry is a map with keys: name, geomType, featureCount,
     *        crsAuth, crsWkt, minX, minY, maxX, maxY.
     */
    Q_INVOKABLE QVariantList getAllLayerInfo();

    /**
     * @brief Convert a list of QUrl strings to plain paths (convenience helper).
     */
    Q_INVOKABLE static QStringList urlListToPaths(const QStringList &urlList);

signals:
    void activeGpkgPathChanged();
    void layerNamesChanged();
    void lastErrorChanged();
    void busyChanged();

    /** Emitted after a successful write operation so the UI can show feedback. */
    void operationSucceeded(const QString &message);

private:
    // Reload layer names from m_activeGpkgPath without emitting busyChanged
    bool reloadLayerNames();

    void setLastError(const QString &error);
    void setBusy(bool busy);

    QString     m_activeGpkgPath;
    QStringList m_layerNames;
    QString     m_lastError;
    bool        m_busy{false};
};
