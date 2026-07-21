import QtQuick
import ProjectO

Item {
    id: dropdown
    width: 200
    height: contentColumn.height + 20
    visible: opacity > 0.01
    opacity: 0
    scale: 0.94
    z: 2000
    transformOrigin: Item.TopRight
    Behavior on opacity { NumberAnimation { duration: Theme.animNormal } }
    Behavior on scale { NumberAnimation { duration: Theme.animNormal } }
    signal aboutRequested()
    signal settingsRequested()
    signal installRequested()

    function openAt(anchor) {
        if (!anchor) return
        const pos = anchor.mapToItem(dropdown.parent, 0, anchor.height + 6)
        dropdown.x = Math.max(8, pos.x - dropdown.width + anchor.width)
        dropdown.y = pos.y
        dropdown.opacity = 1
        dropdown.scale = 1
    }
    function close() {
        dropdown.opacity = 0
        dropdown.scale = 0.94
    }

    ShadowCard {
        anchors.fill: parent
        radius: Theme.radiusMd
        margins: 0
        Column {
            id: contentColumn
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            spacing: 2

            Item {
                width: parent.width
                height: 34

                Rectangle {
                    id: updateRow
                    anchors.centerIn: parent
                    width: parent.width - 4
                    height: 34
                    radius: Theme.radiusSm
                    color: updateHover ? Theme.menuHover : "transparent"

                    property bool updateHover: false

                    Text {
                        id: updateLabel
                        anchors.left: parent.left
                        anchors.leftMargin: 10
                        anchors.verticalCenter: parent.verticalCenter
                        text: Theme.tr("checkUpdate")
                        font: Theme.mainFont
                        color: Theme.text
                    }

                    UpdateNewVersionLabel {
                        id: newVersionPill
                        z: 2
                        anchors.left: updateLabel.right
                        anchors.leftMargin: 8
                        anchors.verticalCenter: parent.verticalCenter
                        clickable: true
                        visible: UpdateChecker.hasUpdate
                                && UpdateChecker.status !== 1
                                && UpdateChecker.status !== 2
                                && UpdateChecker.status !== 4
                                && UpdateChecker.status !== 5
                                && UpdateChecker.status !== 6
                                && UpdateChecker.status !== 7
                        onClicked: UpdateChecker.downloadUpdate()
                    }

                    UpdateStatusIcon {
                        anchors.left: updateLabel.right
                        anchors.leftMargin: 8
                        anchors.verticalCenter: parent.verticalCenter
                        visible: UpdateChecker.status === 1
                                || UpdateChecker.status === 2
                                || UpdateChecker.status === 4
                                || UpdateChecker.status === 6
                    }

                    Text {
                        anchors.left: updateLabel.right
                        anchors.leftMargin: 6
                        anchors.verticalCenter: parent.verticalCenter
                        visible: UpdateChecker.status === 5
                        text: UpdateChecker.downloadProgress + "%"
                        font: Theme.captionFont
                        color: Theme.textSecondary
                    }

                    Rectangle {
                        id: installPill
                        z: 2
                        anchors.left: updateLabel.right
                        anchors.leftMargin: 6
                        anchors.verticalCenter: parent.verticalCenter
                        height: 18
                        width: installPillText.implicitWidth + 10
                        radius: 9
                        color: Theme.accent
                        visible: UpdateChecker.status === 7

                        Text {
                            id: installPillText
                            anchors.centerIn: parent
                            text: Theme.tr("installUpdate")
                            color: "#FFFFFF"
                            font: Theme.captionBoldFont
                        }

                        MouseArea {
                            z: 2
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: function(mouse) {
                                mouse.accepted = true
                                dropdown.close()
                                dropdown.installRequested()
                            }
                        }
                    }

                    MouseArea {
                        z: 1
                        anchors.left: parent.left
                        anchors.top: parent.top
                        anchors.bottom: parent.bottom
                        width: (installPill.visible || newVersionPill.visible)
                               ? Math.max(installPill.visible ? installPill.x : 0,
                                          newVersionPill.visible ? newVersionPill.x : 0)
                               : parent.width
                        cursorShape: Qt.PointingHandCursor
                        hoverEnabled: true
                        onClicked: UpdateChecker.checkForUpdates()
                        onEntered: updateRow.updateHover = true
                        onExited: updateRow.updateHover = false
                    }
                }
            }

            Rectangle {
                width: parent.width - 12
                anchors.horizontalCenter: parent.horizontalCenter
                height: 1
                color: Theme.border
            }

            MenuOption {
                label: Theme.tr("settings")
                onTriggered: {
                    dropdown.close()
                    dropdown.settingsRequested()
                }
            }

            Rectangle {
                width: parent.width - 12
                anchors.horizontalCenter: parent.horizontalCenter
                height: 1
                color: Theme.border
            }

            MenuOption {
                label: Theme.tr("about")
                onTriggered: {
                    dropdown.close()
                    dropdown.aboutRequested()
                }
            }
        }
    }
}
