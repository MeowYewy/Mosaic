import QtQuick
import ProjectO

Rectangle {
    id: pill
    property bool compact: false
    property bool clickable: false

    signal clicked()

    height: 20
    width: compact ? 34 : pillText.implicitWidth + 12
    radius: height / 2
    color: Theme.accent

    Text {
        id: pillText
        anchors.centerIn: parent
        text: compact ? "New" : Theme.tr("newVersion")
        color: "#FFFFFF"
        font.pixelSize: 11
        font.family: Theme.uiFontFamily
        font.weight: Font.Medium
    }

    MouseArea {
        anchors.fill: parent
        enabled: pill.clickable
        cursorShape: Qt.PointingHandCursor
        onClicked: pill.clicked()
    }
}
