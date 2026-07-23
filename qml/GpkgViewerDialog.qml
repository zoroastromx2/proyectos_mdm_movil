import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import App 1.0

Dialog {
    id: root

    required property GeoManager geoMgr

    title: qsTr("Visor de GeoPackage")
    modal: true
    closePolicy: Dialog.CloseOnEscape | Dialog.CloseOnPressOutside
    anchors.centerIn: Overlay.overlay
    width: Math.min(parent ? parent.width * 0.85 : 800, 960)
    height: Math.min(parent ? parent.height * 0.85 : 600, 700)

    property var fileInfo: ({})
    property var layerList: []

    onOpened: {
        root.fileInfo = root.geoMgr.getGpkgFileInfo()
        root.layerList = root.geoMgr.getAllLayerInfo()
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 12

        // ── File info ────────────────────────────────────────────────
        GroupBox {
            title: qsTr("Información del archivo")
            Layout.fillWidth: true

            GridLayout {
                columns: 2
                columnSpacing: 16
                rowSpacing: 6
                Layout.fillWidth: true

                Label { text: qsTr("Ruta:"); font.weight: Font.Medium }
                Label {
                    text: root.fileInfo.path || ""
                    Layout.fillWidth: true
                    elide: Text.ElideLeft
                }

                Label { text: qsTr("Tamaño:"); font.weight: Font.Medium }
                Label {
                    text: root.fileInfo.sizeBytes
                          ? formatBytes(root.fileInfo.sizeBytes) : ""
                }

                Label { text: qsTr("Modificado:"); font.weight: Font.Medium }
                Label { text: root.fileInfo.lastModified || "" }

                Label { text: qsTr("Capas:"); font.weight: Font.Medium }
                Label { text: root.fileInfo.totalLayers ?? "" }
            }
        }

        // ── Layer table ──────────────────────────────────────────────
        GroupBox {
            title: qsTr("Capas (%1)").arg(root.layerList.length)
            Layout.fillWidth: true
            Layout.fillHeight: true

            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 4

                // Header row
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 4

                    Rectangle {
                        Layout.fillWidth: true; Layout.preferredWidth: 30
                        height: 28
                        color: Material.color(Material.Grey, Material.Shade200)
                        radius: 2
                        Label { anchors.fill: parent; anchors.leftMargin: 6; text: qsTr("Nombre"); font.pixelSize: 12; font.weight: Font.Medium; verticalAlignment: Text.AlignVCenter; elide: Text.ElideRight }
                    }
                    Rectangle {
                        Layout.fillWidth: true; Layout.preferredWidth: 14
                        height: 28
                        color: Material.color(Material.Grey, Material.Shade200)
                        radius: 2
                        Label { anchors.fill: parent; anchors.leftMargin: 6; text: qsTr("Tipo"); font.pixelSize: 12; font.weight: Font.Medium; verticalAlignment: Text.AlignVCenter; elide: Text.ElideRight }
                    }
                    Rectangle {
                        Layout.fillWidth: true; Layout.preferredWidth: 20
                        height: 28
                        color: Material.color(Material.Grey, Material.Shade200)
                        radius: 2
                        Label { anchors.fill: parent; anchors.leftMargin: 6; text: qsTr("CRS"); font.pixelSize: 12; font.weight: Font.Medium; verticalAlignment: Text.AlignVCenter; elide: Text.ElideRight }
                    }
                    Rectangle {
                        Layout.fillWidth: true; Layout.preferredWidth: 10
                        height: 28
                        color: Material.color(Material.Grey, Material.Shade200)
                        radius: 2
                        Label { anchors.fill: parent; anchors.leftMargin: 6; text: qsTr("Features"); font.pixelSize: 12; font.weight: Font.Medium; verticalAlignment: Text.AlignVCenter; elide: Text.ElideRight }
                    }
                    Rectangle {
                        Layout.fillWidth: true; Layout.preferredWidth: 26
                        height: 28
                        color: Material.color(Material.Grey, Material.Shade200)
                        radius: 2
                        Label { anchors.fill: parent; anchors.leftMargin: 6; text: qsTr("Extensión"); font.pixelSize: 12; font.weight: Font.Medium; verticalAlignment: Text.AlignVCenter; elide: Text.ElideRight }
                    }
                }

                // Data rows
                ListView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    model: root.layerList

                    ScrollBar.vertical: ScrollBar {}

                    delegate: ItemDelegate {
                        id: layerRow
                        required property var modelData
                        required property int index

                        width: ListView.view.width
                        height: 32

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 4
                            spacing: 4

                            Label {
                                text: layerRow.modelData.name || ""
                                Layout.preferredWidth: parent.width * 0.30
                                font.pixelSize: 12
                                elide: Text.ElideRight
                                verticalAlignment: Text.AlignVCenter
                            }
                            Label {
                                text: layerRow.modelData.geomType || ""
                                Layout.preferredWidth: parent.width * 0.14
                                font.pixelSize: 12
                                elide: Text.ElideRight
                                verticalAlignment: Text.AlignVCenter
                            }
                            Label {
                                text: layerRow.modelData.crsAuth || ""
                                Layout.preferredWidth: parent.width * 0.20
                                font.pixelSize: 12
                                elide: Text.ElideRight
                                verticalAlignment: Text.AlignVCenter
                            }
                            Label {
                                text: layerRow.modelData.featureCount
                                      ? Number(layerRow.modelData.featureCount).toLocaleString() : "0"
                                Layout.preferredWidth: parent.width * 0.10
                                font.pixelSize: 12
                                horizontalAlignment: Text.AlignRight
                                verticalAlignment: Text.AlignVCenter
                            }
                            Label {
                                text: layerRow.modelData.minX !== undefined
                                      ? qsTr("%1, %2").arg(
                                            Number(layerRow.modelData.minX).toLocaleString(undefined, 'fixed', 2)).arg(
                                            Number(layerRow.modelData.minY).toLocaleString(undefined, 'fixed', 2))
                                      : ""
                                Layout.preferredWidth: parent.width * 0.26
                                font.pixelSize: 11
                                elide: Text.ElideRight
                                verticalAlignment: Text.AlignVCenter
                            }
                        }

                        Rectangle {
                            anchors.bottom: parent.bottom
                            anchors.left: parent.left
                            anchors.right: parent.right
                            height: 1
                            color: Material.color(Material.Grey, Material.Shade200)
                            visible: layerRow.index < root.layerList.length - 1
                        }
                    }
                }
            }
        }

        // ── Close button ─────────────────────────────────────────────
        Button {
            text: qsTr("Cerrar")
            Layout.alignment: Qt.AlignHCenter
            onClicked: root.close()
        }
    }

    // Helper
    function formatBytes(bytes) {
        if (bytes < 1024) return bytes + " B"
        if (bytes < 1048576) return (bytes / 1024).toFixed(1) + " KB"
        if (bytes < 1073741824) return (bytes / 1048576).toFixed(1) + " MB"
        return (bytes / 1073741824).toFixed(2) + " GB"
    }
}