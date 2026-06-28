import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

Window {
    id: win
    width: 460
    height: 500
    minimumWidth: 420
    minimumHeight: 360
    title: "Export"
    color: "#111315"

    palette.window: "#111315"
    palette.windowText: "#f4f0e8"
    palette.base: "#171a1d"
    palette.text: "#f4f0e8"
    palette.button: "#202429"
    palette.buttonText: "#f4f0e8"
    palette.highlight: "#4cbf88"

    property url destFolder: ""

    readonly property color cLabel: "#aab2bb"
    readonly property color cValue: "#f4f0e8"
    readonly property color cRule: "#343a40"

    function openForExport() {
        visible = true
        raise()
        requestActivate()
    }

    function folderText() {
        if (destFolder.toString().length === 0)
            return "No export folder selected"
        return decodeURIComponent(destFolder.toString().replace("file://", ""))
    }

    function startExport() {
        trichromeController.startExport(
            destFolder,
            scopeCombo.currentIndex === 1,
            formatCombo.currentValue,
            spaceCombo.currentValue,
            bitDepthCombo.currentValue,
            Math.round(qualitySlider.value))
        visible = false
    }

    FolderDialog {
        id: folderDialog
        title: "Export to..."
        onAccepted: win.destFolder = selectedFolder
    }

    component FieldLabel: Label {
        color: win.cLabel
        font.pixelSize: 12
        Layout.preferredWidth: 94
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.margins: 18
            spacing: 12

            RowLayout {
                Layout.fillWidth: true
                spacing: 10
                FieldLabel { text: "Range" }
                ComboBox {
                    id: scopeCombo
                    Layout.fillWidth: true
                    model: ["Current frame", "All complete frames"]
                    enabled: trichromeController.completeGroupCount > 1
                    opacity: enabled ? 1.0 : 0.55
                }
            }

            Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: win.cRule }

            RowLayout {
                Layout.fillWidth: true
                spacing: 10
                FieldLabel { text: "Format" }
                ComboBox {
                    id: formatCombo
                    Layout.fillWidth: true
                    textRole: "text"
                    valueRole: "value"
                    model: [
                        { text: "TIFF", value: "tiff" },
                        { text: "PNG", value: "png" },
                        { text: "JPEG", value: "jpeg" }
                    ]
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 10
                FieldLabel { text: "Color Space" }
                ComboBox {
                    id: spaceCombo
                    Layout.fillWidth: true
                    textRole: "text"
                    valueRole: "value"
                    model: [
                        { text: "ACES AP0 Linear", value: "aces_ap0_linear" },
                        { text: "ProPhoto RGB Linear", value: "prophoto_linear" },
                        { text: "ACEScg AP1 Linear", value: "acescg_linear" },
                        { text: "Rec.2020 Linear", value: "rec_2020_linear" },
                        { text: "Display P3", value: "display_p3" },
                        { text: "Adobe RGB", value: "adobe_rgb" },
                        { text: "P3-D65 Gamma 2.6", value: "p3_d65_gamma_2_6" },
                        { text: "sRGB", value: "srgb" },
                        { text: "Rec.709 Gamma 2.4", value: "rec_709" }
                    ]
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 10
                enabled: formatCombo.currentValue === "png" || formatCombo.currentValue === "tiff"
                opacity: enabled ? 1.0 : 0.45
                FieldLabel { text: "Bit Depth" }
                ComboBox {
                    id: bitDepthCombo
                    Layout.fillWidth: true
                    textRole: "text"
                    valueRole: "value"
                    currentIndex: 1
                    model: [
                        { text: "8-bit", value: 8 },
                        { text: "16-bit", value: 16 }
                    ]
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 10
                enabled: formatCombo.currentValue === "jpeg"
                opacity: enabled ? 1.0 : 0.45
                FieldLabel { text: "JPEG Quality" }
                Slider {
                    id: qualitySlider
                    Layout.fillWidth: true
                    from: 1
                    to: 100
                    stepSize: 1
                    value: 95
                    snapMode: Slider.SnapAlways
                }
                Label {
                    Layout.preferredWidth: 30
                    text: Math.round(qualitySlider.value)
                    color: win.cValue
                    font.pixelSize: 12
                    horizontalAlignment: Text.AlignRight
                }
            }

            Item { Layout.fillHeight: true }
        }

        Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: win.cRule }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.margins: 18
            spacing: 10

            RowLayout {
                Layout.fillWidth: true
                spacing: 10
                Label {
                    Layout.fillWidth: true
                    text: win.folderText()
                    color: win.destFolder.toString().length > 0 ? win.cValue : win.cLabel
                    font.pixelSize: 12
                    elide: Text.ElideMiddle
                }
                Button {
                    text: "Choose..."
                    onClicked: folderDialog.open()
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                Item { Layout.fillWidth: true }
                Button {
                    text: "Cancel"
                    onClicked: win.visible = false
                }
                Button {
                    text: "Start Export"
                    highlighted: true
                    enabled: win.destFolder.toString().length > 0 && !trichromeController.busy
                    onClicked: win.startExport()
                }
            }
        }
    }
}
