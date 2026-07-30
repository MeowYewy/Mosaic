import QtQuick
import QtQuick.Controls
import ProjectO

Rectangle {
    id: canvas
    radius: Theme.radiusMd
    color: Theme.bg
    border.color: Theme.border
    border.width: 1
    clip: true

    readonly property bool exportBusy: AppController.activeTask === "export"

    property real drawStartX: 0
    property real drawStartY: 0
    property bool drawing: false
    property real draftX: 0
    property real draftY: 0
    property real draftW: 0
    property real draftH: 0

    // Live edit state in normalized coords 鈥?avoids breaking Item bindings on drag/resize.
    property int editRegionId: -1
    property real editRx: 0
    property real editRy: 0
    property real editRw: 0
    property real editRh: 0

    function beginEdit(id, nrx, nry, nrw, nrh) {
        editRegionId = id
        editRx = nrx
        editRy = nry
        editRw = nrw
        editRh = nrh
    }

    function endEdit() {
        editRegionId = -1
    }

    function clampNormRect(nrx, nry, nrw, nrh) {
        let x = Math.max(0, Math.min(1, nrx))
        let y = Math.max(0, Math.min(1, nry))
        let w = Math.max(0.01, Math.min(1 - x, nrw))
        let h = Math.max(0.01, Math.min(1 - y, nrh))
        return { x: x, y: y, w: w, h: h }
    }

    Connections {
        target: AppController
        function onPreviewZoomChanged() {
            canvas.endEdit()
            Qt.callLater(canvas.clampScroll)
        }
        function onCurrentPageChanged() {
            canvas.endEdit()
            Qt.callLater(canvas.clampScroll)
        }
        function onToolModeChanged() { canvas.endEdit() }
        function onPreviewViewResetRequested() {
            Qt.callLater(canvas.resetScrollPosition)
        }
    }

    function previewSource() {
        const mode = AppController.showMaskedPreview ? "mask" : "raw"
        return "image://preview/" + AppController.currentPage + "/" + mode + "/" + AppController.previewToken
    }

    // previewToken bumps when pages finish loading 鈥?re-evaluate readiness
    readonly property bool currentPageReady: {
        const _token = AppController.previewToken
        return AppController.isPageLoaded(AppController.currentPage)
    }

    // Convert position in pageLayer coords -> normalized image coords (0..1)
    function normFromLayer(lx, ly) {
        return Qt.point(
            (lx - pageLayer.offsetX) / pageLayer.paintW,
            (ly - pageLayer.offsetY) / pageLayer.paintH)
    }

    function clampScroll() {
        const f = scrollHost.flickable
        if (!f)
            return
        const maxX = scrollHost.scrollMaxX()
        const maxY = scrollHost.scrollMaxY()
        f.contentX = Math.max(0, Math.min(maxX, f.contentX))
        f.contentY = Math.max(0, Math.min(maxY, f.contentY))
    }

    function resetScrollPosition() {
        const f = scrollHost.flickable
        if (!f)
            return
        const maxX = scrollHost.scrollMaxX()
        const maxY = scrollHost.scrollMaxY()
        f.contentX = maxX > 0 ? maxX / 2 : 0
        f.contentY = maxY > 0 ? maxY / 2 : 0
        clampScroll()
    }

    function scrollByWheel(deltaY) {
        const f = scrollHost.flickable
        if (!f || !scrollHost.canScrollY)
            return false
        const step = deltaY / 120 * 48
        f.contentY = Math.max(0, Math.min(scrollHost.scrollMaxY(), f.contentY - step))
        return true
    }

    Text {
        anchors.centerIn: parent
        visible: !AppController.hasPreview && !AppController.processing
        text: AppController.fileCount === 0
              ? Theme.tr("noFilesEmpty")
              : Theme.tr("loadingPreview")
        color: Theme.textSecondary
        font: Theme.mainFont
        width: parent.width - 40
        wrapMode: Text.Wrap
        horizontalAlignment: Text.AlignHCenter
    }

    Item {
        id: scrollHost
        anchors.fill: parent
        anchors.margins: 4
        visible: AppController.hasPreview
        clip: true

        readonly property Flickable flickable: scroll.contentItem
        readonly property bool canScrollY: flickable
                && flickable.contentHeight > flickable.height + 1
        readonly property bool canScrollX: flickable
                && flickable.contentWidth > flickable.width + 1

        function scrollMaxX() {
            return flickable ? Math.max(0, flickable.contentWidth - flickable.width) : 0
        }
        function scrollMaxY() {
            return flickable ? Math.max(0, flickable.contentHeight - flickable.height) : 0
        }

        ScrollView {
            id: scroll
            anchors.fill: parent
            clip: true
            ScrollBar.horizontal.policy: ScrollBar.AsNeeded
            ScrollBar.vertical.policy: ScrollBar.AsNeeded

            // Left-drag is used for draw/select; wheel + right-drag still pan/zoom.
            Binding {
                target: scroll.contentItem
                property: "interactive"
                value: AppController.toolMode !== "select"
                       && AppController.toolMode !== "draw"
                       && AppController.toolMode !== "fixedDraw"
                       && canvas.editRegionId < 0
                       && !canvas.drawing
            }

            contentWidth: pageLayer.width
            contentHeight: pageLayer.height

            WheelHandler {
                acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
                onWheel: function(event) {
                    if (event.modifiers & Qt.ControlModifier) {
                        const delta = event.angleDelta.y > 0 ? 0.1 : -0.1
                        AppController.previewZoom = AppController.previewZoom + delta
                        event.accepted = true
                        return
                    }
                    if (canvas.scrollByWheel(event.angleDelta.y))
                        event.accepted = true
                }
            }

            Item {
                id: pageLayer

            // Declarative layout: no circular dependencies, stays correct at
            // any zoom level (fixes drawing being broken after zooming).
            // Fit against the fixed canvas size, NOT the ScrollView viewport:
            // scrollbars change the viewport size and would cause a binding loop.
            readonly property real availW: Math.max(1, canvas.width - 24)
            readonly property real availH: Math.max(1, canvas.height - 24)
            readonly property real zoom: AppController.previewZoom
            readonly property real srcW: {
                const _ = AppController.previewToken
                const w = AppController.currentPageWidth
                if (w > 0)
                    return w
                return pageImage.sourceSize.width > 0 ? pageImage.sourceSize.width : 595
            }
            readonly property real srcH: {
                const _ = AppController.previewToken
                const h = AppController.currentPageHeight
                if (h > 0)
                    return h
                return pageImage.sourceSize.height > 0 ? pageImage.sourceSize.height : 842
            }
            readonly property real fitScale: (srcW <= 0 || srcH <= 0)
                ? 1 : Math.min(availW / srcW, availH / srcH)
            readonly property real paintW: srcW * fitScale * zoom
            readonly property real paintH: srcH * fitScale * zoom
            readonly property real offsetX: Math.max(12, (availW - paintW) / 2)
            readonly property real offsetY: Math.max(12, (availH - paintH) / 2)

            width: paintW + offsetX * 2
            height: paintH + offsetY * 2

            Image {
                id: pageImage
                x: pageLayer.offsetX
                y: pageLayer.offsetY
                width: pageLayer.paintW
                height: pageLayer.paintH
                fillMode: Image.PreserveAspectFit
                smooth: true
                mipmap: true
                asynchronous: true
                cache: false
                source: AppController.hasPreview && canvas.currentPageReady
                        ? canvas.previewSource() : ""
            }

            Rectangle {
                anchors.centerIn: pageImage
                width: pageImage.width
                height: pageImage.height
                visible: AppController.hasPreview
                         && !canvas.currentPageReady
                         && !AppController.processing
                color: Theme.surface
                opacity: 0.92

                BusyIndicator {
                    anchors.centerIn: parent
                    running: parent.visible
                }

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.top: parent.verticalCenter
                    anchors.topMargin: 28
                    text: Theme.tr("pageLoading")
                    color: Theme.textSecondary
                    font: Theme.captionFont
                }
            }

            // Auto / manual region overlays (on raw preview; hidden when viewing baked mask)
            Repeater {
                model: AppController.redactions
                delegate: Item {
                    required property int regionId
                    required property real rx
                    required property real ry
                    required property real rw
                    required property real rh
                    required property string source

                    readonly property bool isSelected: AppController.redactions.selectedId === regionId

                    readonly property bool isEditing: canvas.editRegionId === regionId
                    readonly property real curRx: isEditing ? canvas.editRx : rx
                    readonly property real curRy: isEditing ? canvas.editRy : ry
                    readonly property real curRw: isEditing ? canvas.editRw : rw
                    readonly property real curRh: isEditing ? canvas.editRh : rh

                    visible: !AppController.showMaskedPreview
                    x: pageLayer.offsetX + curRx * pageLayer.paintW
                    y: pageLayer.offsetY + curRy * pageLayer.paintH
                    width: curRw * pageLayer.paintW
                    height: curRh * pageLayer.paintH

                    Rectangle {
                        anchors.fill: parent
                        color: source === "fixed" ? "#6366F133"
                              : (source === "manual" ? Theme.maskManualFill : Theme.maskAutoFill)
                        border.width: source === "auto" ? 2 : (isSelected ? 2 : 1)
                        border.color: source === "fixed" ? "#6366F1"
                                    : (source === "manual" ? Theme.maskManual : Theme.maskAuto)
                    }

                    Text {
                        anchors.left: parent.left
                        anchors.top: parent.top
                        anchors.margins: 2
                        text: source === "auto" ? "A" : (source === "fixed" ? "F" : "M")
                        font.pixelSize: 10
                        color: Theme.text
                        visible: parent.height > 14
                    }

                    MouseArea {
                        id: dragArea
                        anchors.left: parent.left
                        anchors.top: parent.top
                        anchors.right: resizeHit.left
                        anchors.bottom: resizeHit.top
                        enabled: AppController.toolMode === "select"
                                 && !canvas.exportBusy
                        cursorShape: Qt.PointingHandCursor
                        preventStealing: true
                        property real grabLayerX
                        property real grabLayerY
                        onClicked: AppController.redactions.selectedId = regionId
                        onPressed: function(mouse) {
                            AppController.redactions.selectedId = regionId
                            canvas.beginEdit(regionId, rx, ry, rw, rh)
                            const p = mapToItem(pageLayer, mouse.x, mouse.y)
                            grabLayerX = p.x - parent.x
                            grabLayerY = p.y - parent.y
                        }
                        onPositionChanged: function(mouse) {
                            if (!pressed)
                                return
                            const p = mapToItem(pageLayer, mouse.x, mouse.y)
                            const maxX = pageLayer.offsetX + pageLayer.paintW - parent.width
                            const maxY = pageLayer.offsetY + pageLayer.paintH - parent.height
                            const lx = Math.max(pageLayer.offsetX,
                                                Math.min(maxX, p.x - grabLayerX))
                            const ly = Math.max(pageLayer.offsetY,
                                                Math.min(maxY, p.y - grabLayerY))
                            const rect = canvas.clampNormRect(
                                (lx - pageLayer.offsetX) / pageLayer.paintW,
                                (ly - pageLayer.offsetY) / pageLayer.paintH,
                                canvas.editRw, canvas.editRh)
                            canvas.editRx = rect.x
                            canvas.editRy = rect.y
                        }
                        onReleased: {
                            AppController.redactions.updateRect(
                                regionId, canvas.editRx, canvas.editRy,
                                canvas.editRw, canvas.editRh)
                            canvas.endEdit()
                        }
                        onCanceled: canvas.endEdit()
                    }

                    Item {
                        id: resizeHit
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        anchors.rightMargin: -8
                        anchors.bottomMargin: -8
                        visible: isSelected && AppController.toolMode === "select"
                        width: visible ? 20 : 0
                        height: visible ? 20 : 0
                        z: 21

                        Rectangle {
                            anchors.centerIn: parent
                            width: 12
                            height: 12
                            radius: 2
                            color: Theme.accent
                        }

                        MouseArea {
                            anchors.fill: parent
                            anchors.margins: -6
                            cursorShape: Qt.SizeFDiagCursor
                            preventStealing: true
                            property real originX
                            property real originY
                            property real startNormW
                            property real startNormH
                            onPressed: function(mouse) {
                                AppController.redactions.selectedId = regionId
                                canvas.beginEdit(regionId, rx, ry, rw, rh)
                                const p = mapToItem(pageLayer, mouse.x, mouse.y)
                                originX = p.x
                                originY = p.y
                                startNormW = rw
                                startNormH = rh
                            }
                            onPositionChanged: function(mouse) {
                                if (!pressed)
                                    return
                                const p = mapToItem(pageLayer, mouse.x, mouse.y)
                                const rect = canvas.clampNormRect(
                                    canvas.editRx, canvas.editRy,
                                    startNormW + (p.x - originX) / pageLayer.paintW,
                                    startNormH + (p.y - originY) / pageLayer.paintH)
                                canvas.editRw = rect.w
                                canvas.editRh = rect.h
                            }
                            onReleased: {
                                AppController.redactions.updateRect(
                                    regionId, canvas.editRx, canvas.editRy,
                                    canvas.editRw, canvas.editRh)
                                canvas.endEdit()
                            }
                            onCanceled: canvas.endEdit()
                        }
                    }
                }
            }

            Rectangle {
                visible: canvas.drawing
                x: canvas.draftX
                y: canvas.draftY
                width: canvas.draftW
                height: canvas.draftH
                color: AppController.toolMode === "fixedDraw" ? "#6366F133" : Theme.maskManualDraft
                border.color: AppController.toolMode === "fixedDraw" ? "#6366F1" : Theme.maskManual
                border.width: 2
            }

            MouseArea {
                anchors.fill: parent
                enabled: (AppController.toolMode === "draw"
                          || AppController.toolMode === "fixedDraw")
                         && !AppController.showMaskedPreview
                         && !canvas.exportBusy
                cursorShape: Qt.CrossCursor
                preventStealing: true
                z: 100
                onPressed: function(mouse) {
                    canvas.drawing = true
                    canvas.drawStartX = mouse.x
                    canvas.drawStartY = mouse.y
                    canvas.draftX = mouse.x
                    canvas.draftY = mouse.y
                    canvas.draftW = 0
                    canvas.draftH = 0
                }
                onPositionChanged: function(mouse) {
                    if (!canvas.drawing) return
                    canvas.draftX = Math.min(canvas.drawStartX, mouse.x)
                    canvas.draftY = Math.min(canvas.drawStartY, mouse.y)
                    canvas.draftW = Math.abs(mouse.x - canvas.drawStartX)
                    canvas.draftH = Math.abs(mouse.y - canvas.drawStartY)
                }
                onReleased: function(mouse) {
                    if (!canvas.drawing) return
                    canvas.drawing = false
                    if (canvas.draftW < 6 || canvas.draftH < 6) return

                    const p1 = canvas.normFromLayer(canvas.draftX, canvas.draftY)
                    const p2 = canvas.normFromLayer(canvas.draftX + canvas.draftW,
                                                    canvas.draftY + canvas.draftH)
                    const nx = Math.min(p1.x, p2.x)
                    const ny = Math.min(p1.y, p2.y)
                    const nw = Math.abs(p2.x - p1.x)
                    const nh = Math.abs(p2.y - p1.y)
                    if (AppController.toolMode === "fixedDraw") {
                        AppController.redactions.addFixed(
                            AppController.previewFilePath, nx, ny, nw, nh)
                    } else {
                        AppController.redactions.addManual(
                            AppController.currentPage, nx, ny, nw, nh)
                    }
                }
            }
        }
        }

        MouseArea {
            id: panArea
            anchors.fill: parent
            acceptedButtons: Qt.RightButton
            enabled: AppController.hasPreview && !AppController.processing && !canvas.exportBusy
            preventStealing: true
            z: 5
            cursorShape: pressed ? Qt.ClosedHandCursor : Qt.ArrowCursor
            hoverEnabled: true

            property real panStartX: 0
            property real panStartY: 0
            property real panStartContentX: 0
            property real panStartContentY: 0

            onPressed: function(mouse) {
                const f = scrollHost.flickable
                if (!f)
                    return
                panStartX = mouse.x
                panStartY = mouse.y
                panStartContentX = f.contentX
                panStartContentY = f.contentY
            }
            onPositionChanged: function(mouse) {
                if (!pressed)
                    return
                const f = scrollHost.flickable
                if (!f)
                    return
                f.contentX = Math.max(0, Math.min(scrollHost.scrollMaxX(),
                                                   panStartContentX - (mouse.x - panStartX)))
                f.contentY = Math.max(0, Math.min(scrollHost.scrollMaxY(),
                                                   panStartContentY - (mouse.y - panStartY)))
            }
        }
    }

    // Busy overlay while loading (not during export 鈥?export keeps preview interactive)
    Rectangle {
        anchors.fill: parent
        radius: parent.radius
        visible: AppController.processing && !canvas.exportBusy
        color: Theme.surface
        opacity: 0.88

        BusyIndicator {
            anchors.centerIn: parent
            anchors.verticalCenterOffset: -20
            running: AppController.processing
        }

        Text {
            anchors.centerIn: parent
            anchors.verticalCenterOffset: 28
            text: AppController.taskLabel.length > 0
                  ? AppController.taskLabel
                  : Theme.tr("loadingPreview")
            color: Theme.textBody
            font: Theme.mainFont
        }
    }
}
