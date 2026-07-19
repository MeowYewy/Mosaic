import QtQuick
import QtQuick.Controls
import ProjectO

Item {
    id: root

    implicitWidth: compact ? 38 : 156
    implicitHeight: 38

    property bool compact: false
    property var model: []
    property int currentIndex: 0
    signal activated(int index)

    readonly property int optionCount: model ? model.length : 0
    readonly property string displayText: {
        if (!model || currentIndex < 0 || currentIndex >= model.length)
            return ""
        const item = model[currentIndex]
        return item && item.label !== undefined ? String(item.label) : ""
    }

    function repositionPopup() {
        const topLeft = root.mapToItem(Overlay.overlay, 0, 0)
        menuPopup.width = root.compact ? Math.max(root.width, 56) : root.width
        // Compact combos sit near the right edge; keep the menu right-aligned.
        menuPopup.x = topLeft.x + root.width - menuPopup.width
        // Use the computed height rather than menuPopup.height: it is valid
        // before the popup is polished, so the first open never flashes at a
        // wrong position.
        menuPopup.y = topLeft.y - (menuPopup.listHeight + 12) - 4
    }

    WheelHandler {
        onWheel: function(event) {
            event.accepted = true
        }
    }

    Rectangle {
        anchors.fill: parent
        radius: Theme.radiusSm
        color: comboMouse.containsMouse || menuPopup.visible ? Theme.menuHover : Theme.surfaceAlt
        border.color: menuPopup.visible ? Theme.accent : Theme.border
        border.width: menuPopup.visible ? 2 : 1
        clip: true

        Behavior on color {
            ColorAnimation { duration: Theme.animFast; easing.type: Easing.OutCubic }
        }

        MouseArea {
            id: comboMouse
            anchors.fill: parent
            hoverEnabled: true
            onWheel: function(wheel) {
                wheel.accepted = true
            }
            onClicked: {
                if (menuPopup.visible) {
                    menuPopup.close()
                } else {
                    menuPopup.open()
                    Qt.callLater(root.repositionPopup)
                }
            }
        }

        Text {
            anchors.fill: parent
            leftPadding: root.compact ? 0 : 12
            rightPadding: root.compact ? 0 : 24
            text: root.displayText
            font.pixelSize: 15
            font.family: Theme.mainFont.family
            font.weight: Font.Normal
            color: Theme.text
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }

        Text {
            visible: !root.compact
            anchors.right: parent.right
            anchors.rightMargin: 10
            anchors.verticalCenter: parent.verticalCenter
            text: "▴"
            font.pixelSize: 14
            color: Theme.text
        }
    }

    Popup {
        id: menuPopup
        parent: Overlay.overlay
        z: 7000
        width: root.compact ? Math.max(root.width, 56) : root.width
        padding: 0
        modal: false
        dim: false
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        readonly property int listHeight: 36 * root.optionCount
                                          + Math.max(0, root.optionCount - 1) * 2

        property bool reveal: false
        onAboutToShow: {
            reveal = true
            root.repositionPopup()
        }
        onAboutToHide: reveal = false

        enter: Transition {
            NumberAnimation {
                property: "opacity"; from: 0; to: 1
                duration: Theme.animFast; easing.type: Easing.OutCubic
            }
        }
        exit: Transition {
            NumberAnimation {
                property: "opacity"; from: 1; to: 0
                duration: Theme.animNormal; easing.type: Easing.InCubic
            }
        }

        background: null

        contentItem: Item {
            implicitHeight: menuPopup.listHeight + 12

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: menuPopup.reveal ? parent.height : 0
                radius: Theme.radiusSm
                color: Theme.surface
                border.color: Theme.border
                border.width: 1
                clip: true

                Behavior on height {
                    NumberAnimation { duration: Theme.animNormal; easing.type: Easing.OutCubic }
                }

                ListView {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    anchors.margins: 6
                    height: menuPopup.listHeight
                    clip: true
                    spacing: 2
                    interactive: false
                    model: root.model
                    boundsBehavior: Flickable.StopAtBounds
                    onContentHeightChanged: if (menuPopup.visible)
                                                Qt.callLater(root.repositionPopup)

                    delegate: ItemDelegate {
                id: optionDelegate
                width: ListView.view.width
                height: 36
                leftPadding: 0
                rightPadding: 0
                topPadding: 0
                bottomPadding: 0
                spacing: 0
                required property var modelData
                required property int index
                readonly property bool picked: index === root.currentIndex
                text: modelData && modelData.label !== undefined ? String(modelData.label) : ""
                font.pixelSize: 15
                font.family: Theme.mainFont.family
                font.weight: Font.Normal
                contentItem: Text {
                    width: parent.width
                    height: parent.height
                    text: optionDelegate.text
                    font: optionDelegate.font
                    color: optionDelegate.picked ? "#FFFFFF" : Theme.text
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideRight
                }
                background: Rectangle {
                    radius: 4
                    anchors.fill: parent
                    color: optionDelegate.picked ? Theme.accent
                           : (optionDelegate.hovered ? Theme.menuHover : "transparent")
                }
                onClicked: {
                    root.currentIndex = index
                    root.activated(index)
                    menuPopup.close()
                }
            }
                }
            }
        }
    }
}
