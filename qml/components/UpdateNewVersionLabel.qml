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
        font.family: compact ? Theme.uiFontFamily : Theme.mainFont.family
        font.weight: compact ? Font.Medium : Font.Normal
    }

    MouseArea {
        anchors.fill: parent
        // Non-clickable pills still absorb hover so the menu button underneath
        // does not show a pointing-hand cursor (PageCase header "New" is visual only).
        acceptedButtons: pill.clickable ? Qt.AllButtons : Qt.NoButtons
        cursorShape: pill.clickable ? Qt.PointingHandCursor : Qt.ArrowCursor
        onClicked: pill.clicked()
    }
}
