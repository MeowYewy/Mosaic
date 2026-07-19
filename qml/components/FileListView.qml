import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ProjectO

Rectangle {
    id: listRoot
    radius: Theme.radiusMd
    color: Theme.surfaceAlt
    border.color: Theme.border
    border.width: 1
    clip: true

    readonly property int rowHeight: 44
    readonly property int rowSpacing: 6
    readonly property int rowStride: rowHeight + rowSpacing
    readonly property int listMargin: 8

    property int dragIndex: -1
    property int insertAt: 0
    property real dragPointerY: 0
    property string dragLabel: ""
    property string dragCategoryLabel: ""

    function computeInsertAt(listView, x, y) {
        const count = listView.count
        if (count <= 0)
            return 0

        const contentY = y + listView.contentY
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

    function moveTarget(from, insertIndex, count) {
        if (from < 0 || count <= 0)
            return from
        let to = insertIndex
        if (to > from)
            to--
        return Math.max(0, Math.min(count - 1, to))
    }

    function endDrag() {
        if (dragIndex >= 0) {
            const target = moveTarget(dragIndex, insertAt, list.count)
            if (dragIndex !== target)
                AppController.files.move(dragIndex, target)
        }
        dragIndex = -1
        insertAt = 0
        dragLabel = ""
        dragCategoryLabel = ""
    }

    ListView {
        id: list
        anchors.fill: parent
        anchors.margins: listRoot.listMargin
        spacing: listRoot.rowSpacing
        model: AppController.files
        clip: true
        interactive: listRoot.dragIndex < 0

        moveDisplaced: Transition {
            NumberAnimation {
                properties: "y"
                duration: Theme.animNormal
                easing.type: Easing.OutCubic
            }
        }

        delegate: Rectangle {
            id: row
            required property int index
            required property string name
            required property string categoryLabel
            required property string path

            width: list.width
            height: listRoot.rowHeight
            radius: Theme.radiusSm
            color: Theme.surface
            border.color: row.isSelected ? Theme.accent : Theme.border
            border.width: row.isSelected ? 2 : 1

            readonly property bool isDragSource: listRoot.dragIndex === index
            readonly property bool isSelected:
                AppController.previewFilePath.length > 0
                && AppController.previewFilePath === row.path

            opacity: isDragSource ? 0.28 : 1.0
            Behavior on opacity { NumberAnimation { duration: Theme.animFast } }

            Rectangle {
                anchors.fill: parent
                radius: parent.radius
                visible: isDragSource
                color: "transparent"
                border.color: Theme.border
                border.width: 1
            }

            MouseArea {
                anchors.fill: parent
                anchors.leftMargin: 28
                anchors.rightMargin: 32
                enabled: listRoot.dragIndex < 0
                cursorShape: Qt.PointingHandCursor
                onClicked: AppController.jumpToFile(row.path)
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 8
                anchors.rightMargin: 4
                spacing: 8

                DragGrip {
                    Layout.alignment: Qt.AlignVCenter
                    lineOpacity: gripArea.pressed ? 0.7 : 0.35

                    MouseArea {
                        id: gripArea
                        anchors.fill: parent
                        cursorShape: Qt.ClosedHandCursor
                        preventStealing: true
                        onPressed: function(mouse) {
                            listRoot.dragIndex = row.index
                            listRoot.insertAt = row.index
                            listRoot.dragLabel = row.name
                            listRoot.dragCategoryLabel = row.categoryLabel
                            const rootPos = mapToItem(listRoot, mouse.x, mouse.y)
                            listRoot.dragPointerY = rootPos.y
                            mouse.accepted = true
                        }
                        onPositionChanged: function(mouse) {
                            if (!pressed)
                                return
                            const listPos = mapToItem(list, mouse.x, mouse.y)
                            const rootPos = mapToItem(listRoot, mouse.x, mouse.y)
                            listRoot.dragPointerY = rootPos.y
                            listRoot.insertAt = listRoot.computeInsertAt(list, listPos.x, listPos.y)
                        }
                        onReleased: listRoot.endDrag()
                        onCanceled: listRoot.endDrag()
                    }
                }

                Rectangle {
                    Layout.alignment: Qt.AlignVCenter
                    Layout.preferredWidth: catText.implicitWidth + 12
                    Layout.preferredHeight: 22
                    radius: 11
                    color: Theme.menuHover
                    Text {
                        id: catText
                        anchors.centerIn: parent
                        text: row.categoryLabel
                        font: Theme.captionBoldFont
                        color: Theme.accent
                    }
                }

                Text {
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignVCenter
                    elide: Text.ElideMiddle
                    text: row.name
                    font: Theme.mainFont
                    color: Theme.text
                }

                IconDeleteButton {
                    Layout.alignment: Qt.AlignVCenter
                    enabled: listRoot.dragIndex < 0
                    onClicked: AppController.removeFileAt(row.index)
                }
            }
        }

        Text {
            anchors.centerIn: parent
            visible: list.count === 0
            text: Theme.tr("noFilesEmpty")
            color: Theme.textSecondary
            font: Theme.captionFont
            width: parent.width - 24
            wrapMode: Text.Wrap
            horizontalAlignment: Text.AlignHCenter
        }
    }

    Rectangle {
        id: insertLine
        x: listRoot.listMargin
        width: list.width
        height: 2
        radius: 1
        color: Theme.accent
        opacity: 0.55
        visible: listRoot.dragIndex >= 0
        y: listRoot.listMargin + listRoot.insertAt * listRoot.rowStride - list.contentY - 1
        z: 50

        Behavior on y {
            NumberAnimation { duration: 120; easing.type: Easing.OutCubic }
        }
    }

    Rectangle {
        id: dragGhost
        x: listRoot.listMargin
        width: list.width
        height: listRoot.rowHeight
        radius: Theme.radiusSm
        color: Theme.surface
        border.color: Theme.accent
        border.width: 1
        visible: listRoot.dragIndex >= 0
        y: listRoot.dragPointerY - height / 2
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
            anchors.leftMargin: 8
            anchors.rightMargin: 4
            spacing: 8

            Item { Layout.preferredWidth: 20; Layout.preferredHeight: 20 }

            Rectangle {
                Layout.alignment: Qt.AlignVCenter
                Layout.preferredWidth: ghostCat.implicitWidth + 12
                Layout.preferredHeight: 22
                radius: 11
                color: Theme.menuHover
                Text {
                    id: ghostCat
                    anchors.centerIn: parent
                    text: listRoot.dragCategoryLabel
                    font: Theme.captionBoldFont
                    color: Theme.accent
                }
            }

            Text {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignVCenter
                text: listRoot.dragLabel
                elide: Text.ElideMiddle
                font: Theme.mainFont
                color: Theme.text
            }

            Item { Layout.preferredWidth: 24; Layout.preferredHeight: 24 }
        }
    }
}
