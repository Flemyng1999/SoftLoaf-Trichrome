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

    property var selectedGroups: []
    property int selectionAnchorGroup: -1

    function completeGroupIndexes() {
        var indexes = []
        for (var i = 0; i < trichromeController.groups.length; ++i) {
            var group = trichromeController.groups[i]
            if (group.complete)
                indexes.push(group.index)
        }
        return indexes
    }

    function setFrameSelection(indexes, anchorIndex) {
        var unique = {}
        var next = []
        for (var i = 0; i < indexes.length; ++i) {
            var index = indexes[i]
            if (index < 0 || unique[index])
                continue
            unique[index] = true
            next.push(index)
        }
        next.sort(function(a, b) { return a - b })
        selectedGroups = next
        selectionAnchorGroup = anchorIndex === undefined ? (next.length > 0 ? next[next.length - 1] : -1) : anchorIndex
    }

    function clearFrameSelection() {
        selectedGroups = []
        selectionAnchorGroup = -1
    }

    function selectAllFrames() {
        var indexes = completeGroupIndexes()
        setFrameSelection(indexes, indexes.length > 0 ? indexes[0] : -1)
        if (trichromeController.activeGroup < 0 && indexes.length > 0)
            trichromeController.setActiveGroup(indexes[0])
    }

    function frameIsSelected(index) {
        return selectedGroups.indexOf(index) !== -1
    }

    function selectFrameRange(fromIndex, toIndex) {
        var first = Math.min(fromIndex, toIndex)
        var last = Math.max(fromIndex, toIndex)
        var indexes = []
        for (var i = 0; i < trichromeController.groups.length; ++i) {
            var group = trichromeController.groups[i]
            if (group.complete && group.index >= first && group.index <= last)
                indexes.push(group.index)
        }
        setFrameSelection(indexes, fromIndex)
    }

    function handleFrameClick(index, shiftPressed) {
        if (shiftPressed && selectionAnchorGroup >= 0)
            selectFrameRange(selectionAnchorGroup, index)
        else
            setFrameSelection([index], index)
        trichromeController.setActiveGroup(index)
    }

    function pruneFrameSelection() {
        if (selectedGroups.length === 0)
            return
        var available = {}
        for (var i = 0; i < trichromeController.groups.length; ++i) {
            var group = trichromeController.groups[i]
            if (group.complete)
                available[group.index] = true
        }
        var next = []
        for (var j = 0; j < selectedGroups.length; ++j) {
            var selected = selectedGroups[j]
            if (available[selected])
                next.push(selected)
        }
        if (available[selectionAnchorGroup])
            setFrameSelection(next, selectionAnchorGroup)
        else
            setFrameSelection(next)
    }

    Component.onCompleted: {
        if (trichromeController.activeGroup >= 0)
            setFrameSelection([trichromeController.activeGroup], trichromeController.activeGroup)
    }

    Connections {
        target: trichromeController

        function onGroupsChanged() {
            root.pruneFrameSelection()
        }
    }

    Shortcut {
        sequence: StandardKey.SelectAll
        onActivated: root.selectAllFrames()
    }

    Shortcut {
        sequence: "Ctrl+Shift+A"
        onActivated: root.clearFrameSelection()
    }

    Shortcut {
        sequence: "Meta+Shift+A"
        onActivated: root.clearFrameSelection()
    }

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

                        Row {
                            spacing: 5

                            Text {
                                text: "SoftLoaf"
                                color: "#f4f0e8"
                                font.pixelSize: 21
                                font.bold: true
                            }

                            Text {
                                text: "Trichrome"
                                color: "#f4f0e8"
                                font.pixelSize: 21
                                font.bold: true
                            }
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

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 10

                        Label {
                            text: "Sensor"
                            color: "#aab2bb"
                            font.pixelSize: 12
                            Layout.preferredWidth: 80
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 34
                            radius: 7
                            color: "#171a1d"
                            border.color: "#343a40"
                            border.width: 1
                            clip: true

                            Row {
                                anchors.fill: parent
                                anchors.margins: 3

                                Repeater {
                                    model: [
                                        { text: "Bayer", value: "bayer" },
                                        { text: "Monochrome", value: "mono" }
                                    ]

                                    delegate: Rectangle {
                                        required property var modelData
                                        readonly property bool selected: trichromeController.sensorMode === modelData.value

                                        width: parent.width / 2
                                        height: parent.height
                                        radius: 5
                                        color: selected ? "#244f3d" : "transparent"
                                        border.color: selected ? "#4cbf88" : "transparent"
                                        border.width: 1

                                        Text {
                                            anchors.centerIn: parent
                                            text: modelData.text
                                            color: selected ? "#f4f0e8" : "#d6d0c8"
                                            font.pixelSize: 12
                                            font.bold: selected
                                            horizontalAlignment: Text.AlignHCenter
                                            verticalAlignment: Text.AlignVCenter
                                            elide: Text.ElideRight
                                            width: parent.width - 16
                                        }

                                        MouseArea {
                                            anchors.fill: parent
                                            cursorShape: Qt.PointingHandCursor
                                            onClicked: trichromeController.sensorMode = modelData.value
                                        }
                                    }
                                }
                            }
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

                    RowLayout {
                        spacing: 8

                        Rectangle {
                            Layout.preferredWidth: 76
                            Layout.preferredHeight: 4
                            radius: 2
                            color: "#2a2f34"
                            visible: trichromeController.completeGroupCount > 0

                            Rectangle {
                                anchors.left: parent.left
                                anchors.top: parent.top
                                anchors.bottom: parent.bottom
                                width: parent.width * (
                                    trichromeController.completeGroupCount > 0
                                        ? trichromeController.previewReadyCount /
                                          trichromeController.completeGroupCount
                                        : 0)
                                radius: 2
                                color: "#4cbf88"
                            }
                        }

                        Text {
                            text: trichromeController.completeGroupCount > 0
                                ? trichromeController.previewReadyCount + "/" +
                                  trichromeController.completeGroupCount
                                : trichromeController.groups.length
                            color: "#aab2bb"
                            font.pixelSize: 13
                            horizontalAlignment: Text.AlignRight
                            Layout.preferredWidth: 34
                        }
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

                    TapHandler {
                        acceptedButtons: Qt.LeftButton
                        onTapped: function(point) {
                            if (groupList.indexAt(point.position.x, point.position.y) < 0)
                                root.clearFrameSelection()
                        }
                    }

                    delegate: Rectangle {
                        id: frameDelegate
                        required property var modelData
                        property bool selected: root.frameIsSelected(modelData.index)
                        width: ListView.view.width
                        height: 106
                        radius: 8
                        color: selected ? "#202a25" : "#202429"
                        border.color: selected ? "#4cbf88" : "#343a40"
                        border.width: 1
                        property bool hovered: frameMouse.containsMouse || deleteMouse.containsMouse

                        MouseArea {
                            id: frameMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            enabled: modelData.complete
                            onClicked: function(mouse) {
                                root.handleFrameClick(
                                    modelData.index,
                                    (mouse.modifiers & Qt.ShiftModifier) !== 0)
                            }
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

                        Rectangle {
                            id: deleteButton
                            width: 24
                            height: 24
                            radius: 12
                            anchors.top: parent.top
                            anchors.right: parent.right
                            anchors.topMargin: 8
                            anchors.rightMargin: 8
                            color: deleteMouse.containsMouse ? "#343a40" : "#2a2f34"
                            border.color: deleteMouse.containsMouse ? "#4cbf88" : "#424950"
                            border.width: 1
                            opacity: frameDelegate.hovered || frameDelegate.selected ? 1 : 0
                            visible: opacity > 0
                            z: 2

                            Behavior on opacity {
                                NumberAnimation { duration: 90 }
                            }

                            Rectangle {
                                anchors.centerIn: parent
                                width: 13
                                height: 2
                                radius: 1
                                color: "#f4f0e8"
                                rotation: 45
                            }

                            Rectangle {
                                anchors.centerIn: parent
                                width: 13
                                height: 2
                                radius: 1
                                color: "#f4f0e8"
                                rotation: -45
                            }

                            MouseArea {
                                id: deleteMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                acceptedButtons: Qt.LeftButton
                                onClicked: function(mouse) {
                                    mouse.accepted = true
                                    trichromeController.deleteGroup(modelData.index)
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
                                : ""
                            visible: text.length > 0
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
