import QtQuick
import ProjectO

Rectangle {
    id: card
    property int margins: 12
    property alias radius: bg.radius
    default property alias content: body.data

    color: "transparent"

    Rectangle {
        anchors.fill: bg
        anchors.topMargin: Theme.shadowOffset2
        radius: bg.radius
        color: Theme.shadowColor
        opacity: Theme.shadowOpacity2
    }
    Rectangle {
        anchors.fill: bg
        anchors.topMargin: Theme.shadowOffset1
        radius: bg.radius
        color: Theme.shadowColor
        opacity: Theme.shadowOpacity1
    }

    Rectangle {
        id: bg
        anchors.fill: parent
        radius: Theme.radiusMd
        color: Theme.surface
        border.color: Theme.border
        border.width: 1

        Item {
            id: body
            anchors.fill: parent
            anchors.margins: card.margins
        }
    }
}
