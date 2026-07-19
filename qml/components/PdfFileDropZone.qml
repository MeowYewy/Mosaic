import QtQuick
import QtQuick.Controls
import ProjectO

Item {
    id: dropHost
    implicitHeight: 108

    property bool dragActive: FileDropBridge.dragActive
    property string hintText: Theme.tr("dropHint")
    property int incomingCount: FileDropBridge.dragFileCount

    readonly property real dragScale: dragActive ? 1.025 : 1.0

    Connections {
        target: FileDropBridge
        function onFilesDropped(paths) {
            if (paths.length > 0)
                dropPulse.restart()
        }
    }

    Rectangle {
        id: shadowFar
        anchors.fill: card
        anchors.topMargin: dropHost.dragActive ? 10 : 3
        radius: Theme.radiusMd
        color: Theme.shadowColor
        opacity: dropHost.dragActive ? Theme.shadowOpacity2 : 0
        z: 0

        Behavior on opacity { NumberAnimation { duration: Theme.animNormal; easing.type: Easing.OutCubic } }
        Behavior on anchors.topMargin { NumberAnimation { duration: Theme.animNormal; easing.type: Easing.OutCubic } }
    }

    Rectangle {
        id: shadowNear
        anchors.fill: card
        anchors.topMargin: dropHost.dragActive ? 5 : 2
        radius: Theme.radiusMd
        color: Theme.shadowColor
        opacity: dropHost.dragActive ? Theme.shadowOpacity1 : 0
        z: 1

        Behavior on opacity { NumberAnimation { duration: Theme.animNormal; easing.type: Easing.OutCubic } }
        Behavior on anchors.topMargin { NumberAnimation { duration: Theme.animNormal; easing.type: Easing.OutCubic } }
    }

    Rectangle {
        id: card
        anchors.fill: parent
        z: 2
        radius: Theme.radiusMd
        color: dropHost.dragActive ? Theme.surfaceAlt : Theme.surface
        border.color: Theme.border
        border.width: 1
        scale: dropHost.dragScale
        transformOrigin: Item.Center

        Behavior on color { ColorAnimation { duration: Theme.animNormal } }
        Behavior on scale {
            NumberAnimation { duration: Theme.animNormal; easing.type: Easing.OutCubic }
        }

        Column {
            anchors.centerIn: parent
            width: parent.width - 24
            spacing: 10
            z: 1
            enabled: true

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: dropHost.dragActive && dropHost.incomingCount > 0
                      ? ("+" + dropHost.incomingCount + " " + Theme.tr("filesAdded"))
                      : dropHost.hintText
                color: dropHost.dragActive ? Theme.text : Theme.textBody
                font: dropHost.dragActive ? Theme.mainFontBold : Theme.mainFont
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
                width: parent.width

                Behavior on color { ColorAnimation { duration: Theme.animFast } }
            }

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                visible: !dropHost.dragActive && PdfController.fileCount > 0
                text: PdfController.fileCount + " " + Theme.tr("filesAdded")
                color: Theme.accent
                font: Theme.captionFont
            }

            StyledButton {
                anchors.horizontalCenter: parent.horizontalCenter
                text: Theme.tr("browse")
                highlighted: true
                onClicked: PdfController.browseAndAddFiles()
            }
        }
    }

    SequentialAnimation {
        id: dropPulse
        NumberAnimation {
            target: card
            property: "scale"
            to: 1.03
            duration: Theme.animFast
            easing.type: Easing.OutCubic
        }
        NumberAnimation {
            target: card
            property: "scale"
            to: dropHost.dragScale
            duration: Theme.animNormal
            easing.type: Easing.OutCubic
        }
    }
}
