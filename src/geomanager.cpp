#include "geomanager.h"

#include <QFileInfo>
#include <QUrl>

// GDAL / OGR headers
#include <gdal.h>
#include <gdal_priv.h>
#include <ogr_api.h>
#include <ogrsf_frmts.h>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

namespace {

/// RAII wrapper so we never forget GDALClose().
struct GdalDatasetGuard
{
    explicit GdalDatasetGuard(GDALDataset *ds) : m_ds(ds) {}
    ~GdalDatasetGuard() { if (m_ds) GDALClose(m_ds); }

    GDALDataset *get()  const { return m_ds; }
    bool isValid()      const { return m_ds != nullptr; }

    GdalDatasetGuard(const GdalDatasetGuard &)            = delete;
    GdalDatasetGuard &operator=(const GdalDatasetGuard &) = delete;

private:
    GDALDataset *m_ds;
};

} // anonymous namespace

// ---------------------------------------------------------------------------
// Construction / destruction
// ---------------------------------------------------------------------------

GeoManager::GeoManager(QObject *parent)
    : QObject(parent)
{
    GDALAllRegister();
}

GeoManager::~GeoManager() = default;

// ---------------------------------------------------------------------------
// Property accessors
// ---------------------------------------------------------------------------

QString GeoManager::activeGpkgPath() const { return m_activeGpkgPath; }
QStringList GeoManager::layerNames() const { return m_layerNames; }
QString GeoManager::lastError()      const { return m_lastError; }
bool    GeoManager::busy()           const { return m_busy; }

void GeoManager::setActiveGpkgPath(const QString &path)
{
    if (m_activeGpkgPath == path)
        return;
    m_activeGpkgPath = path;
    emit activeGpkgPathChanged();
}

void GeoManager::setLastError(const QString &error)
{
    if (m_lastError == error)
        return;
    m_lastError = error;
    emit lastErrorChanged();
}

void GeoManager::setBusy(bool busy)
{
    if (m_busy == busy)
        return;
    m_busy = busy;
    emit busyChanged();
}

// ---------------------------------------------------------------------------
// Static helpers
// ---------------------------------------------------------------------------

QString GeoManager::urlToPath(const QString &urlString)
{
    // QML FileDialog returns "file:///C:/path/to/file" on Windows and
    // "file:///home/user/file" on Linux.  QUrl handles both correctly.
    QUrl url(urlString);
    if (url.isLocalFile())
        return url.toLocalFile();
    return urlString; // already a plain path
}

QStringList GeoManager::urlListToPaths(const QStringList &urlList)
{
    QStringList paths;
    paths.reserve(urlList.size());
    for (const QString &u : urlList)
        paths << urlToPath(u);
    return paths;
}

// ---------------------------------------------------------------------------
// Internal: reload layer names from disk
// ---------------------------------------------------------------------------

