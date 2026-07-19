import QtQuick
import ProjectO

Rectangle {
    id: toast
    visible: opacity > 0.01
    opacity: 0
    radius: Theme.radiusMd
    color: Theme.surface
    border.color: ok ? Theme.success : Theme.danger
    border.width: 1
    width: Math.min(380, Math.max(160, label.implicitWidth + 32))
    height: 44
    scale: 0.96
    transformOrigin: Item.TopRight
    transform: Translate { y: toast.slideY }
    property bool ok: true
    property real slideY: -10

    function show(message, success) {
        ok = success
        label.text = message
        fadeOut.stop()
        hideTimer.stop()
        if (opacity < 0.05) {
            opacity = 0
            slideY = -10
            scale = 0.96
        }
        showAnim.restart()
        hideTimer.restart()
    }

    Text {
        id: label
        anchors.centerIn: parent
        font: Theme.mainFont
        color: toast.ok ? Theme.success : Theme.danger
    }

    ParallelAnimation {
        id: showAnim
        NumberAnimation { target: toast; property: "opacity"; to: 1; duration: Theme.animNormal }
        NumberAnimation { target: toast; property: "slideY"; to: 0; duration: Theme.animNormal }
        NumberAnimation { target: toast; property: "scale"; to: 1; duration: Theme.animNormal }
    }
    Timer {
        id: hideTimer
        interval: 2400
        onTriggered: fadeOut.start()
    }
    ParallelAnimation {
        id: fadeOut
        NumberAnimation { target: toast; property: "opacity"; to: 0; duration: Theme.animNormal }
        NumberAnimation { target: toast; property: "slideY"; to: -10; duration: Theme.animNormal }
        NumberAnimation { target: toast; property: "scale"; to: 0.96; duration: Theme.animNormal }
    }
}
