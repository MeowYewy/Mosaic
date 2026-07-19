import QtQuick
import ProjectO

Rectangle {
    id: option
    width: parent ? parent.width - 8 : 252
    anchors.horizontalCenter: parent ? parent.horizontalCenter : undefined
    height: 36
    radius: Theme.radiusSm
    property string label: ""
    property bool selected: false
    property bool hovered: false
    property string fontFamily: ""
    signal triggered()
    color: selected ? Theme.accent : (hovered ? Theme.menuHover : "transparent")

    Text {
        anchors.left: parent.left
        anchors.leftMargin: 12
        anchors.verticalCenter: parent.verticalCenter
        text: option.label
        font.pixelSize: Theme.mainFont.pixelSize
        font.family: option.fontFamily || Theme.mainFont.family
        font.weight: option.selected ? Font.DemiBold : Font.Medium
        color: option.selected ? "#FFFFFF" : Theme.text
    }
    MouseArea {
        anchors.fill: parent
        cursorShape: Qt.PointingHandCursor
        hoverEnabled: true
        onClicked: option.triggered()
        onEntered: option.hovered = true
        onExited: option.hovered = false
    }
}
