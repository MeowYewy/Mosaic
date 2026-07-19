import QtQuick
import QtQuick.Layouts
import ProjectO

Item {
    id: overlay
    anchors.fill: parent
    z: 5000
    visible: active
    clip: true

    property bool active: false
    property point origin: Qt.point(width / 2, height / 2)
    property rect anchorRect: Qt.rect(0, 0, 72, 44)
    property real converge: 0
    property real brandOpacity: 0
    property real anchorOpacity: 1
    property real overlayOpacity: 1
    property bool entering: true
    property var finishCallback: null
    property Item sourceButton: null

    opacity: overlayOpacity

    function coverRadiusAt(ox, oy) {
        const w = overlay.width
        const h = overlay.height
        if (w <= 0 || h <= 0)
            return 0
        return Math.max(
            Math.hypot(ox, oy),
            Math.hypot(w - ox, oy),
            Math.hypot(ox, h - oy),
            Math.hypot(w - ox, h - oy)
        ) + 2
    }

    readonly property real coverRadius: coverRadiusAt(origin.x, origin.y)

    function syncAnchorRect(sourceBtn) {
        if (!sourceBtn || overlay.width <= 0 || overlay.height <= 0)
            return
        const cx = sourceBtn.width / 2
        const cy = sourceBtn.height / 2
        const center = sourceBtn.mapToItem(overlay, cx, cy)
        const w = sourceBtn.width
        const h = sourceBtn.height
        origin = Qt.point(center.x, center.y)
        anchorRect = Qt.rect(center.x - w / 2, center.y - h / 2, w, h)
    }

    function startEnter(sourceBtn, done) {
        entering = true
        finishCallback = done || null
        sourceButton = sourceBtn
        active = true
        converge = 0
        brandOpacity = 0
        anchorOpacity = 1
        overlayOpacity = 1
        syncAnchorRect(sourceBtn)
        enterSeq.restart()
    }

    function startExit(sourceBtn, done) {
        entering = false
        finishCallback = done || null
        sourceButton = sourceBtn
        active = true
        converge = 1
        brandOpacity = 0
        anchorOpacity = 0
        overlayOpacity = 1
        syncAnchorRect(sourceBtn)
        exitSeq.restart()
    }

    onWidthChanged: if (active && sourceButton) syncAnchorRect(sourceButton)
    onHeightChanged: if (active && sourceButton) syncAnchorRect(sourceButton)

    Rectangle {
        id: ripple
        z: 0
        x: overlay.origin.x - width / 2
        y: overlay.origin.y - height / 2
        width: overlay.coverRadius * 2 * overlay.converge
        height: width
        radius: width / 2
        color: Theme.accent
    }

    Rectangle {
        id: anchorClone
        z: 20
        x: overlay.origin.x - width / 2
        y: overlay.origin.y - height / 2
        width: overlay.anchorRect.width
        height: overlay.anchorRect.height
        radius: Theme.radiusMd
        color: Theme.tabInactive
        border.color: Theme.border
        border.width: 1
        opacity: overlay.anchorOpacity

        Text {
            anchors.centerIn: parent
            text: overlay.entering ? Theme.tr("tabMask") : Theme.tr("back")
            font: Theme.tabFont
            color: Theme.text
        }
    }

    ColumnLayout {
        z: 10
        anchors.centerIn: parent
        spacing: 14
        opacity: brandOpacity
        scale: 0.92 + brandOpacity * 0.08

        AppLogo {
            Layout.alignment: Qt.AlignHCenter
            logoSize: 56
        }

        Text {
            Layout.alignment: Qt.AlignHCenter
            text: Theme.tr("appName")
            font: Theme.titleFont
            color: "#FFFFFF"
        }
    }

    SequentialAnimation {
        id: enterSeq
        ScriptAction {
            script: overlay.syncAnchorRect(overlay.sourceButton)
        }
        NumberAnimation {
            target: overlay
            property: "converge"
            from: 0
            to: 1
            duration: 400
            easing.type: Easing.OutCubic
        }
        ParallelAnimation {
            NumberAnimation {
                target: overlay
                property: "brandOpacity"
                from: 0
                to: 1
                duration: 220
                easing.type: Easing.OutCubic
            }
            NumberAnimation {
                target: overlay
                property: "anchorOpacity"
                from: 1
                to: 0
                duration: 220
                easing.type: Easing.InQuad
            }
        }
        PauseAnimation { duration: 420 }
        ScriptAction {
            script: {
                if (overlay.finishCallback) {
                    const cb = overlay.finishCallback
                    overlay.finishCallback = null
                    cb()
                }
            }
        }
        NumberAnimation {
            target: overlay
            property: "brandOpacity"
            to: 0
            duration: 180
            easing.type: Easing.InQuad
        }
        NumberAnimation {
            target: overlay
            property: "overlayOpacity"
            to: 0
            duration: 340
            easing.type: Easing.InOutQuad
        }
        ScriptAction {
            script: {
                overlay.active = false
                overlay.converge = 0
                overlay.brandOpacity = 0
                overlay.anchorOpacity = 1
                overlay.overlayOpacity = 1
            }
        }
    }

    SequentialAnimation {
        id: exitSeq
        ScriptAction {
            script: overlay.syncAnchorRect(overlay.sourceButton)
        }
        NumberAnimation {
            target: overlay
            property: "converge"
            from: 1
            to: 0
            duration: 340
            easing.type: Easing.InOutCubic
        }
        ScriptAction {
            script: {
                overlay.active = false
                overlay.converge = 0
                overlay.overlayOpacity = 1
                if (overlay.finishCallback) {
                    const cb = overlay.finishCallback
                    overlay.finishCallback = null
                    cb()
                }
            }
        }
    }
}
