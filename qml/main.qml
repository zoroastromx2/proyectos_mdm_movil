import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import App 1.0

/**
 * main.qml – Application root window.
 *
 * Structure:
 *   ┌─────────────────────────────────────────┐
 *   │  Header bar (title + status indicator)  │
 *   ├──────────────┬──────────────────────────┤
 *   │  Navigation  │  Active panel            │
 *   │  rail        │  (GeoPackage / QGIS)     │
 *   └──────────────┴──────────────────────────┘
 *
 * The C++ singletons GeoManager and QgisProjectGenerator are
 * registered into the "App" URI by qt_add_qml_module and are
 * available as GeoManager { } and QgisProjectGenerator { } objects.
 */

ApplicationWindow {
    id: root

    visible: true
    minimumWidth:  900
    minimumHeight: 620
    title: qsTr("Provectos MDM Móvil")

    Material.theme: Material.Light
    Material.accent: Material.Teal

    // -----------------------------------------------------------------------
    // Singleton instances (registered from C++ via QML_SINGLETON)
    // -----------------------------------------------------------------------
    GeoManager          { id: geoManager }
    QgisProjectGenerator { id: generator }

    // -----------------------------------------------------------------------
    // Status-bar message (shared by both panels via a property)
    // -----------------------------------------------------------------------
    property string statusMessage: ""

    Connections {
        target: geoManager
        function onOperationSucceeded(message) { root.statusMessage = message }
        function onLastErrorChanged()          { root.statusMessage = "⚠ " + geoManager.lastError }
    }
    Connections {
        target: qgisGen
        function onGenerationSucceeded(path)   { root.statusMessage = qsTr("Proyecto generado: ") + path }
        function onLastErrorChanged()          { root.statusMessage = "⚠ " + qgisGen.lastError }
    }

    // -----------------------------------------------------------------------
    // Main layout
    // -----------------------------------------------------------------------
    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // ── Header ──────────────────────────────────────────────────────
        ToolBar {
            Layout.fillWidth: true
            Material.background: Material.Teal

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin:  12
                anchors.rightMargin: 12

                Label {
                    text: qsTr("Provectos MDM Móvil")
                    font.pixelSize: 18
                    font.weight: Font.Medium
                    color: "white"
                }

                Item { Layout.fillWidth: true }

                // Busy spinner
                BusyIndicator {
                    running: geoManager.busy || qgisGen.busy
                    visible: running
                    implicitWidth: 28
                    implicitHeight: 28
                    palette.dark: "white"
                }
            }
        }

        // ── Body: navigation rail + panel ───────────────────────────────
        RowLayout {
            Layout.fillWidth:  true
            Layout.fillHeight: true
            spacing: 0

            // Navigation rail
            Pane {
                Layout.fillHeight: true
                implicitWidth:     120
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
                            required property var    modelData
                            required property int    index

                            Layout.fillWidth: true
                            highlighted: swipeView.currentIndex === index
                            onClicked:   swipeView.currentIndex = index

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

            // Content panels in a SwipeView (tabs controlled by the rail)
            SwipeView {
                id: swipeView
                Layout.fillWidth:  true
                Layout.fillHeight: true
                interactive: false   // navigation via rail only

                GeoPackagePanel   { geoMgr: geoManager }
                QgisGeneratorPanel { geoMgr: geoManager; qgisMgr: qgisGen }
            }
        }

        // ── Status bar ───────────────────────────────────────────────────
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
                color: root.statusMessage.startsWith("⚠") ? Material.color(Material.Red)
                                                           : Material.foreground
            }
        }
    }
}
