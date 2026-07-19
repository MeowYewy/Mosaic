import QtQuick
import ProjectO

Item {
    id: grip
    implicitWidth: 20
    implicitHeight: 20

    property color lineColor: Theme.textSecondary
    property real lineOpacity: 0.35

    Column {
        anchors.centerIn: parent
        spacing: 3

        Repeater {
            model: 3
            Rectangle {
                width: 12
                height: 2
                radius: 1
                color: grip.lineColor
                opacity: grip.lineOpacity
                anchors.horizontalCenter: parent.horizontalCenter
            }
        }
    }
}
