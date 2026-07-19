import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ProjectO

Item {
    id: root
    z: 3000

    property bool shown: false
    property real overlayOpacity: 0
    visible: shown || overlayOpacity > 0.001

    signal changelogRequested()

    function open() {
        root.shown = true
        root.overlayOpacity = Theme.dimOpacity
        root.forceActiveFocus()
    }

    function close() {
        root.shown = false
        root.overlayOpacity = 0
    }

    Behavior on overlayOpacity {
        NumberAnimation { duration: Theme.animNormal; easing.type: Easing.OutCubic }
    }

    Keys.onEscapePressed: root.close()

    Rectangle {
        anchors.fill: parent
        color: Theme.dimOverlay
        opacity: root.overlayOpacity

        MouseArea {
            anchors.fill: parent
            onClicked: root.close()
            onWheel: function(wheel) { wheel.accepted = true }
        }
    }

    Item {
        id: dialogLayer
        anchors.centerIn: parent
        width: card.width
        height: card.height
        scale: root.shown ? 1 : 0.92
        opacity: root.shown ? 1 : 0
        transformOrigin: Item.Center

        Behavior on scale {
            NumberAnimation {
                duration: Theme.animSlow
                easing.type: Easing.OutBack
                easing.overshoot: 1.08
            }
        }
        Behavior on opacity {
            NumberAnimation { duration: Theme.animNormal; easing.type: Easing.OutCubic }
        }

        Rectangle {
            anchors.fill: card
            anchors.topMargin: Theme.shadowOffset2
            radius: card.radius
            color: Theme.shadowColor
            opacity: Theme.shadowOpacity2
        }

        Rectangle {
            anchors.fill: card
            anchors.topMargin: Theme.shadowOffset1
            radius: card.radius
            color: Theme.shadowColor
            opacity: Theme.shadowOpacity1
        }

        Rectangle {
            id: card
            width: 360
            height: body.implicitHeight + 64
            radius: Theme.radiusLg
            color: Theme.surface
            border.color: Theme.border
            border.width: 1

            MouseArea {
                anchors.fill: parent
                onClicked: {}
                onWheel: function(wheel) { wheel.accepted = true }
            }

            Column {
                id: body
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.top: parent.top
                anchors.topMargin: 32
                spacing: 16
                width: 300

                AppLogo {
                    anchors.horizontalCenter: parent.horizontalCenter
                    logoSize: 64
                    cornerRadius: 16
                }

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: Theme.tr("appName")
                    font: Theme.brandTitleFont
                    color: Theme.text
                }

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: parent.width
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.Wrap
                    text: Theme.tr("aboutTagline")
                    font: Theme.captionFont
                    color: Theme.textSecondary
                }

                Rectangle {
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: parent.width
                    height: 1
                    color: Theme.border
                }

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: Theme.tr("version") + "  v" + AppSettings.appVersion
                    color: Theme.text
                    font: Theme.mainFont
                }

                Column {
                    anchors.horizontalCenter: parent.horizontalCenter
                    spacing: 4
                    width: parent.width

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: Theme.tr("author")
                        color: Theme.text
                        font: Theme.mainFont
                        horizontalAlignment: Text.AlignHCenter
                    }

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: Theme.tr("aboutCopyright")
                        color: Theme.textSecondary
                        font: Theme.captionFont
                        horizontalAlignment: Text.AlignHCenter
                    }
                }

                Item { width: 1; height: 8 }

                Row {
                    anchors.horizontalCenter: parent.horizontalCenter
                    spacing: 8

                    StyledButton {
                        width: 92
                        text: Theme.tr("changelog")
                        onClicked: root.changelogRequested()
                    }

                    StyledButton {
                        width: 92
                        text: Theme.tr("close")
                        highlighted: true
                        onClicked: root.close()
                    }
                }
            }
        }
    }
}
