import QtQuick
import QtQuick.Controls
import ProjectO

Item {
    id: root
    implicitHeight: 36

    property int pathDirection: 0
    property int _prevCrumbCount: 0
    property alias pathField: pathField

    property bool crumbsExpanded: false
    readonly property bool crumbsOverflow: crumbList.contentWidth > crumbList.width + 1

    function blurPath() {
        collapseCrumbs()
        if (pathField.activeFocus)
            pathField.focus = false
        pathField.text = FilePicker.pathInput
    }

    function snapCrumbsToEnd() {
        crumbXAnim.stop()
        crumbsExpanded = false
        crumbList.contentX = Math.max(0, crumbList.contentWidth - crumbList.width)
    }

    function expandCrumbs() {
        if (!crumbsOverflow)
            return
        crumbXAnim.stop()
        crumbsExpanded = true
        crumbXAnim.to = 0
        crumbXAnim.start()
    }

    function collapseCrumbs() {
        if (!crumbsExpanded)
            return
        crumbXAnim.stop()
        crumbsExpanded = false
        crumbXAnim.to = Math.max(0, crumbList.contentWidth - crumbList.width)
        crumbXAnim.start()
    }

    NumberAnimation {
        id: crumbXAnim
        target: crumbList
        property: "contentX"
        duration: Theme.animNormal
        easing.type: Easing.OutCubic
    }

    function syncBreadcrumbs() {
        crumbsExpanded = false
        Qt.callLater(snapCrumbsToEnd)
        const crumbs = FilePicker.breadcrumb
        if (pathDirection > 0 && crumbs.length > crumbModel.count) {
            for (let i = crumbModel.count; i < crumbs.length; ++i)
                crumbModel.append({ label: crumbs[i].label, path: crumbs[i].path })
            return
        }
        if (pathDirection < 0 && crumbs.length < crumbModel.count) {
            while (crumbModel.count > crumbs.length)
                crumbModel.remove(crumbModel.count - 1)
            return
        }
        if (crumbModel.count !== crumbs.length) {
            crumbModel.clear()
            for (let i = 0; i < crumbs.length; ++i)
                crumbModel.append({ label: crumbs[i].label, path: crumbs[i].path })
        } else {
            for (let i = 0; i < crumbs.length; ++i) {
                if (crumbModel.get(i).label !== crumbs[i].label
                        || crumbModel.get(i).path !== crumbs[i].path) {
                    crumbModel.set(i, { label: crumbs[i].label, path: crumbs[i].path })
                }
            }
        }
    }

    Timer {
        id: directionReset
        interval: Theme.animNormal + 20
        repeat: false
        onTriggered: root.pathDirection = 0
    }

    onPathDirectionChanged: {
        if (pathDirection !== 0)
            directionReset.restart()
    }

    Item {
        id: pathBox
        anchors.fill: parent
        clip: true

        Rectangle {
            anchors.fill: parent
            radius: Theme.radiusSm
            color: Theme.surfaceAlt
            border.color: Theme.border
            border.width: 1
        }

        Rectangle {
            anchors.fill: parent
            radius: Theme.radiusSm
            color: "transparent"
            border.color: Theme.accent
            border.width: 2
            opacity: pathField.activeFocus ? 1 : 0
            z: 2

            Behavior on opacity {
                NumberAnimation {
                    duration: Theme.animNormal
                    easing.type: Easing.OutCubic
                }
            }
        }

        ListModel {
            id: crumbModel
        }

        ListView {
            id: crumbList
            anchors.fill: parent
            anchors.margins: 6
            orientation: ListView.Horizontal
            spacing: 2
            clip: true
            model: crumbModel
            visible: !pathField.activeFocus
            boundsBehavior: Flickable.StopAtBounds
            interactive: false

            onContentWidthChanged: if (!root.crumbsExpanded && !crumbXAnim.running)
                                       root.snapCrumbsToEnd()
            onWidthChanged: if (!root.crumbsExpanded && !crumbXAnim.running)
                                root.snapCrumbsToEnd()

            add: Transition {
                enabled: root.pathDirection > 0
                ParallelAnimation {
                    NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Theme.animNormal; easing.type: Easing.OutCubic }
                    NumberAnimation { property: "scale"; from: 0.9; to: 1; duration: Theme.animNormal; easing.type: Easing.OutCubic }
                }
            }

            remove: Transition {
                enabled: root.pathDirection < 0
                ParallelAnimation {
                    NumberAnimation { property: "opacity"; to: 0; duration: Theme.animFast; easing.type: Easing.OutCubic }
                    NumberAnimation { property: "scale"; to: 0.9; duration: Theme.animFast; easing.type: Easing.OutCubic }
                }
            }

            displaced: Transition {
                NumberAnimation { properties: "x"; duration: Theme.animNormal; easing.type: Easing.OutCubic }
            }

            delegate: Item {
                id: crumbItem
                height: crumbList.height
                width: crumbRow.width
                opacity: 1
                scale: 1
                transformOrigin: Item.Left

                required property int index
                required property string label
                required property string path

                Row {
                    id: crumbRow
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 4

                    Text {
                        visible: index > 0
                        anchors.verticalCenter: parent.verticalCenter
                        text: "›"
                        color: Theme.textSecondary
                        font.family: Theme.captionFont.family
                        font.pixelSize: 14
                    }

                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: label
                        font.family: Theme.captionFont.family
                        font.pixelSize: 14
                        color: crumbMouse.containsMouse ? Theme.accent : Theme.textBody

                        MouseArea {
                            id: crumbMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: FilePicker.navigateTo(path)
                        }
                    }
                }
            }
        }

        Rectangle {
            id: ellipsisChip
            anchors.left: parent.left
            anchors.leftMargin: 1
            anchors.top: parent.top
            anchors.topMargin: 1
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 1
            width: 28
            radius: Theme.radiusSm
            color: Theme.surfaceAlt
            visible: crumbList.visible && root.crumbsOverflow && !root.crumbsExpanded

            Row {
                anchors.centerIn: parent
                spacing: 2

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: "…"
                    font.family: Theme.captionFont.family
                    font.pixelSize: 14
                    color: ellipsisMouse.containsMouse ? Theme.accent : Theme.textBody
                }

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: "›"
                    font.family: Theme.captionFont.family
                    font.pixelSize: 14
                    color: Theme.textSecondary
                }
            }

            MouseArea {
                id: ellipsisMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: root.expandCrumbs()
            }
        }

        TextField {
            id: pathField
            anchors.fill: parent
            topPadding: 0
            bottomPadding: 0
            leftPadding: 8
            rightPadding: 8
            verticalAlignment: TextInput.AlignVCenter
            font.family: Theme.captionFont.family
            font.pixelSize: 14
            color: Theme.textBody
            selectByMouse: true
            visible: activeFocus
            background: Rectangle { color: "transparent" }

            Keys.onPressed: function(event) {
                if ((event.modifiers & Qt.ControlModifier) && event.key === Qt.Key_V) {
                    event.accepted = false
                    return
                }
                if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                    FilePicker.navigateToInput(text)
                    event.accepted = true
                }
            }

            onAccepted: FilePicker.navigateToInput(text)
            onEditingFinished: {
                pathField.text = FilePicker.pathInput
            }
            Keys.onEscapePressed: {
                pathField.text = FilePicker.pathInput
                pathField.focus = false
            }
        }

        MouseArea {
            anchors.fill: parent
            visible: !pathField.activeFocus
            cursorShape: root.crumbsExpanded ? Qt.ArrowCursor : Qt.IBeamCursor
            z: -1
            onClicked: {
                if (root.crumbsExpanded) {
                    root.collapseCrumbs()
                    return
                }
                pathField.text = FilePicker.pathInput
                pathField.forceActiveFocus()
                pathField.selectAll()
            }
        }
    }
}
