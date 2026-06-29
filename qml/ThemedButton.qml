import QtQuick
import QtQuick.Controls

Button {
    id: control

    implicitHeight: 28
    padding: 8
    leftPadding: 12
    rightPadding: 12

    contentItem: Text {
        text: control.text
        color: control.enabled ? "#f4f0e8" : "#7d858e"
        font: control.font
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    background: Rectangle {
        radius: 4
        color: !control.enabled ? "#272b30"
            : control.down ? "#5ebf94"
            : control.checked || control.highlighted ? "#4cbf88"
            : control.hovered ? "#2c3338"
            : "#202429"
        border.color: !control.enabled ? "#3a4046"
            : control.checked || control.highlighted ? "#76d8aa"
            : control.hovered ? "#46505a"
            : "#343a40"
        border.width: 1
    }
}
