import QtQuick
import QtQuick.Controls
import ProjectO

Item {
    id: root

    implicitWidth: 38
    implicitHeight: 38

    property string colorValue: Theme.watermarkDefaultColor

    readonly property var colorOptions: [
        "#FF3B30",
        "#FF9500",
        "#FFCC00",
        "#34C759",
        "#007AFF",
        "#0F766E",
        "#FFFFFF",
        "#5A5A5A",
        "#1D1D1F"
    ]

    readonly property int swatchSize: 24
    readonly property int panelPadding: 7
    // presets + gaps + divider + current swatch, all inside the panel padding
    readonly property int panelWidth: panelPadding * 2
                                      + colorOptions.length * swatchSize
                                      + (colorOptions.length + 1) * 4
                                      + 1 + swatchSize

    function needsOutline(c) {
        return String(c).toUpperCase() === "#FFFFFF"
    }

    WheelHandler {
        onWheel: function(event) {
            event.accepted = true
        }
    }

    // Closed state: framed square button holding the current color swatch.
    Rectangle {
        id: frame
        anchors.fill: parent
        radius: Theme.radiusSm
        color: mouseArea.containsMouse || menuPopup.visible ? Theme.menuHover : Theme.surfaceAlt
        border.color: menuPopup.visible ? Theme.accent : Theme.border
        border.width: menuPopup.visible ? 2 : 1

        Behavior on color {
            ColorAnimation { duration: Theme.animFast; easing.type: Easing.OutCubic }
        }

        Rectangle {
            anchors.centerIn: parent
            width: root.swatchSize
            height: root.swatchSize
            radius: 6
            color: root.colorValue
            border.color: root.needsOutline(root.colorValue) ? Theme.border : "transparent"
            border.width: 1
        }

        MouseArea {
            id: mouseArea
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
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
    }

    // Expands leftwards from the swatch; the current color stays put at the
    // right end, separated from the presets by a vertical divider.
    Popup {
        id: menuPopup
        x: root.width - width
        y: 0
        width: root.panelWidth
        height: 38
        padding: 0
        modal: false
        dim: false
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

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
            Rectangle {
                id: expandPanel
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                height: 38
                width: menuPopup.reveal ? root.panelWidth : 38
                radius: Theme.radiusSm
                color: Theme.surface
                border.color: Theme.border
                border.width: 1
                clip: true

                Behavior on width {
                    NumberAnimation { duration: Theme.animNormal; easing.type: Easing.OutCubic }
                }

                Row {
                    anchors.right: parent.right
                    anchors.rightMargin: root.panelPadding
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 4

                    Repeater {
                        model: root.colorOptions

                        Rectangle {
                            required property string modelData
                            readonly property bool picked:
                                modelData.toUpperCase() === root.colorValue.toUpperCase()

                            width: root.swatchSize
                            height: root.swatchSize
                            anchors.verticalCenter: parent.verticalCenter
                            radius: 6
                            color: modelData
                            border.color: picked ? Theme.accent
                                        : (root.needsOutline(modelData) ? Theme.border : "transparent")
                            border.width: picked ? 2 : 1
                            scale: presetMouse.containsMouse && !picked ? 1.12 : 1

                            Behavior on scale {
                                NumberAnimation { duration: Theme.animFast; easing.type: Easing.OutCubic }
                            }
                            Behavior on border.color {
                                ColorAnimation { duration: Theme.animFast; easing.type: Easing.OutCubic }
                            }

                            MouseArea {
                                id: presetMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    root.colorValue = parent.modelData
                                    menuPopup.close()
                                }
                            }
                        }
                    }

                    Rectangle {
                        width: 1
                        height: 22
                        anchors.verticalCenter: parent.verticalCenter
                        color: Theme.border
                    }

                    // Current color, kept in the same place as the closed swatch.
                    Rectangle {
                        width: root.swatchSize
                        height: root.swatchSize
                        anchors.verticalCenter: parent.verticalCenter
                        radius: 6
                        color: root.colorValue
                        border.color: root.needsOutline(root.colorValue) ? Theme.border : "transparent"
                        border.width: 1

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: menuPopup.close()
                        }
                    }
                }
            }
        }
    }
}