bool GeoManager::reloadLayerNames()
{
    if (m_activeGpkgPath.isEmpty()) {
        m_layerNames.clear();
        emit layerNamesChanged();
        return true;
    }

    GdalDatasetGuard ds{static_cast<GDALDataset *>(
        GDALOpenEx(m_activeGpkgPath.toUtf8().constData(),
                   GDAL_OF_VECTOR | GDAL_OF_READONLY,
                   nullptr, nullptr, nullptr))};

    if (!ds.isValid()) {
        setLastError(tr("Cannot open GeoPackage: %1").arg(m_activeGpkgPath));
        return false;
    }

    QStringList names;
    const int count = ds.get()->GetLayerCount();
    names.reserve(count);
    for (int i = 0; i < count; ++i) {
        OGRLayer *layer = ds.get()->GetLayer(i);
        if (layer)
            names << QString::fromUtf8(layer->GetName());
    }

    m_layerNames = names;
    emit layerNamesChanged();
    return true;
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

bool GeoManager::createGeoPackage(const QStringList &shpPaths,
                                   const QString     &gpkgPath)
{
    if (gpkgPath.isEmpty()) {
        setLastError(tr("Output GeoPackage path is empty."));
        return false;
    }
    if (shpPaths.isEmpty()) {
        setLastError(tr("No Shapefile paths provided."));
        return false;
    }

    setBusy(true);
    setLastError(QString{});

    // Get (or create) the GPKG driver
    GDALDriver *gpkgDriver =
        GetGDALDriverManager()->GetDriverByName("GPKG");
    if (!gpkgDriver) {
        setLastError(tr("GPKG driver not available. Check GDAL installation."));
        setBusy(false);
        return false;
    }

    // Create the new (empty) GeoPackage – truncating any existing file
    GdalDatasetGuard outDS{gpkgDriver->Create(
        gpkgPath.toUtf8().constData(), 0, 0, 0, GDT_Unknown, nullptr)};

    if (!outDS.isValid()) {
        setLastError(tr("Failed to create GeoPackage: %1").arg(gpkgPath));
        setBusy(false);
        return false;
    }

    bool anyImported = false;
    for (const QString &shpPath : shpPaths) {
        GdalDatasetGuard srcDS{static_cast<GDALDataset *>(
            GDALOpenEx(shpPath.toUtf8().constData(),
                       GDAL_OF_VECTOR | GDAL_OF_READONLY,
                       nullptr, nullptr, nullptr))};

        if (!srcDS.isValid()) {
            // Non-fatal: report and continue with remaining files
            setLastError(tr("Cannot open Shapefile: %1 – skipped.").arg(shpPath));
            continue;
        }

        const QString layerName = QFileInfo(shpPath).baseName();

        for (int i = 0; i < srcDS.get()->GetLayerCount(); ++i) {
            OGRLayer *srcLayer = srcDS.get()->GetLayer(i);
            if (!srcLayer)
                continue;

            // Use the shapefile's base name as the layer name in the GPKG
            OGRLayer *newLayer =
                outDS.get()->CopyLayer(srcLayer,
                                       layerName.toUtf8().constData(),
                                       nullptr);
            if (!newLayer) {
                setLastError(tr("Failed to copy layer '%1' from '%2'.")
                                 .arg(layerName, shpPath));
            } else {
                anyImported = true;
            }
        }
    }

    if (!anyImported) {
        setLastError(tr("No layers were imported into the GeoPackage."));
        setBusy(false);
        return false;
    }

    setActiveGpkgPath(gpkgPath);
    reloadLayerNames();
    setBusy(false);

    emit operationSucceeded(tr("GeoPackage created: %1").arg(gpkgPath));
    return true;
}

bool GeoManager::openGeoPackage(const QString &gpkgPath)
{
    if (gpkgPath.isEmpty()) {
        setLastError(tr("GeoPackage path is empty."));
        return false;
    }

    setBusy(true);
    setLastError(QString{});
    setActiveGpkgPath(gpkgPath);

    const bool ok = reloadLayerNames();
    setBusy(false);

    if (ok)
        emit operationSucceeded(tr("Opened: %1").arg(gpkgPath));
    return ok;
}

bool GeoManager::addLayers(const QStringList &shpPaths)
{
    if (m_activeGpkgPath.isEmpty()) {
        setLastError(tr("No GeoPackage is currently open."));
        return false;
    }
    if (shpPaths.isEmpty()) {
        setLastError(tr("No Shapefile paths provided."));
        return false;
    }

    setBusy(true);
    setLastError(QString{});

    GdalDatasetGuard outDS{static_cast<GDALDataset *>(
        GDALOpenEx(m_activeGpkgPath.toUtf8().constData(),
                   GDAL_OF_VECTOR | GDAL_OF_UPDATE,
                   nullptr, nullptr, nullptr))};

    if (!outDS.isValid()) {
        setLastError(tr("Cannot open GeoPackage for editing: %1")
                         .arg(m_activeGpkgPath));
        setBusy(false);
        return false;
    }

    bool anyAdded = false;
    for (const QString &shpPath : shpPaths) {
        GdalDatasetGuard srcDS{static_cast<GDALDataset *>(
            GDALOpenEx(shpPath.toUtf8().constData(),
                       GDAL_OF_VECTOR | GDAL_OF_READONLY,
                       nullptr, nullptr, nullptr))};

        if (!srcDS.isValid()) {
            setLastError(tr("Cannot open Shapefile: %1 – skipped.").arg(shpPath));
            continue;
        }

        const QString layerName = QFileInfo(shpPath).baseName();

        for (int i = 0; i < srcDS.get()->GetLayerCount(); ++i) {
            OGRLayer *srcLayer = srcDS.get()->GetLayer(i);
            if (!srcLayer)
                continue;

            OGRLayer *newLayer =
                outDS.get()->CopyLayer(srcLayer,
                                       layerName.toUtf8().constData(),
                                       nullptr);
            if (!newLayer) {
                setLastError(tr("Failed to add layer '%1'.").arg(layerName));
            } else {
                anyAdded = true;
            }
        }
    }

    reloadLayerNames();
    setBusy(false);

    if (anyAdded)
        emit operationSucceeded(tr("Layers added successfully."));
    return anyAdded;
}

bool GeoManager::removeLayer(const QString &layerName)
{
    if (m_activeGpkgPath.isEmpty()) {
        setLastError(tr("No GeoPackage is currently open."));
        return false;
    }
    if (layerName.isEmpty()) {
        setLastError(tr("Layer name is empty."));
        return false;
    }

    setBusy(true);
    setLastError(QString{});

    GdalDatasetGuard ds{static_cast<GDALDataset *>(
        GDALOpenEx(m_activeGpkgPath.toUtf8().constData(),
                   GDAL_OF_VECTOR | GDAL_OF_UPDATE,
                   nullptr, nullptr, nullptr))};

    if (!ds.isValid()) {
        setLastError(tr("Cannot open GeoPackage for editing: %1")
                         .arg(m_activeGpkgPath));
        setBusy(false);
        return false;
    }

    int targetIndex = -1;
    const int count = ds.get()->GetLayerCount();
    for (int i = 0; i < count; ++i) {
        OGRLayer *layer = ds.get()->GetLayer(i);
        if (layer && QString::fromUtf8(layer->GetName()) == layerName) {
            targetIndex = i;
            break;
        }
    }

    if (targetIndex < 0) {
        setLastError(tr("Layer '%1' not found in GeoPackage.").arg(layerName));
        setBusy(false);
        return false;
    }

    const OGRErr err = ds.get()->DeleteLayer(targetIndex);
    if (err != OGRERR_NONE) {
        setLastError(tr("Failed to delete layer '%1': GDAL error %2.")
                         .arg(layerName).arg(static_cast<int>(err)));
        setBusy(false);
        return false;
    }

    reloadLayerNames();
    setBusy(false);

    emit operationSucceeded(tr("Layer '%1' removed.").arg(layerName));
    return true;
}

void GeoManager::refreshLayers()
{
    setBusy(true);
    reloadLayerNames();
    setBusy(false);
}
