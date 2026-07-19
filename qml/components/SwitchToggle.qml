import QtQuick
import ProjectO

// Compact Apple-style switch (knob stays inside track)
Item {
    id: toggle

    property bool checked: false
    signal toggled(bool checked)

    implicitWidth: 40
    implicitHeight: 22

    Rectangle {
        id: track
        anchors.fill: parent
        radius: height / 2
        color: toggle.checked ? Theme.accent : Theme.tabInactive
        border.color: toggle.checked ? Theme.accent : Theme.border
        border.width: 1

        Behavior on color {
            ColorAnimation { duration: Theme.animFast; easing.type: Easing.OutCubic }
        }
    }

    Rectangle {
        id: knob
        width: 18
        height: 18
        radius: width / 2
        y: 2
        x: toggle.checked ? track.width - width - 2 : 2
        color: "#FFFFFF"

        Behavior on x {
            NumberAnimation { duration: Theme.animFast; easing.type: Easing.OutCubic }
        }
    }

    MouseArea {
        anchors.fill: parent
        cursorShape: Qt.PointingHandCursor
        onClicked: {
            toggle.checked = !toggle.checked
            toggle.toggled(toggle.checked)
        }
    }
}
