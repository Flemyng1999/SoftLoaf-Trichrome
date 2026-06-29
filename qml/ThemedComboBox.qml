import QtQuick
import QtQuick.Controls

ComboBox {
    id: control

    implicitHeight: 28
    leftPadding: 10
    rightPadding: 28

    contentItem: Text {
        leftPadding: 0
        rightPadding: 0
        text: control.displayText
        color: control.enabled ? "#f4f0e8" : "#7d858e"
        font: control.font
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    indicator: Canvas {
        x: control.width - width - 10
        y: Math.round((control.height - height) / 2)
        width: 9
        height: 6
        opacity: control.enabled ? 1.0 : 0.65

        onPaint: {
            const ctx = getContext("2d")
            ctx.reset()
            ctx.strokeStyle = "#aab2bb"
            ctx.lineWidth = 1.4
            ctx.lineCap = "round"
            ctx.lineJoin = "round"
            ctx.beginPath()
            ctx.moveTo(1, 1)
            ctx.lineTo(width / 2, height - 1)
            ctx.lineTo(width - 1, 1)
            ctx.stroke()
        }
    }

    background: Rectangle {
        radius: 4
        color: control.enabled ? "#202429" : "#272b30"
        border.color: control.enabled && (control.hovered || control.visualFocus)
            ? "#4cbf88"
            : "#343a40"
        border.width: 1
    }

    delegate: ItemDelegate {
        width: control.width
        height: 30
        text: control.textRole.length > 0 && modelData
            && modelData[control.textRole] !== undefined
            ? modelData[control.textRole]
            : modelData
        highlighted: control.highlightedIndex === index

        contentItem: Text {
            text: parent.text
            color: parent.highlighted ? "#0b0d0e" : "#f4f0e8"
            font: control.font
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }

        background: Rectangle {
            color: parent.highlighted ? "#4cbf88" : "#202429"
        }
    }

    popup: Popup {
        y: control.height + 2
        width: control.width
        implicitHeight: contentItem.implicitHeight
        padding: 1

        contentItem: ListView {
            clip: true
            implicitHeight: Math.min(contentHeight, 240)
            model: control.popup.visible ? control.delegateModel : null
            currentIndex: control.highlightedIndex
        }

        background: Rectangle {
            radius: 4
            color: "#202429"
            border.color: "#4cbf88"
            border.width: 1
        }
    }
}
