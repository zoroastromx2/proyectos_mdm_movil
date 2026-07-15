import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import QtQuick.Dialogs

/**
 * QgisGeneratorPanel.qml
 *
 * Allows the user to:
 *   1. Select (or reuse the active) GeoPackage as source.
 *   2. Choose an output path for the .qgz file.
 *   3. Optionally set a project name.
 *   4. Trigger generation and see progress / result.
 */
Item {
    id: root

    required property var geoMgr    // GeoManager singleton
    required property var qgisMgr   // QgisProjectGenerator singleton

    // -----------------------------------------------------------------------
    // Local state
    // -----------------------------------------------------------------------
    property string sourceGpkgPath:  root.geoMgr.activeGpkgPath
    property string outputQgzPath:   ""

    // Keep sourceGpkgPath in sync when the user changes the active GPKG
    Connections {
        target: root.geoMgr
        function onActiveGpkgPathChanged() {
            root.sourceGpkgPath = root.geoMgr.activeGpkgPath
        }
    }

    // -----------------------------------------------------------------------
    // File dialogs
    // -----------------------------------------------------------------------

    FileDialog {
        id: gpkgSelectDialog
        title:       qsTr("Seleccionar GeoPackage de origen")
        nameFilters: [ qsTr("GeoPackage (*.gpkg)") ]
        fileMode:    FileDialog.OpenFile

        onAccepted: {
            root.sourceGpkgPath =
                root.geoMgr.urlToPath(selectedFile.toString())
        }
    }

    FileDialog {
        id: qgzSaveDialog
        title:         qsTr("Guardar proyecto QGIS (.qgz) como…")
        nameFilters:   [ qsTr("Proyecto QGIS (*.qgz)") ]
        fileMode:      FileDialog.SaveFile
        defaultSuffix: "qgz"

        onAccepted: {
            root.outputQgzPath =
                root.geoMgr.urlToPath(selectedFile.toString())
        }
    }

    // -----------------------------------------------------------------------
    // UI
    // -----------------------------------------------------------------------
    ColumnLayout {
        anchors.fill:    parent
        anchors.margins: 16
        spacing:         12

        // ── Title ─────────────────────────────────────────────────────────
        Label {
            text: qsTr("Generador de Proyecto QGIS (.qgz)")
            font.pixelSize: 16
            font.weight: Font.Medium
        }

        Label {
            text: qsTr("Genera un archivo de proyecto QGIS 3.40 (.qgz) a partir de un GeoPackage existente.\nTodas las capas vectoriales serán vinculadas automáticamente.")
            wrapMode: Text.WordWrap
            color: Material.color(Material.Grey, Material.Shade700)
            Layout.fillWidth: true
        }

        // ── GeoPackage de origen ─────────────────────────────────────────
        GroupBox {
            title: qsTr("GeoPackage de origen")
            Layout.fillWidth: true

            ColumnLayout {
                anchors.fill: parent
                spacing: 8

                RowLayout {
                    spacing: 8
                    Layout.fillWidth: true

                    Button {
                        text: qsTr("Seleccionar .gpkg…")
                        onClicked: gpkgSelectDialog.open()
                    }

                    // Use the currently active GPKG if already opened
                    Button {
                        text: qsTr("Usar el activo")
                        flat: true
                        enabled: root.geoMgr.activeGpkgPath.length > 0 &&
                                 root.geoMgr.activeGpkgPath !== root.sourceGpkgPath
                        onClicked: root.sourceGpkgPath = root.geoMgr.activeGpkgPath
                        ToolTip.text: root.geoMgr.activeGpkgPath
                        ToolTip.visible: hovered && enabled
                        ToolTip.delay: 400
                    }
                }

                Label {
                    text: root.sourceGpkgPath.length > 0
                          ? root.sourceGpkgPath
                          : qsTr("Ningún GeoPackage seleccionado.")
                    font.italic:  root.sourceGpkgPath.length === 0
                    color: root.sourceGpkgPath.length === 0
                           ? Material.color(Material.Grey)
                           : Material.foreground
                    elide: Text.ElideLeft
                    Layout.fillWidth: true
                }
            }
        }

        // ── Nombre del proyecto ─────────────────────────────────────────
        GroupBox {
            title: qsTr("Nombre del proyecto (opcional)")
            Layout.fillWidth: true

            TextField {
                id: projectNameField
                width: parent.width
                placeholderText: qsTr("Se usará el nombre del archivo si se deja vacío")
                selectByMouse: true
            }
        }

        // ── Archivo de salida ────────────────────────────────────────────
        GroupBox {
            title: qsTr("Archivo de salida (.qgz)")
            Layout.fillWidth: true

            ColumnLayout {
                anchors.fill: parent
                spacing: 8

                RowLayout {
                    spacing: 8
                    Layout.fillWidth: true

                    Button {
                        text: qsTr("Seleccionar destino…")
                        onClicked: qgzSaveDialog.open()
                    }
                }

                Label {
                    text: root.outputQgzPath.length > 0
                          ? root.outputQgzPath
                          : qsTr("Ningún destino seleccionado.")
                    font.italic:  root.outputQgzPath.length === 0
                    color: root.outputQgzPath.length === 0
                           ? Material.color(Material.Grey)
                           : Material.foreground
                    elide: Text.ElideLeft
                    Layout.fillWidth: true
                }
            }
        }

        // ── Generate button ──────────────────────────────────────────────
        Button {
            id: generateBtn
            text: root.qgisMgr.busy ? qsTr("Generando…") : qsTr("Generar proyecto .qgz")
            Material.background: Material.Teal
            Material.foreground: "white"
            Layout.alignment: Qt.AlignRight
            enabled: root.sourceGpkgPath.length > 0 &&
                     root.outputQgzPath.length  > 0 &&
                     !root.qgisMgr.busy

            onClicked: {
                root.qgisMgr.generate(
                    root.sourceGpkgPath,
                    root.outputQgzPath,
                    projectNameField.text.trim())
            }
        }

        // ── Result section ────────────────────────────────────────────────
        Pane {
            id: resultPane
            Layout.fillWidth: true
            visible: resultLabel.text.length > 0
            Material.elevation: 1

            RowLayout {
                anchors.fill: parent
                spacing: 8

                Label {
                    id: resultIcon
                    font.pixelSize: 18
                }

                Label {
                    id: resultLabel
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }
            }
        }

        // Wire generation events to the result pane
        Connections {
            target: root.qgisMgr
            function onGenerationSucceeded(path) {
                resultIcon.text  = "✓"
                resultIcon.color = Material.color(Material.Green)
                resultLabel.text = qsTr("Proyecto generado exitosamente:\n") + path
                resultLabel.color = Material.foreground
            }
            function onLastErrorChanged() {
                if (root.qgisMgr.lastError.length > 0) {
                    resultIcon.text   = "⚠"
                    resultIcon.color  = Material.color(Material.Red)
                    resultLabel.text  = root.qgisMgr.lastError
                    resultLabel.color = Material.color(Material.Red)
                }
            }
        }

        // Spacer
        Item { Layout.fillHeight: true }

        // ── Info footer ───────────────────────────────────────────────────
        Label {
            text: qsTr("El archivo .qgz generado es compatible con QGIS 3.40 (Bratislava) y versiones posteriores.")
            font.pixelSize: 11
            color: Material.color(Material.Grey)
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }
    }
}
