import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import QtQuick.Dialogs
import App 1.0

Item {
    id: root

    // Tipado fuerte (mejor validación en carga)
    required property GeoManager geoMgr
    required property QgisProjectGenerator qgisMgr

    property string sourceGpkgPath: root.geoMgr.activeGpkgPath
    property string outputQgzPath: ""

    Connections {
        target: root.geoMgr
        function onActiveGpkgPathChanged() {
            root.sourceGpkgPath = root.geoMgr.activeGpkgPath
        }
    }

    FileDialog {
        id: gpkgSelectDialog
        title: qsTr("Seleccionar GeoPackage de origen")
        nameFilters: [qsTr("GeoPackage (*.gpkg)")]
        fileMode: FileDialog.OpenFile
        onAccepted: root.sourceGpkgPath = root.geoMgr.urlToPath(selectedFile.toString())
    }

    FileDialog {
        id: qgzSaveDialog
        title: qsTr("Guardar proyecto QGIS (.qgz) como…")
        nameFilters: [qsTr("Proyecto QGIS (*.qgz)")]
        fileMode: FileDialog.SaveFile
        defaultSuffix: "qgz"
        onAccepted: root.outputQgzPath = root.geoMgr.urlToPath(selectedFile.toString())
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 12

        Label {
            text: qsTr("Generador de Proyecto QGIS (.qgz)")
            font.pixelSize: 16
            font.weight: Font.Medium
        }

        Button {
            id: generateBtn
            text: root.qgisMgr.busy ? qsTr("Generando…") : qsTr("Generar proyecto .qgz")
            enabled: root.sourceGpkgPath.length > 0 &&
                     root.outputQgzPath.length > 0 &&
                     !root.qgisMgr.busy
            onClicked: root.qgisMgr.generate(
                root.sourceGpkgPath,
                root.outputQgzPath,
                projectNameField.text.trim()
            )
        }

        TextField {
            id: projectNameField
            placeholderText: qsTr("Se usará el nombre del archivo si se deja vacío")
        }

        Label {
            id: resultLabel
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
        }

        Connections {
            target: root.qgisMgr
            function onGenerationSucceeded(path) {
                resultLabel.color = Material.foreground
                resultLabel.text = qsTr("Proyecto generado exitosamente:\n") + path
            }
            function onLastErrorChanged() {
                if (root.qgisMgr.lastError.length > 0) {
                    resultLabel.color = Material.color(Material.Red)
                    resultLabel.text = root.qgisMgr.lastError
                }
            }
        }

        Item { Layout.fillHeight: true }
    }
}