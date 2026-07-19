import QtQuick
import QtQuick.Controls
import ProjectO

ToolTip {
    id: tip
    delay: 280
    padding: 6

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

    contentItem: Text {
        text: tip.text
        font: Theme.captionFont
        color: Theme.text
    }
}
