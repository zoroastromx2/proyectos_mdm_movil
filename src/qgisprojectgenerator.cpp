#include "qgisprojectgenerator.h"

#include <QDateTime>
#include <QFileInfo>
#include <QRandomGenerator>
#include <QXmlStreamWriter>

// GDAL / OGR
#include <gdal.h>
#include <gdal_priv.h>
#include <ogrsf_frmts.h>

// libzip
#include <zip.h>

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

namespace {

/// Returns a pseudo-random hex string of @p length characters (uppercase).
QString randomHex(int length)
{
    const QString chars = QStringLiteral("0123456789ABCDEF");
    QString result;
    result.reserve(length);
    for (int i = 0; i < length; ++i)
        result += chars[QRandomGenerator::global()->bounded(16)];
    return result;
}

/// Maps an OGR geometry type to the QGIS geometry string and numeric code.
struct QgisGeomInfo
{
    QString name;   // e.g. "Polygon"
    int     code;   // layerGeometryType: 0=Point,1=Line,2=Polygon,3=Unknown
};

QgisGeomInfo ogrGeomToQgis(OGRwkbGeometryType geomType)
{
    const OGRwkbGeometryType flat = wkbFlatten(geomType);
    switch (flat) {
        case wkbPoint:
        case wkbMultiPoint:
            return { QStringLiteral("Point"),   0 };
        case wkbLineString:
        case wkbMultiLineString:
            return { QStringLiteral("Line"),    1 };
        case wkbPolygon:
        case wkbMultiPolygon:
            return { QStringLiteral("Polygon"), 2 };
        default:
            return { QStringLiteral("Unknown"), 3 };
    }
}

/// Minimal WGS84 SRS block that QGIS 3.x can parse.
void writeWgs84Srs(QXmlStreamWriter &xml)
{
    xml.writeStartElement(QStringLiteral("spatialrefsys"));
    xml.writeAttribute(QStringLiteral("nativeFormat"), QStringLiteral("Wkt"));

    xml.writeTextElement(QStringLiteral("wkt"),
        QStringLiteral("GEOGCS[\"WGS 84\","
                        "DATUM[\"WGS_1984\","
                        "SPHEROID[\"WGS 84\",6378137,298.257223563,"
                        "AUTHORITY[\"EPSG\",\"7030\"]],"
                        "AUTHORITY[\"EPSG\",\"6326\"]],"
                        "PRIMEM[\"Greenwich\",0,AUTHORITY[\"EPSG\",\"8901\"]],"
                        "UNIT[\"degree\",0.0174532925199433,"
                        "AUTHORITY[\"EPSG\",\"9122\"]],"
                        "AUTHORITY[\"EPSG\",\"4326\"]]"));

    xml.writeTextElement(QStringLiteral("proj4"),
        QStringLiteral("+proj=longlat +datum=WGS84 +no_defs"));
    xml.writeTextElement(QStringLiteral("srsid"),    QStringLiteral("3452"));
    xml.writeTextElement(QStringLiteral("srid"),     QStringLiteral("4326"));
    xml.writeTextElement(QStringLiteral("authid"),   QStringLiteral("EPSG:4326"));
    xml.writeTextElement(QStringLiteral("description"),
                         QStringLiteral("WGS 84"));
    xml.writeTextElement(QStringLiteral("projectionacronym"),
                         QStringLiteral("longlat"));
    xml.writeTextElement(QStringLiteral("ellipsoidacronym"),
                         QStringLiteral("EPSG:7030"));
    xml.writeTextElement(QStringLiteral("geographicflag"),
                         QStringLiteral("true"));
    xml.writeEndElement(); // spatialrefsys
}

/// Write the SRS of an OGRLayer (falls back to WGS84 if unavailable).
void writeLayerSrs(QXmlStreamWriter &xml, OGRLayer *layer)
{
    xml.writeStartElement(QStringLiteral("srs"));
    OGRSpatialReference *srs = layer->GetSpatialRef();
    if (srs) {
        char *pszWkt  = nullptr;
        char *pszProj = nullptr;
        srs->exportToWkt(&pszWkt);
        srs->exportToProj4(&pszProj);

        const char *authName = srs->GetAuthorityName(nullptr);
        const char *authCode = srs->GetAuthorityCode(nullptr);

        xml.writeStartElement(QStringLiteral("spatialrefsys"));
        xml.writeAttribute(QStringLiteral("nativeFormat"),
                           QStringLiteral("Wkt"));
        xml.writeTextElement(QStringLiteral("wkt"),
                             pszWkt ? QString::fromUtf8(pszWkt) : QString{});
        xml.writeTextElement(QStringLiteral("proj4"),
                             pszProj ? QString::fromUtf8(pszProj) : QString{});

        if (authName && authCode) {
            const QString authid = QString::fromUtf8(authName) + QLatin1Char(':')
                                   + QString::fromUtf8(authCode);
            xml.writeTextElement(QStringLiteral("authid"), authid);
            xml.writeTextElement(QStringLiteral("description"),
                                 QString::fromUtf8(authName));
        }
        xml.writeEndElement(); // spatialrefsys

        CPLFree(pszWkt);
        CPLFree(pszProj);
    } else {
        writeWgs84Srs(xml);
    }
    xml.writeEndElement(); // srs
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// QgisProjectGenerator implementation
// ---------------------------------------------------------------------------

QgisProjectGenerator::QgisProjectGenerator(QObject *parent)
    : QObject(parent)
{
    GDALAllRegister();
}

QgisProjectGenerator::~QgisProjectGenerator() = default;

QString QgisProjectGenerator::lastError() const { return m_lastError; }
bool    QgisProjectGenerator::busy()      const { return m_busy; }

void QgisProjectGenerator::setLastError(const QString &error)
{
    if (m_lastError == error) return;
    m_lastError = error;
    emit lastErrorChanged();
}

void QgisProjectGenerator::setBusy(bool busy)
{
    if (m_busy == busy) return;
    m_busy = busy;
    emit busyChanged();
}

// ---------------------------------------------------------------------------
// generate()
// ---------------------------------------------------------------------------

bool QgisProjectGenerator::generate(const QString &gpkgPath,
                                     const QString &outputPath,
                                     const QString &projectName)
{
    if (gpkgPath.isEmpty()) {
        setLastError(tr("GeoPackage path is empty."));
        return false;
    }
    if (outputPath.isEmpty()) {
        setLastError(tr("Output path is empty."));
        return false;
    }

    setBusy(true);
    setLastError(QString{});

    const QString name = projectName.isEmpty()
                             ? QFileInfo(outputPath).baseName()
                             : projectName;

    const QByteArray xml = buildQgsXml(gpkgPath, name);
    if (xml.isEmpty()) {
        // lastError was set inside buildQgsXml
        setBusy(false);
        return false;
    }

    if (!writeZip(outputPath, xml)) {
        setBusy(false);
        return false;
    }

    setBusy(false);
    emit generationSucceeded(outputPath);
    return true;
}

// ---------------------------------------------------------------------------
// buildQgsXml()  – produces a QGIS 3.40-compatible project XML
// ---------------------------------------------------------------------------

QByteArray QgisProjectGenerator::buildQgsXml(const QString &gpkgPath,
                                              const QString &projectName)
{
    // Open the GeoPackage read-only
    GDALDataset *ds = static_cast<GDALDataset *>(
        GDALOpenEx(gpkgPath.toUtf8().constData(),
                   GDAL_OF_VECTOR | GDAL_OF_READONLY,
                   nullptr, nullptr, nullptr));

    if (!ds) {
        setLastError(tr("Cannot open GeoPackage: %1").arg(gpkgPath));
        return {};
    }

    // Collect layer info before writing XML
    struct LayerInfo {
        QString         id;
        QString         name;
        QString         datasource;
        QgisGeomInfo    geom;
        OGRLayer       *ogrLayer; // borrowed – valid while ds is open
    };

    QVector<LayerInfo> layers;
    const int layerCount = ds->GetLayerCount();
    for (int i = 0; i < layerCount; ++i) {
        OGRLayer *ogrLayer = ds->GetLayer(i);
        if (!ogrLayer)
            continue;

        LayerInfo info;
        info.name       = QString::fromUtf8(ogrLayer->GetName());
        info.id         = info.name + QLatin1Char('_') + randomHex(16);
        // QGIS datasource format for GeoPackage layers:
        info.datasource = gpkgPath + QStringLiteral("|layername=") + info.name;
        info.geom       = ogrGeomToQgis(ogrLayer->GetGeomType());
        info.ogrLayer   = ogrLayer;
        layers.append(info);
    }

    if (layers.isEmpty()) {
        setLastError(tr("GeoPackage contains no vector layers: %1").arg(gpkgPath));
        GDALClose(ds);
        return {};
    }

    // ------------------------------------------------------------------
    // Build the XML document in memory
    // ------------------------------------------------------------------
    QByteArray   buffer;
    QXmlStreamWriter xml(&buffer);
    xml.setAutoFormatting(true);
    xml.setAutoFormattingIndent(2);

    xml.writeStartDocument();

    // QGIS DOCTYPE declaration expected by the application
    xml.writeDTD(QStringLiteral("<!DOCTYPE qgis PUBLIC 'http://mrcc.com/qgis.dtd' 'SYSTEM'>"));

    // Root element
    xml.writeStartElement(QStringLiteral("qgis"));
    xml.writeAttribute(QStringLiteral("version"),
                       QStringLiteral("3.40.0-Bratislava"));
    xml.writeAttribute(QStringLiteral("saveDateTime"),
        QDateTime::currentDateTime().toString(Qt::ISODate));
    xml.writeAttribute(QStringLiteral("saveNumber"),
                       QStringLiteral("1"));
    xml.writeAttribute(QStringLiteral("projectname"), projectName);

    // ------------------------------------------------------------------
    // Metadata
    // ------------------------------------------------------------------
    xml.writeTextElement(QStringLiteral("homePath"),   QString{});
    xml.writeTextElement(QStringLiteral("title"),      projectName);

    xml.writeStartElement(QStringLiteral("autotransaction"));
    xml.writeAttribute(QStringLiteral("active"), QStringLiteral("0"));
    xml.writeEndElement();

    xml.writeStartElement(QStringLiteral("evaluateDefaultValues"));
    xml.writeAttribute(QStringLiteral("active"), QStringLiteral("0"));
    xml.writeEndElement();

    xml.writeStartElement(QStringLiteral("trust"));
    xml.writeAttribute(QStringLiteral("active"), QStringLiteral("0"));
    xml.writeEndElement();

    // ------------------------------------------------------------------
    // Project CRS (WGS 84 by default)
    // ------------------------------------------------------------------
    xml.writeStartElement(QStringLiteral("projectCrs"));
    writeWgs84Srs(xml);
    xml.writeEndElement(); // projectCrs

    // ------------------------------------------------------------------
    // Layer-tree (controls the Layers panel in QGIS)
    // ------------------------------------------------------------------
    xml.writeStartElement(QStringLiteral("layer-tree-group"));
    {
        xml.writeStartElement(QStringLiteral("custom-properties"));
        xml.writeEndElement();

        for (const LayerInfo &info : layers) {
            xml.writeStartElement(QStringLiteral("layer-tree-layer"));
            xml.writeAttribute(QStringLiteral("id"),          info.id);
            xml.writeAttribute(QStringLiteral("name"),        info.name);
            xml.writeAttribute(QStringLiteral("checked"),
                               QStringLiteral("Qt::Checked"));
            xml.writeAttribute(QStringLiteral("expanded"),    QStringLiteral("1"));
            xml.writeAttribute(QStringLiteral("source"),      info.datasource);
            xml.writeAttribute(QStringLiteral("providerKey"), QStringLiteral("ogr"));

            xml.writeStartElement(QStringLiteral("customproperties"));
            xml.writeEndElement();

            xml.writeEndElement(); // layer-tree-layer
        }
    }
    xml.writeEndElement(); // layer-tree-group

    // ------------------------------------------------------------------
    // Snapping settings (required by QGIS 3.x even if empty)
    // ------------------------------------------------------------------
    xml.writeStartElement(QStringLiteral("snapping_settings"));
    xml.writeAttribute(QStringLiteral("enabled"),          QStringLiteral("0"));
    xml.writeAttribute(QStringLiteral("mode"),             QStringLiteral("2"));
    xml.writeAttribute(QStringLiteral("type"),             QStringLiteral("1"));
    xml.writeAttribute(QStringLiteral("tolerance"),        QStringLiteral("12"));
    xml.writeAttribute(QStringLiteral("unit"),             QStringLiteral("1"));
    xml.writeAttribute(QStringLiteral("maxScale"),         QStringLiteral("0"));
    xml.writeAttribute(QStringLiteral("minScale"),         QStringLiteral("0"));
    xml.writeAttribute(QStringLiteral("scaleDependencyMode"), QStringLiteral("0"));
    xml.writeAttribute(QStringLiteral("intersectionSnapping"), QStringLiteral("0"));
    xml.writeAttribute(QStringLiteral("self_snapping"),    QStringLiteral("0"));
    xml.writeEndElement();

    xml.writeStartElement(QStringLiteral("relations"));
    xml.writeEndElement();
    xml.writeStartElement(QStringLiteral("polymorphicRelations"));
    xml.writeEndElement();

    // ------------------------------------------------------------------
    // Map canvas (extent / CRS for the default view)
    // ------------------------------------------------------------------
    xml.writeStartElement(QStringLiteral("mapcanvas"));
    xml.writeAttribute(QStringLiteral("annotationsVisible"), QStringLiteral("1"));
    xml.writeAttribute(QStringLiteral("name"),               QStringLiteral("theMapCanvas"));
    {
        xml.writeTextElement(QStringLiteral("units"),    QStringLiteral("degrees"));
        xml.writeStartElement(QStringLiteral("extent"));
        xml.writeTextElement(QStringLiteral("xmin"),    QStringLiteral("-180"));
        xml.writeTextElement(QStringLiteral("ymin"),    QStringLiteral("-90"));
        xml.writeTextElement(QStringLiteral("xmax"),    QStringLiteral("180"));
        xml.writeTextElement(QStringLiteral("ymax"),    QStringLiteral("90"));
        xml.writeEndElement(); // extent
        xml.writeTextElement(QStringLiteral("rotation"), QStringLiteral("0"));
        xml.writeStartElement(QStringLiteral("destinationsrs"));
        writeWgs84Srs(xml);
        xml.writeEndElement(); // destinationsrs
        xml.writeTextElement(QStringLiteral("rendermaptile"), QStringLiteral("0"));
    }
    xml.writeEndElement(); // mapcanvas

    // ------------------------------------------------------------------
    // Legend
    // ------------------------------------------------------------------
    xml.writeStartElement(QStringLiteral("legend"));
    xml.writeAttribute(QStringLiteral("updateDrawingOrder"), QStringLiteral("true"));
    xml.writeEndElement();

    xml.writeStartElement(QStringLiteral("mapViewDocks"));
    xml.writeEndElement();

    // ------------------------------------------------------------------
    // Project layers
    // ------------------------------------------------------------------
    xml.writeStartElement(QStringLiteral("projectlayers"));
    for (const LayerInfo &info : layers) {
        xml.writeStartElement(QStringLiteral("maplayer"));
        xml.writeAttribute(QStringLiteral("type"),     QStringLiteral("vector"));
        xml.writeAttribute(QStringLiteral("geometry"), info.geom.name);
        xml.writeAttribute(QStringLiteral("autoRefreshEnabled"), QStringLiteral("0"));
        xml.writeAttribute(QStringLiteral("autoRefreshTime"),    QStringLiteral("0"));
        xml.writeAttribute(QStringLiteral("hasScaleBasedVisibilityFlag"), QStringLiteral("0"));
        xml.writeAttribute(QStringLiteral("refreshOnNotifyEnabled"), QStringLiteral("0"));
        xml.writeAttribute(QStringLiteral("maxScale"), QStringLiteral("0"));
        xml.writeAttribute(QStringLiteral("minScale"), QStringLiteral("100000000"));

        xml.writeTextElement(QStringLiteral("id"),          info.id);
        xml.writeTextElement(QStringLiteral("datasource"),  info.datasource);
        xml.writeTextElement(QStringLiteral("layername"),   info.name);

        xml.writeStartElement(QStringLiteral("keywordList"));
        xml.writeTextElement(QStringLiteral("value"), QString{});
        xml.writeEndElement();

        writeLayerSrs(xml, info.ogrLayer);

        xml.writeStartElement(QStringLiteral("provider"));
        xml.writeAttribute(QStringLiteral("encoding"), QStringLiteral("UTF-8"));
        xml.writeCharacters(QStringLiteral("ogr"));
        xml.writeEndElement();

        xml.writeStartElement(QStringLiteral("vectorjoins"));
        xml.writeEndElement();

        xml.writeTextElement(QStringLiteral("layerGeometryType"),
                             QString::number(info.geom.code));

        xml.writeEndElement(); // maplayer
    }
    xml.writeEndElement(); // projectlayers

    // ------------------------------------------------------------------
    // Layer order
    // ------------------------------------------------------------------
    xml.writeStartElement(QStringLiteral("layerorder"));
    for (const LayerInfo &info : layers) {
        xml.writeStartElement(QStringLiteral("layer"));
        xml.writeAttribute(QStringLiteral("id"), info.id);
        xml.writeEndElement();
    }
    xml.writeEndElement(); // layerorder

    // ------------------------------------------------------------------
    // Remaining required top-level elements
    // ------------------------------------------------------------------
    xml.writeStartElement(QStringLiteral("properties"));
    xml.writeEndElement();

    xml.writeStartElement(QStringLiteral("dataDefinedServerProperties"));
    xml.writeEndElement();

    xml.writeStartElement(QStringLiteral("visibility-presets"));
    xml.writeEndElement();

    xml.writeStartElement(QStringLiteral("transformContext"));
    xml.writeEndElement();

    xml.writeEndElement(); // qgis
    xml.writeEndDocument();

    GDALClose(ds);
    return buffer;
}

// ---------------------------------------------------------------------------
// writeZip() – pack xmlContent as "project.qgs" inside a new ZIP archive
// ---------------------------------------------------------------------------

bool QgisProjectGenerator::writeZip(const QString    &zipPath,
                                     const QByteArray &xmlContent)
{
    int errCode = 0;
    zip_t *archive = zip_open(zipPath.toUtf8().constData(),
                              ZIP_CREATE | ZIP_TRUNCATE,
                              &errCode);
    if (!archive) {
        zip_error_t zipErr;
        zip_error_init_with_code(&zipErr, errCode);
        setLastError(tr("Cannot create ZIP archive '%1': %2")
                         .arg(zipPath,
                              QString::fromUtf8(zip_error_strerror(&zipErr))));
        zip_error_fini(&zipErr);
        return false;
    }

    // zip_source_buffer does NOT take ownership of the buffer; the buffer
    // must stay alive until zip_close().
    zip_source_t *source = zip_source_buffer(archive,
                                             xmlContent.constData(),
                                             static_cast<zip_uint64_t>(xmlContent.size()),
                                             0 /* do not free */);
    if (!source) {
        setLastError(tr("Failed to create zip source: %1")
                         .arg(QString::fromUtf8(zip_strerror(archive))));
        zip_close(archive);
        return false;
    }

    if (zip_file_add(archive, "project.qgs", source, ZIP_FL_OVERWRITE) < 0) {
        setLastError(tr("Failed to add file to ZIP: %1")
                         .arg(QString::fromUtf8(zip_strerror(archive))));
        zip_source_free(source);
        zip_close(archive);
        return false;
    }

    if (zip_close(archive) != 0) {
        // Note: zip_close frees the archive even on error.
        setLastError(tr("Failed to close ZIP archive '%1'.").arg(zipPath));
        return false;
    }

    return true;
}
