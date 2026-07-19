import QtQuick
import ProjectO

Item {
    id: icon
    width: 18
    height: 18

    property int status: UpdateChecker.status
    property color tone: Theme.accent

    readonly property bool showSpinner: status === 1
    readonly property bool showSuccess: status === 2
    readonly property bool showFailure: status === 4 || status === 6

    visible: showSpinner || showSuccess || showFailure

    Item {
        anchors.centerIn: parent
        width: 18
        height: 18
        visible: showSpinner

        Canvas {
            anchors.fill: parent
            onPaint: {
                const ctx = getContext("2d")
                ctx.clearRect(0, 0, width, height)
                ctx.strokeStyle = icon.tone
                ctx.lineWidth = 2
                ctx.lineCap = "round"
                ctx.beginPath()
                ctx.arc(width / 2, height / 2, 6.5, 0, Math.PI * 1.35)
                ctx.stroke()
            }
        }

        RotationAnimator on rotation {
            running: showSpinner
            from: 0
            to: 360
            duration: 900
            loops: Animation.Infinite
        }
    }

    Canvas {
        anchors.fill: parent
        visible: showSuccess
        onPaint: {
            const ctx = getContext("2d")
            ctx.clearRect(0, 0, width, height)
            ctx.strokeStyle = Theme.success
            ctx.lineWidth = 2
            ctx.lineCap = "round"
            ctx.beginPath()
            ctx.arc(width / 2, height / 2, 6.5, -Math.PI * 0.75, Math.PI * 0.95)
            ctx.stroke()

            ctx.strokeStyle = Theme.success
            ctx.beginPath()
            ctx.moveTo(4.5, 9.2)
            ctx.lineTo(7.6, 12.2)
            ctx.lineTo(13.8, 5.8)
            ctx.stroke()
        }
        onVisibleChanged: if (visible) requestPaint()
    }

    Canvas {
        anchors.fill: parent
        visible: showFailure
        onPaint: {
            const ctx = getContext("2d")
            ctx.clearRect(0, 0, width, height)
            ctx.strokeStyle = Theme.danger
            ctx.lineWidth = 2
            ctx.lineCap = "round"
            ctx.beginPath()
            ctx.arc(width / 2, height / 2, 6.5, -Math.PI * 0.75, Math.PI * 0.95)
            ctx.stroke()

            ctx.strokeStyle = Theme.danger
            ctx.beginPath()
            ctx.moveTo(6.2, 6.2)
            ctx.lineTo(11.8, 11.8)
            ctx.moveTo(11.8, 6.2)
            ctx.lineTo(6.2, 11.8)
            ctx.stroke()
        }
        onVisibleChanged: if (visible) requestPaint()
    }
}
