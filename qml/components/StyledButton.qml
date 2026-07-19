import QtQuick
import QtQuick.Controls
import ProjectO

Button {
    id: control
    implicitHeight: 36
    padding: 10
    hoverEnabled: true
    scale: control.down ? 0.97 : 1.0
    Behavior on scale { NumberAnimation { duration: Theme.animFast } }

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

    contentItem: Text {
        text: control.text
        font: control.highlighted ? Theme.mainFontBold : Theme.mainFont
        color: control.highlighted ? "#FFFFFF" : Theme.text
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
}
