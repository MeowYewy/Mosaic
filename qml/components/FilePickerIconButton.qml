import QtQuick
import QtQuick.Controls
import ProjectO

ToolButton {
    id: control

    property string iconKind: "up"
    property real iconSlide: 0
    property real iconSpin: 0

    implicitWidth: 36
    implicitHeight: 36
    hoverEnabled: true
    focusPolicy: Qt.NoFocus

    function playUpEffect() {
        upAnim.restart()
    }

    function playRefreshEffect() {
        refreshAnim.restart()
    }

    SequentialAnimation {
        id: upAnim
        NumberAnimation {
            target: control
            property: "iconSlide"
            from: 0
            to: -12
            duration: 170
            easing.type: Easing.InCubic
        }
        ScriptAction {
            script: control.iconSlide = 12
        }
        NumberAnimation {
            target: control
            property: "iconSlide"
            from: 12
            to: 0
            duration: 300
            easing.type: Easing.OutCubic
        }
    }

    NumberAnimation {
        id: refreshAnim
        target: control
        property: "iconSpin"
        from: control.iconSpin
        to: control.iconSpin + 720
        duration: 600
        easing.type: Easing.InOutCubic
    }

    background: Rectangle {
        radius: Theme.radiusSm
        color: !control.enabled ? Theme.tabInactive
               : control.down ? Theme.tabInactive
               : control.hovered ? Theme.menuHover : Theme.surfaceAlt
        border.color: Theme.border
        border.width: 1
        opacity: control.enabled ? 1 : 0.55

        Behavior on color {
            ColorAnimation { duration: Theme.animFast; easing.type: Easing.OutCubic }
        }
    }

    Item {
        anchors.fill: parent
        clip: true

        Item {
            id: iconHost
            width: 18
            height: 18
            x: (parent.width - width) / 2
            y: (parent.height - height) / 2
            transform: [
                Translate {
                    x: control.iconKind === "up" ? control.iconSlide : 0
                },
                Rotation {
                    origin.x: iconHost.width / 2
                    origin.y: iconHost.height / 2
                    angle: control.iconKind === "refresh" ? control.iconSpin : 0
                }
            ]

            LucideIcon {
                anchors.fill: parent
                icon: control.iconKind === "up" ? "chevron-left" : "loader-circle"
                color: control.enabled ? Theme.textBody : Theme.textSecondary
            }
        }
    }
}
