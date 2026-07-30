import QtQuick
import QtQuick.Controls
import ProjectO

Button {
    id: control
    property bool compact: false

    implicitHeight: compact ? 28 : 36
    padding: compact ? 6 : 10
    hoverEnabled: control.enabled
    scale: control.enabled && control.down ? 0.97 : 1.0
    opacity: control.enabled ? 1.0 : 0.52
    Behavior on scale { NumberAnimation { duration: Theme.animFast } }
    Behavior on opacity { NumberAnimation { duration: Theme.animFast } }

    background: Rectangle {
        radius: Theme.radiusSm
        color: !control.enabled
               ? Theme.surfaceAlt
               : (control.down ? Theme.menuHover
                  : (control.hovered
                     ? (control.highlighted ? Theme.accentLight : Theme.menuHover)
                     : (control.highlighted ? Theme.accent : Theme.surfaceAlt)))
        border.color: !control.enabled
                      ? Theme.border
                      : (control.highlighted ? Theme.accent : Theme.border)
        border.width: 1
        Behavior on color { ColorAnimation { duration: Theme.animFast } }
    }

    contentItem: Text {
        text: control.text
        font: control.compact
               ? Theme.captionFont
               : (control.highlighted ? Theme.mainFontBold : Theme.mainFont)
        color: !control.enabled
               ? Theme.textSecondary
               : (control.highlighted ? "#FFFFFF" : Theme.text)
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
}
