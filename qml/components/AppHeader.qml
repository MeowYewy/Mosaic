import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ProjectO

Rectangle {
    id: header
    color: Theme.surface
    clip: false

    signal menuRequested(Item anchor)

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 1
        color: Theme.border
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 32
        anchors.rightMargin: 28
        spacing: 12

        AppLogo {
            Layout.alignment: Qt.AlignVCenter
            logoSize: 32
            cornerRadius: 8
        }

        Text {
            text: Theme.headerName
            font: Theme.brandTitleFont
            color: Theme.text
        }

        Item { Layout.fillWidth: true; Layout.minimumWidth: 8 }

        Text {
            Layout.alignment: Qt.AlignVCenter
            Layout.maximumWidth: 420
            text: Theme.tr("devDisclaimer")
            font: Theme.captionFont
            color: Theme.textSecondary
            wrapMode: Text.Wrap
            horizontalAlignment: Text.AlignRight
        }

        Item {
            Layout.preferredWidth: 36
            Layout.preferredHeight: 36
            Layout.topMargin: UpdateChecker.hasUpdate ? 4 : 0
            clip: false

            ToolButton {
                id: menuBtn
                anchors.fill: parent
                hoverEnabled: true
                focusPolicy: Qt.NoFocus
                background: Rectangle {
                    radius: Theme.radiusSm
                    color: menuBtn.hovered ? Theme.menuHover : "transparent"
                }
                contentItem: Item {
                    LucideIcon {
                        anchors.centerIn: parent
                        iconSize: 18
                        name: "menu"
                        strokeColor: Theme.iconDefault
                    }
                }
                onClicked: header.menuRequested(menuBtn)
            }

            UpdateNewVersionLabel {
                compact: true
                anchors.left: parent.right
                anchors.top: parent.top
                anchors.leftMargin: -9
                anchors.topMargin: -5
                visible: UpdateChecker.hasUpdate
                z: 10
            }
        }
    }
}
