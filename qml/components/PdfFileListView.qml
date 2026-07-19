import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ProjectO

Item {
    id: root

    property bool enableReorder: false
    readonly property int rowHeight: 36
    readonly property int rowSpacing: 4
    readonly property int rowStride: rowHeight + rowSpacing
    readonly property int deleteBtnWidth: 24
    readonly property int gripWidth: enableReorder ? 24 : 0

    property int dragIndex: -1
    property int insertAt: 0
    property real dragPointerY: 0
    property string dragLabel: ""

    function computeInsertAt(list, x, y) {
        const count = list.count
        if (count <= 0)
            return 0

        const contentY = y + list.contentY
        if (contentY <= rowHeight * 0.5)
            return 0
        if (contentY >= count * rowStride - rowSpacing - rowHeight * 0.5)
            return count

        const band = contentY / rowStride
        const base = Math.floor(band)
        const frac = band - base
        if (frac < 0.5)
            return Math.max(0, Math.min(count, base))
        return Math.max(0, Math.min(count, base + 1))
    }

    function moveTarget(from, insertAt, count) {
        if (from < 0 || count <= 0)
            return from
        let to = insertAt
        if (to > from)
            to--
        return Math.max(0, Math.min(count - 1, to))
    }

    function endDrag() {
        if (root.dragIndex >= 0) {
            const target = moveTarget(root.dragIndex, root.insertAt, fileList.count)
            if (root.dragIndex !== target)
                PdfController.moveFile(root.dragIndex, target)
        }
        root.dragIndex = -1
        root.insertAt = 0
        root.dragLabel = ""
    }

    ListView {
        id: fileList
        anchors.fill: parent
        clip: true
        model: PdfController.files
        spacing: root.rowSpacing
        interactive: root.dragIndex < 0

        moveDisplaced: Transition {
            NumberAnimation {
                properties: "y"
                duration: Theme.animNormal
                easing.type: Easing.OutCubic
            }
        }

        delegate: Item {
            id: rowRoot
            required property int index
            required property string path
            required property string name

            width: fileList.width
            height: root.rowHeight

            readonly property bool isSelected:
                PdfController.preview.currentFile === path
            readonly property bool isDragSource: root.dragIndex === index

            opacity: isDragSource ? 0.28 : 1.0
            Behavior on opacity { NumberAnimation { duration: Theme.animFast } }

            Rectangle {
                anchors.fill: parent
                radius: Theme.radiusSm
                color: isSelected ? Theme.surface : "transparent"
                border.color: isSelected ? Theme.border : "transparent"
                border.width: isSelected ? 1 : 0
            }

            Rectangle {
                anchors.fill: parent
                radius: Theme.radiusSm
                visible: isDragSource
                color: "transparent"
                border.color: Theme.border
                border.width: 1
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 4
                anchors.rightMargin: root.enableReorder ? 2 : 6
                spacing: 4

                IconDeleteButton {
                    Layout.preferredWidth: root.deleteBtnWidth
                    Layout.preferredHeight: root.deleteBtnWidth
                    Layout.alignment: Qt.AlignVCenter
                    onClicked: PdfController.removeFileAt(index)
                }

                Text {
                    Layout.fillWidth: true
                    Layout.minimumWidth: 40
                    text: name
                    elide: Text.ElideRight
                    color: isSelected ? Theme.text : Theme.textBody
                    font: Theme.mainFont
                }

                // Chosen page range (gray, small). Only shown once at least
                // one file has a range; files without one read "All".
                Text {
                    id: rangeBadge
                    visible: PdfController.currentTab <= 2 && PdfController.anyPageRangeSet
                    Layout.maximumWidth: 84
                    Layout.alignment: Qt.AlignVCenter
                    text: {
                        const r = PdfController.pageRanges[rowRoot.path]
                        return (r && r.length > 0) ? r : "All"
                    }
                    elide: Text.ElideRight
                    horizontalAlignment: Text.AlignRight
                    color: Theme.textSecondary
                    font: Theme.captionFont
                }

                DragGrip {
                    visible: root.enableReorder
                    Layout.preferredWidth: root.gripWidth
                    Layout.alignment: Qt.AlignVCenter
                    lineOpacity: gripArea.pressed ? 0.7 : 0.35
                }
            }

            MouseArea {
                id: selectArea
                anchors.fill: parent
                anchors.leftMargin: root.deleteBtnWidth + 8
                anchors.rightMargin: root.enableReorder ? root.gripWidth + 4 : 0
                cursorShape: Qt.PointingHandCursor
                enabled: root.dragIndex < 0
                onClicked: PdfController.selectPreviewFile(path)
            }

            MouseArea {
                id: gripArea
                visible: root.enableReorder
                width: root.gripWidth + 4
                height: parent.height
                anchors.right: parent.right
                cursorShape: Qt.ClosedHandCursor

                onPressed: function(mouse) {
                    root.dragIndex = index
                    root.insertAt = index
                    root.dragLabel = name
                    const rootPos = mapToItem(root, mouse.x, mouse.y)
                    root.dragPointerY = rootPos.y
                    mouse.accepted = true
                }
                onPositionChanged: function(mouse) {
                    if (!pressed)
                        return
                    const listPos = mapToItem(fileList, mouse.x, mouse.y)
                    const rootPos = mapToItem(root, mouse.x, mouse.y)
                    root.dragPointerY = rootPos.y
                    root.insertAt = root.computeInsertAt(fileList, listPos.x, listPos.y)
                }
                onReleased: root.endDrag()
                onCanceled: root.endDrag()
            }
        }

        Label {
            anchors.centerIn: parent
            visible: fileList.count === 0
            text: Theme.tr("noFilesEmpty")
            color: Theme.textBody
            font: Theme.mainFont
        }
    }

    Rectangle {
        id: insertLine
        width: parent.width
        height: 2
        radius: 1
        color: Theme.accent
        opacity: 0.55
        visible: root.enableReorder && root.dragIndex >= 0
        y: root.insertAt * root.rowStride - fileList.contentY - 1
        z: 50

        Behavior on y {
            NumberAnimation { duration: 120; easing.type: Easing.OutCubic }
        }
    }

    Rectangle {
        id: dragGhost
        width: parent.width
        height: root.rowHeight
        radius: Theme.radiusSm
        color: Theme.surface
        border.color: Theme.accent
        border.width: 1
        visible: root.enableReorder && root.dragIndex >= 0
        y: root.dragPointerY - height / 2
        z: 100

        Rectangle {
            anchors.fill: parent
            anchors.topMargin: 4
            radius: parent.radius
            color: Theme.shadowColor
            opacity: Theme.shadowOpacity1
            z: -1
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 4
            anchors.rightMargin: root.enableReorder ? 2 : 6
            spacing: 4

            Item { Layout.preferredWidth: root.deleteBtnWidth }

            Text {
                Layout.fillWidth: true
                verticalAlignment: Text.AlignVCenter
                text: root.dragLabel
                elide: Text.ElideRight
                color: Theme.text
                font: Theme.mainFont
            }

            Item {
                visible: root.enableReorder
                Layout.preferredWidth: root.gripWidth
            }
        }
    }
}
