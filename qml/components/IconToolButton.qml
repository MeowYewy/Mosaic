import QtQuick
import QtQuick.Controls
import ProjectO

Button {
    id: control
    implicitWidth: 32
    implicitHeight: 32
    padding: 0
    hoverEnabled: true
    scale: control.down ? 0.97 : 1.0
    property string iconName: "fit-view"
    property string tipText: ""

    Behavior on scale { NumberAnimation { duration: Theme.animFast } }

    StyledToolTip {
        visible: control.hovered && control.tipText.length > 0
        text: control.tipText
    }

    background: Rectangle {
        radius: Theme.radiusSm
        color: control.down ? Theme.menuHover
               : (control.hovered
                  ? (control.highlighted ? Theme.accentLight : Theme.menuHover)
                  : (control.highlighted ? Theme.accent : Theme.surfaceAlt))
        border.color: control.highlighted ? Theme.accent : Theme.border
        border.width: 1
        Behavior on color { ColorAnimation { duration: Theme.animFast } }
    }

    contentItem: LucideIcon {
        anchors.centerIn: parent
        name: control.iconName
        iconSize: 13
        color: control.highlighted ? Theme.accent
               : (control.hovered ? Theme.iconHover : Theme.iconDefault)
    }
}
