import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import App 1.0

ApplicationWindow {
    id: root

    visible: true
    minimumWidth: 900
    minimumHeight: 620
    title: qsTr("Proyectos MDM Móvil 0.0.1")

    Material.theme: Material.Light
    Material.accent: Material.Teal

    // Instancias C++ registradas en App 1.0
    GeoManager { id: geoManager }
    QgisProjectGenerator { id: qgisGen }

    property string statusMessage: ""

    Connections {
        target: geoManager
        function onOperationSucceeded(message) { root.statusMessage = message }
        function onLastErrorChanged() {
            root.statusMessage = geoManager.lastError.length > 0
                               ? "⚠ " + geoManager.lastError
                               : ""
        }
    }

    Connections {
        target: qgisGen
        function onGenerationSucceeded(path) {
            root.statusMessage = qsTr("Proyecto generado: ") + path
        }
        function onLastErrorChanged() {
            root.statusMessage = qgisGen.lastError.length > 0
                               ? "⚠ " + qgisGen.lastError
                               : root.statusMessage
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        ToolBar {
            Layout.fillWidth: true
            Material.background: Material.Teal

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 12

                Label {
                    text: qsTr("Proyectos MDM Móvil")
                    font.pixelSize: 18
                    font.weight: Font.Medium
                    color: "white"
                }

                Item { Layout.fillWidth: true }

                BusyIndicator {
                    running: geoManager.busy || qgisGen.busy
                    visible: running
                    implicitWidth: 28
                    implicitHeight: 28
                    palette.dark: "white"
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            Pane {
                Layout.fillHeight: true
                implicitWidth: 120
                Material.elevation: 2
                padding: 0

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 4

                    Repeater {
                        model: [
                            { label: qsTr("GeoPackage"), icon: "⬡" },
                            { label: qsTr("Proyecto QGIS"), icon: "⬢" }
                        ]

                        delegate: ItemDelegate {
                            required property var modelData
                            required property int index

                            Layout.fillWidth: true
                            highlighted: swipeView.currentIndex === index
                            onClicked: swipeView.currentIndex = index

                            contentItem: ColumnLayout {
                                spacing: 2
                                Label {
                                    text: modelData.icon
                                    font.pixelSize: 22
                                    Layout.alignment: Qt.AlignHCenter
                                }
                                Label {
                                    text: modelData.label
                                    font.pixelSize: 11
                                    wrapMode: Text.WordWrap
                                    horizontalAlignment: Text.AlignHCenter
                                    Layout.fillWidth: true
                                    Layout.alignment: Qt.AlignHCenter
                                }
                            }
                        }
                    }

                    Item { Layout.fillHeight: true }
                }
            }

            SwipeView {
                id: swipeView
                Layout.fillWidth: true
                Layout.fillHeight: true
                interactive: false

                GeoPackagePanel { geoMgr: geoManager }
                QgisGeneratorPanel { geoMgr: geoManager; qgisMgr: qgisGen }
            }
        }

        Pane {
            Layout.fillWidth: true
            implicitHeight: 28
            Material.elevation: 1
            padding: 0
            leftPadding: 12

            Label {
                anchors.verticalCenter: parent.verticalCenter
                text: root.statusMessage
                font.pixelSize: 12
                elide: Text.ElideRight
                width: parent.width - 24
                color: root.statusMessage.startsWith("⚠")
                       ? Material.color(Material.Red)
                       : Material.foreground
            }
        }
    }
}