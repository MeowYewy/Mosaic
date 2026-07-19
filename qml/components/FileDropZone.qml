import QtQuick
import QtQuick.Controls
import ProjectO

Item {
    id: dropHost
    implicitHeight: 108

    property bool dragActive: FileDropBridge.dragActive
    property int incomingCount: FileDropBridge.dragFileCount

    Rectangle {
        id: card
        anchors.fill: parent
        radius: Theme.radiusMd
        color: dropHost.dragActive ? Theme.menuHover : Theme.surfaceAlt
        border.color: Theme.border
        border.width: 1
        scale: dropHost.dragActive ? 1.02 : 1.0
        Behavior on scale { NumberAnimation { duration: Theme.animNormal } }
        Behavior on color { ColorAnimation { duration: Theme.animFast } }

        Column {
            anchors.centerIn: parent
            spacing: 10
            width: parent.width - 24
            z: 1

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                width: parent.width
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
                text: dropHost.dragActive && dropHost.incomingCount > 0
                      ? ("+" + dropHost.incomingCount + " " + Theme.tr("filesAdded"))
                      : Theme.tr("dropHint")
                color: dropHost.dragActive ? Theme.text : Theme.textBody
                font: dropHost.dragActive ? Theme.mainFontBold : Theme.mainFont

                Behavior on color { ColorAnimation { duration: Theme.animFast } }
            }

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                visible: !dropHost.dragActive && AppController.fileCount > 0
                text: AppController.fileCount + " " + Theme.tr("filesAdded")
                color: Theme.accent
                font: Theme.captionFont
            }

            StyledButton {
                anchors.horizontalCenter: parent.horizontalCenter
                text: Theme.tr("browse")
                highlighted: true
                onClicked: AppController.browseAndAddFiles()
            }
        }
    }
}
