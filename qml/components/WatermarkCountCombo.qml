import QtQuick
import QtQuick.Controls
import ProjectO

Item {
    id: root

    implicitWidth: compact ? 38 : Theme.compactControlWidth
    implicitHeight: 38

    property bool compact: false
    property int countValue: 1

    readonly property var countOptions: [
        { label: "1", value: 1 },
        { label: "2", value: 2 },
        { label: "3", value: 3 },
        { label: "4", value: 4 },
        { label: "5", value: 5 }
    ]

    property int selectedIndex: 0

    function applySelection(index) {
        if (index < 0 || index >= countOptions.length)
            return
        selectedIndex = index
        countValue = countOptions[index].value
    }

    WheelHandler {
        onWheel: function(event) {
            event.accepted = true
        }
    }

    Rectangle {
        id: background
        anchors.fill: parent
        radius: Theme.radiusSm
        color: mouseArea.containsMouse || menuPopup.visible ? Theme.menuHover : Theme.surfaceAlt
        border.color: menuPopup.visible ? Theme.accent : Theme.border
        border.width: menuPopup.visible ? 2 : 1
        clip: true

        Behavior on color {
            ColorAnimation { duration: Theme.animFast; easing.type: Easing.OutCubic }
        }

        MouseArea {
            id: mouseArea
            anchors.fill: parent
            hoverEnabled: true
            onWheel: function(wheel) {
                wheel.accepted = true
            }
            onClicked: {
                if (menuPopup.visible)
                    menuPopup.close()
                else
                    menuPopup.open()
            }
        }

        Text {
            anchors.fill: parent
            leftPadding: root.compact ? 0 : 20
            rightPadding: root.compact ? 0 : 20
            text: countOptions[selectedIndex].label
            font.pixelSize: 15
            font.family: Theme.mainFont.family
            font.weight: Font.Normal
            color: Theme.text
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
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
        x: 0
        // Positioned above the control before it is ever shown, so the first
        // open never flashes at the default (below) position.
        y: -height - 4
        width: root.width
        padding: 0
        modal: false
        dim: false
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        readonly property int listHeight: 36 * root.countOptions.length
                                          + Math.max(0, root.countOptions.length - 1) * 2

        property bool reveal: false
        onAboutToShow: reveal = true
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
                    model: root.countOptions
                    boundsBehavior: Flickable.StopAtBounds

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
                readonly property bool picked: index === root.selectedIndex
                text: modelData.label
                font.pixelSize: 15
                font.family: Theme.mainFont.family
                font.weight: Font.Normal
                palette.text: picked ? "#FFFFFF" : Theme.text
                palette.highlight: Theme.accent
                palette.highlightedText: "#FFFFFF"
                contentItem: Text {
                    width: parent.width
                    height: parent.height
                    text: optionDelegate.text
                    font: optionDelegate.font
                    color: optionDelegate.picked ? "#FFFFFF" : Theme.text
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                background: Rectangle {
                    radius: 4
                    anchors.fill: parent
                    color: optionDelegate.picked ? Theme.accent
                           : (optionDelegate.hovered ? Theme.menuHover : "transparent")
                }
                onClicked: {
                    root.applySelection(index)
                    menuPopup.close()
                }
            }
                }
            }
        }
    }

    Component.onCompleted: applySelection(0)
}
