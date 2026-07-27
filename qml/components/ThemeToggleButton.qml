import QtQuick
import ProjectO

Rectangle {
    id: btn
    property string label: ""
    property bool selected: false
    property bool hovered: false
    property int labelSize: 0
    property bool outlined: false
    signal clicked()
    radius: Theme.radiusSm - 2
    color: selected ? Theme.accent : (hovered ? Theme.menuHover : (outlined ? Theme.surfaceAlt : "transparent"))
    border.width: outlined ? 1 : 0
    border.color: Theme.border

    Text {
        anchors.centerIn: parent
        text: btn.label
        font.pixelSize: btn.labelSize > 0 ? btn.labelSize : Theme.mainFont.pixelSize
        font.family: Theme.mainFont.family
        font.weight: btn.selected ? Font.DemiBold : Font.Medium
        color: btn.selected ? "#FFFFFF" : Theme.text
    }
    MouseArea {
        anchors.fill: parent
        cursorShape: Qt.PointingHandCursor
        hoverEnabled: true
        onClicked: btn.clicked()
        onEntered: btn.hovered = true
        onExited: btn.hovered = false
    }
}
