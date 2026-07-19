import QtQuick
import ProjectO

Rectangle {
    id: btn
    property string label: ""
    property bool selected: false
    property bool hovered: false
    signal clicked()
    radius: Theme.radiusSm - 2
    color: selected ? Theme.accent : (hovered ? Theme.menuHover : "transparent")

    Text {
        anchors.centerIn: parent
        text: btn.label
        font.pixelSize: Theme.mainFont.pixelSize
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
