import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: root
    width: 1180
    height: 760
    minimumWidth: 980
    minimumHeight: 640
    visible: true
    title: "SoftLoaf Trichrome"
    color: "#111315"

    palette.window: "#111315"
    palette.windowText: "#f4f0e8"
    palette.base: "#171a1d"
    palette.text: "#f4f0e8"
    palette.button: "#202429"
    palette.buttonText: "#f4f0e8"
    palette.highlight: "#4cbf88"

    ExportWindow {
        id: exportWindow
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.preferredWidth: 340
            Layout.fillHeight: true
            color: "#1a1d20"
            border.color: "#343a40"
            border.width: 1

            ColumnLayout {
                anchors.fill: parent
                spacing: 0

                Item {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 88

                    Column {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.leftMargin: 20
                        anchors.rightMargin: 20
                        spacing: 6

                        Text {
                            text: "SoftLoaf Trichrome"
                            color: "#f4f0e8"
                            font.pixelSize: 21
                            font.bold: true
                        }

                        Text {
                            text: trichromeController.status
                            color: "#aab2bb"
                            font.pixelSize: 13
                            elide: Text.ElideRight
                            width: parent.width
                        }
                    }
                }

                Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: "#343a40" }

                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.margins: 16
                    spacing: 10

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        ThemedButton {
                            Layout.fillWidth: true
                            text: "Import"
                            enabled: !trichromeController.busy
                            onClicked: trichromeController.chooseImport()
                        }
                    }

                    Label {
                        text: "Sensor"
                        color: "#aab2bb"
                        font.pixelSize: 12
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        ThemedButton {
                            Layout.fillWidth: true
                            text: "Bayer"
                            checkable: true
                            checked: trichromeController.sensorMode === "bayer"
                            onClicked: trichromeController.sensorMode = "bayer"
                        }

                        ThemedButton {
                            Layout.fillWidth: true
                            text: "Monochrome"
                            checkable: true
                            checked: trichromeController.sensorMode === "mono"
                            onClicked: trichromeController.sensorMode = "mono"
                        }
                    }

                    GridLayout {
                        Layout.fillWidth: true
                        columns: 2
                        columnSpacing: 10
                        rowSpacing: 10

                        Label {
                            text: "Grouping"
                            color: "#aab2bb"
                            font.pixelSize: 12
                        }

                        ThemedComboBox {
                            Layout.fillWidth: true
                            model: [
                                { text: "Filename", value: "filename" },
                                { text: "Selection", value: "selection" }
                            ]
                            textRole: "text"
                            valueRole: "value"
                            currentIndex: trichromeController.sortMode === "selection" ? 1 : 0
                            onActivated: trichromeController.sortMode = currentValue
                        }

                        Label {
                            text: "Order"
                            color: "#aab2bb"
                            font.pixelSize: 12
                        }

                        ThemedComboBox {
                            Layout.fillWidth: true
                            model: ["RGB", "RBG", "GRB", "GBR", "BRG", "BGR"]
                            currentIndex: model.indexOf(trichromeController.roleOrder)
                            onActivated: trichromeController.roleOrder = currentText
                        }
                    }
                }

                Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: "#343a40" }

                RowLayout {
                    Layout.fillWidth: true
                    Layout.leftMargin: 16
                    Layout.rightMargin: 16
                    Layout.topMargin: 14
                    Layout.bottomMargin: 10

                    Text {
                        text: "Frames"
                        color: "#aab2bb"
                        font.pixelSize: 13
                        font.bold: true
                        Layout.fillWidth: true
                    }

                    Text {
                        text: trichromeController.groups.length
                        color: "#aab2bb"
                        font.pixelSize: 13
                    }
                }

                ListView {
                    id: groupList
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.leftMargin: 16
                    Layout.rightMargin: 16
                    Layout.bottomMargin: 16
                    clip: true
                    spacing: 8
                    model: trichromeController.groups

                    delegate: Rectangle {
                        required property var modelData
                        width: ListView.view.width
                        height: 106
                        radius: 8
                        color: modelData.index === trichromeController.activeGroup ? "#202a25" : "#202429"
                        border.color: modelData.index === trichromeController.activeGroup ? "#4cbf88" : "#343a40"
                        border.width: 1

                        MouseArea {
                            anchors.fill: parent
                            enabled: modelData.complete
                            onClicked: trichromeController.setActiveGroup(modelData.index)
                        }

                        Column {
                            anchors.fill: parent
                            anchors.margins: 10
                            spacing: 7

                            Text {
                                text: modelData.complete ? modelData.title : modelData.title + " · incomplete"
                                color: "#f4f0e8"
                                font.pixelSize: 14
                                font.bold: true
                                elide: Text.ElideRight
                                width: parent.width
                            }

                            Repeater {
                                model: modelData.sources

                                Row {
                                    required property var modelData
                                    spacing: 8
                                    width: parent.width
                                    height: 18

                                    Rectangle {
                                        width: 18
                                        height: 18
                                        radius: 9
                                        color: modelData.roleShort === "R" ? "#e45b5b"
                                             : modelData.roleShort === "G" ? "#4cbf88"
                                             : "#5b8ee4"

                                        Text {
                                            anchors.centerIn: parent
                                            text: modelData.roleShort
                                            color: "#0b0d0e"
                                            font.pixelSize: 10
                                            font.bold: true
                                        }
                                    }

                                    Text {
                                        text: modelData.name
                                        color: "#aab2bb"
                                        font.pixelSize: 12
                                        elide: Text.ElideMiddle
                                        width: parent.width - 28
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 70
                color: "#15181b"

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 22
                    anchors.rightMargin: 22
                    spacing: 14

                    Column {
                        Layout.fillWidth: true
                        spacing: 4

                        Text {
                            text: trichromeController.activeGroup >= 0
                                ? "Frame " + (trichromeController.activeGroup + 1)
                                : "Frame 0"
                            color: "#aab2bb"
                            font.pixelSize: 12
                        }

                        Text {
                            text: trichromeController.exporting
                                ? trichromeController.exportProgressText
                                : trichromeController.hasPreview
                                ? "Composite preview"
                                : "Load a complete R/G/B group"
                            color: "#f4f0e8"
                            font.pixelSize: 17
                            font.bold: true
                            elide: Text.ElideRight
                            width: parent.width
                        }
                    }

                    BusyIndicator {
                        running: trichromeController.busy
                        visible: trichromeController.busy
                    }

                    ThemedButton {
                        text: "Export"
                        enabled: trichromeController.activeGroup >= 0 && !trichromeController.exporting
                        onClicked: exportWindow.openForExport()
                    }
                }
            }

            Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: "#343a40" }

            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: "#111315"

                Image {
                    id: preview
                    anchors.centerIn: parent
                    width: Math.min(parent.width - 56, sourceSize.width)
                    height: sourceSize.width > 0
                        ? width * sourceSize.height / sourceSize.width
                        : parent.height - 56
                    fillMode: Image.PreserveAspectFit
                    source: trichromeController.previewSource
                    cache: false
                    visible: trichromeController.hasPreview
                }

                Column {
                    anchors.centerIn: parent
                    spacing: 8
                    visible: !trichromeController.hasPreview

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "No composite preview"
                        color: "#f4f0e8"
                        font.pixelSize: 20
                        font.bold: true
                    }

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "Choose files or a folder, then select Bayer or Monochrome."
                        color: "#aab2bb"
                        font.pixelSize: 14
                    }
                }
            }
        }
    }
}
