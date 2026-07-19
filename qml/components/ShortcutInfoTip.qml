import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ProjectO

Item {
    id: tipRoot

    property Item popupParent: tipRoot
    property var rows: []
    property int labelColumnWidth: {
        const _ = AppSettings.languageRevision
        return AppSettings.language === "en" ? 116 : 72
    }

    implicitWidth: 24
    implicitHeight: 24

    property bool iconHovered: false
    property bool panelHovered: false
    readonly property bool open: iconHovered || panelHovered

    Rectangle {
        anchors.centerIn: parent
        width: 22
        height: 22
        radius: 11
        color: tipRoot.iconHovered ? Theme.menuHover : Theme.surfaceAlt
        border.color: tipRoot.iconHovered ? Theme.iconHover : Theme.border
        border.width: 1
        Behavior on color { ColorAnimation { duration: Theme.animFast } }
    }

    LucideIcon {
        anchors.centerIn: parent
        name: "info"
        iconSize: 14
        color: {
            const _ = AppSettings.themeRevision
            return Theme.iconDefault
        }
    }

    MouseArea {
        id: infoArea
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onContainsMouseChanged: tipRoot.iconHovered = containsMouse
    }

    Popup {
        id: shortcutPopup
        parent: tipRoot.popupParent
        z: 2000
        modal: false
        focus: false
        padding: 10
        closePolicy: Popup.NoAutoClose
        visible: tipRoot.open

        onAboutToShow: reposition()
        onVisibleChanged: if (visible) reposition()

        function reposition() {
            if (!tipRoot.popupParent)
                return
            const anchor = tipRoot.mapToItem(tipRoot.popupParent, tipRoot.width / 2,
                                             tipRoot.height + 6)
            x = anchor.x - width / 2
            y = anchor.y
        }

        background: Rectangle {
            radius: Theme.radiusSm
            color: Theme.surface
            border.color: Theme.border
            border.width: 1

            Rectangle {
                z: -1
                anchors.fill: parent
                anchors.topMargin: 2
                radius: parent.radius
                color: Theme.shadowColor
                opacity: Theme.shadowOpacity1
            }
        }

        contentItem: Item {
            implicitWidth: grid.implicitWidth
            implicitHeight: grid.implicitHeight

            MouseArea {
                anchors.fill: parent
                hoverEnabled: true
                acceptedButtons: Qt.NoButton
                onContainsMouseChanged: tipRoot.panelHovered = containsMouse
            }

            ColumnLayout {
                id: grid
                spacing: 6

                Repeater {
                    model: tipRoot.rows

                    RowLayout {
                        spacing: 12

                        Text {
                            Layout.preferredWidth: tipRoot.labelColumnWidth
                            Layout.minimumWidth: tipRoot.labelColumnWidth
                            Layout.alignment: Qt.AlignVCenter
                            text: Theme.tr(modelData.labelKey)
                            font: Theme.captionFont
                            color: Theme.textSecondary
                            elide: Text.ElideRight
                        }

                        Text {
                            Layout.alignment: Qt.AlignVCenter
                            text: modelData.keys
                            font: Theme.captionBoldFont
                            color: Theme.text
                        }
                    }
                }
            }
        }
    }
}
