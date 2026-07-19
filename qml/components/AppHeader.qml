import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ProjectO

Rectangle {
    id: header
    color: Theme.surface

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

        Item { Layout.fillWidth: true }

        Text {
            Layout.alignment: Qt.AlignVCenter
            text: Theme.tr("devDisclaimer")
            font: Theme.captionFont
            color: Theme.textSecondary
            elide: Text.ElideRight
            Layout.maximumWidth: 260
        }

        ToolButton {
            id: menuBtn
            implicitWidth: 36
            implicitHeight: 36
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

            Rectangle {
                anchors.top: parent.top
                anchors.right: parent.right
                anchors.topMargin: 5
                anchors.rightMargin: 5
                width: 7
                height: 7
                radius: width / 2
                color: Theme.accent
                visible: UpdateChecker.hasUpdate
                border.width: 1.5
                border.color: Theme.surface
            }

            onClicked: header.menuRequested(menuBtn)
        }
    }
}
