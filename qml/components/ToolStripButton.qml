import QtQuick
import QtQuick.Controls
import ProjectO

Item {
    id: btn
    property string iconName: "expand"
    property string label: ""
    property bool active: false
    property bool enabled: true

    signal clicked()

    implicitWidth: 34
    implicitHeight: 34

    opacity: enabled ? 1 : 0.4
    scale: pressed && enabled ? 0.97 : 1
    Behavior on scale { NumberAnimation { duration: Theme.animFast } }

    property bool hovered: false
    property bool pressed: false

    StyledToolTip {
        visible: btn.hovered && btn.enabled && btn.label.length > 0
        text: btn.label
    }

    Rectangle {
        anchors.fill: parent
        radius: Theme.radiusSm
        color: btn.active ? Theme.accentSoft
               : (btn.pressed && btn.enabled ? Theme.menuHover
                  : (btn.hovered && btn.enabled ? Theme.menuHover : Theme.surfaceAlt))
        border.color: btn.active ? Theme.accent
                      : (btn.hovered && btn.enabled ? Theme.border : Theme.border)
        border.width: 1
        Behavior on color { ColorAnimation { duration: Theme.animFast } }
    }

    LucideIcon {
        anchors.centerIn: parent
        name: btn.iconName
        iconSize: 16
        color: {
            const _ = AppSettings.themeRevision
            if (btn.active)
                return Theme.accent
            return Theme.iconDefault
        }
    }

    MouseArea {
        anchors.fill: parent
        enabled: btn.enabled
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onEntered: btn.hovered = true
        onExited: { btn.hovered = false; btn.pressed = false }
        onPressed: btn.pressed = true
        onReleased: btn.pressed = false
        onCanceled: btn.pressed = false
        onClicked: btn.clicked()
    }
}
