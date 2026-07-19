import QtQuick
import ProjectO

Item {
    id: btn
    width: 28
    height: 28
    signal clicked()

    Rectangle {
        anchors.fill: parent
        radius: 6
        color: area.containsMouse ? Theme.menuHover : "transparent"
        Text {
            anchors.centerIn: parent
            text: "×"
            font.pixelSize: 18
            color: Theme.textSecondary
        }
    }
    MouseArea {
        id: area
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        preventStealing: true
        z: 1
        onClicked: btn.clicked()
    }
}
