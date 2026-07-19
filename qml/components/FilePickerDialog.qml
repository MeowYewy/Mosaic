import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ProjectO

Item {
    id: root
    z: 3200
    visible: FilePicker.shown

    property bool shown: FilePicker.shown
    property real overlayOpacity: shown ? Theme.dimOpacity : 0

    Behavior on overlayOpacity {
        NumberAnimation { duration: Theme.animNormal; easing.type: Easing.OutCubic }
    }

    Rectangle {
        anchors.fill: parent
        color: Theme.dimOverlay
        opacity: root.overlayOpacity

        MouseArea {
            anchors.fill: parent
            onClicked: FilePicker.reject()
            // Swallow wheel events so the main window behind never scrolls.
            onWheel: function(wheel) { wheel.accepted = true }
        }
    }

    Item {
        id: dialogLayer
        anchors.centerIn: parent
        width: card.width
        height: card.height
        scale: root.shown ? 1 : 0.94
        opacity: root.shown ? 1 : 0
        transformOrigin: Item.Center

        Behavior on scale {
            NumberAnimation {
                duration: Theme.animSlow
                easing.type: Easing.OutBack
                easing.overshoot: 1.06
            }
        }
        Behavior on opacity {
            NumberAnimation { duration: Theme.animNormal; easing.type: Easing.OutCubic }
        }

        Rectangle {
            anchors.fill: card
            anchors.topMargin: Theme.shadowOffset2
            radius: card.radius
            color: Theme.shadowColor
            opacity: Theme.shadowOpacity2
        }

        Rectangle {
            anchors.fill: card
            anchors.topMargin: Theme.shadowOffset1
            radius: card.radius
            color: Theme.shadowColor
            opacity: Theme.shadowOpacity1
        }

        Rectangle {
            id: card
            width: 760
            height: 520
            radius: Theme.radiusLg
            color: Theme.surface
            border.color: Theme.border
            border.width: 1
            clip: true

            MouseArea {
                anchors.fill: parent
                onClicked: {
                    filePanel.blurSearch()
                    pathBar.collapseCrumbs()
                }
                // Inner lists sit above this area and keep their own wheel
                // scrolling; anything else on the card must not leak through.
                onWheel: function(wheel) { wheel.accepted = true }
            }

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 20
                // Leave room for the split filename preview under the bottom bar.
                anchors.bottomMargin: bottomBar.splitMode ? 38 : 20
                spacing: 14

                // Main body: sidebar + file area
                RowLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    spacing: 14

                    // ---- Sidebar ----
                    ColumnLayout {
                        Layout.preferredWidth: 156
                        Layout.maximumWidth: 156
                        Layout.fillHeight: true
                        spacing: 8

                        Text {
                            Layout.preferredWidth: 156
                            Layout.preferredHeight: 28
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                            text: FilePicker.title
                            font.pixelSize: 18
                            font.family: Theme.mainFont.family
                            font.weight: Font.DemiBold
                            color: Theme.text
                            elide: Text.ElideRight
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            radius: Theme.radiusMd
                            color: Theme.surfaceAlt
                            border.color: Theme.border
                            border.width: 1
                            clip: true

                            ListView {
                                id: placesList
                                anchors.fill: parent
                                anchors.margins: 6
                                spacing: 2
                                model: FilePicker.places
                                boundsBehavior: Flickable.StopAtBounds

                                delegate: Rectangle {
                                    required property string label
                                    required property string path
                                    required property string iconKind

                                    width: placesList.width
                                    height: 34
                                    radius: Theme.radiusSm
                                    color: placeMouse.containsMouse ? Theme.menuHover : "transparent"
                                    scale: placeMouse.pressed ? 0.97 : 1

                                    Behavior on color {
                                        ColorAnimation { duration: Theme.animFast; easing.type: Easing.OutCubic }
                                    }
                                    Behavior on scale {
                                        NumberAnimation { duration: Theme.animFast; easing.type: Easing.OutCubic }
                                    }

                                    RowLayout {
                                        anchors.fill: parent
                                        anchors.leftMargin: 8
                                        anchors.rightMargin: 6
                                        spacing: 8

                                        FileKindIcon {
                                            Layout.preferredWidth: 18
                                            Layout.preferredHeight: 18
                                            kind: iconKind
                                            tint: Theme.accent
                                        }

                                        Text {
                                            Layout.fillWidth: true
                                            text: label
                                            font: Theme.mainFont
                                            color: Theme.text
                                            elide: Text.ElideRight
                                        }
                                    }

                                    MouseArea {
                                        id: placeMouse
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: {
                                            filePanel.blurSearch()
                                            FilePicker.navigateTo(path)
                                        }
                                    }
                                }
                            }
                        }
                    }

                    // ---- Main file panel ----
                    ColumnLayout {
                        id: filePanel
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        spacing: 8

                        property bool creatingFolder: false
                        property string renamingPath: ""
                        property string renameOriginalName: ""
                        property Item renameFieldItem: null

                        function startRename(path, name) {
                            creatingFolder = false
                            renamingPath = path
                            renameOriginalName = name
                        }

                        function cancelRename() {
                            renamingPath = ""
                        }

                        function commitRename() {
                            if (renamingPath.length === 0 || folderExistsPopup.visible)
                                return
                            const field = renameFieldItem
                            if (!field) {
                                renamingPath = ""
                                return
                            }
                            const newName = field.text.trim()
                            if (newName.length === 0 || newName === renameOriginalName) {
                                renamingPath = ""
                                return
                            }
                            if (newName.toLowerCase() !== renameOriginalName.toLowerCase()
                                    && FilePicker.entryNameExists(newName)) {
                                folderExistsPopup.messageKey = "pickerNameExistsMessage"
                                folderExistsPopup.open()
                                return
                            }
                            if (FilePicker.renameEntry(renamingPath, newName))
                                renamingPath = ""
                        }

                        function startInlineNewFolder() {
                            creatingFolder = true
                            newFolderField.text = FilePicker.defaultNewFolderName()
                            entryList.contentY = 0
                            Qt.callLater(function() {
                                newFolderField.forceActiveFocus()
                                newFolderField.selectAll()
                            })
                        }

                        function commitInlineNewFolder() {
                            if (!creatingFolder || folderExistsPopup.visible)
                                return
                            const name = newFolderField.text.trim()
                            if (name.length === 0) {
                                cancelInlineNewFolder()
                                return
                            }
                            if (FilePicker.entryNameExists(name)) {
                                folderExistsPopup.messageKey = "pickerFolderExistsMessage"
                                folderExistsPopup.open()
                                return
                            }
                            if (FilePicker.createFolder(name)) {
                                creatingFolder = false
                                newFolderField.text = ""
                            }
                        }

                        function cancelInlineNewFolder() {
                            creatingFolder = false
                            newFolderField.text = ""
                        }

                        function blurSearch() {
                            if (searchField.activeFocus)
                                searchField.focus = false
                            pathBar.blurPath()
                            if (fileNameField.activeFocus)
                                fileNameField.focus = false
                            if (splitBaseField.activeFocus)
                                splitBaseField.focus = false
                            if (splitSepField.activeFocus)
                                splitSepField.focus = false
                        }

                        // Toolbar row 1: nav + breadcrumb
                        RowLayout {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 36
                            spacing: 6

                            FilePickerIconButton {
                                Layout.preferredWidth: 36
                                Layout.preferredHeight: 36
                                iconKind: "up"
                                enabled: FilePicker.canGoUp
                                onClicked: {
                                    filePanel.blurSearch()
                                    playUpEffect()
                                    FilePicker.navigateUp()
                                }
                            }

                            FilePickerIconButton {
                                id: refreshButton
                                Layout.preferredWidth: 36
                                Layout.preferredHeight: 36
                                iconKind: "refresh"
                                onClicked: {
                                    filePanel.blurSearch()
                                    playRefreshEffect()
                                    FilePicker.refresh()
                                }
                            }

                            PathBreadcrumbBar {
                                id: pathBar
                                Layout.fillWidth: true
                                Layout.preferredHeight: 36
                            }
                        }

                        // Toolbar row 2: new folder + hidden + search
                        RowLayout {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 36
                            spacing: 8

                            Rectangle {
                                Layout.preferredHeight: 32
                                Layout.preferredWidth: newFolderBtnRow.implicitWidth + 16
                                radius: Theme.radiusSm
                                color: newFolderMouse.containsMouse ? Theme.menuHover : Theme.surface
                                border.color: Theme.border
                                border.width: 1

                                RowLayout {
                                    id: newFolderBtnRow
                                    anchors.centerIn: parent
                                    spacing: 6

                                    FileKindIcon {
                                        Layout.preferredWidth: 16
                                        Layout.preferredHeight: 16
                                        kind: "folder"
                                        tint: Theme.accent
                                    }

                                    Text {
                                        text: Theme.tr("pickerNewFolder")
                                        font: Theme.mainFont
                                        color: Theme.textBody
                                    }
                                }

                                MouseArea {
                                    id: newFolderMouse
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: {
                                        filePanel.blurSearch()
                                        filePanel.startInlineNewFolder()
                                    }
                                }
                            }

                            Item { Layout.fillWidth: true }

                            Rectangle {
                                Layout.preferredHeight: 32
                                Layout.preferredWidth: hiddenRow.implicitWidth + 20
                                radius: Theme.radiusSm
                                color: Theme.surface
                                border.color: Theme.border
                                border.width: 1

                                RowLayout {
                                    id: hiddenRow
                                    anchors.centerIn: parent
                                    spacing: 8

                                    Text {
                                        text: Theme.tr("pickerShowHidden")
                                        color: Theme.textBody
                                        font: Theme.captionFont
                                    }

                                    SwitchToggle {
                                        id: hiddenSwitch
                                        checked: FilePicker.showHidden
                                        onToggled: function(value) {
                                            filePanel.blurSearch()
                                            FilePicker.showHidden = value
                                        }
                                    }
                                }
                            }

                            Item {
                                Layout.preferredWidth: 180
                                Layout.preferredHeight: 32

                                TextField {
                                    id: searchField
                                    anchors.fill: parent
                                    font: Theme.captionFont
                                    color: Theme.textBody
                                    placeholderText: Theme.tr("pickerSearch")
                                    placeholderTextColor: Theme.textSecondary
                                    selectByMouse: true
                                    leftPadding: 10
                                    rightPadding: searchClear.opacity > 0.01 ? 28 : 10
                                    topPadding: 0
                                    bottomPadding: 0
                                    verticalAlignment: TextInput.AlignVCenter
                                    onTextChanged: FilePicker.searchQuery = text
                                    Keys.onPressed: function(event) {
                                        if (event.key === Qt.Key_Escape) {
                                            focus = false
                                            event.accepted = true
                                        }
                                    }
                                    background: Item {
                                        Rectangle {
                                            anchors.fill: parent
                                            radius: Theme.radiusSm
                                            color: Theme.surface
                                            border.color: Theme.border
                                            border.width: 1
                                        }

                                        Rectangle {
                                            anchors.fill: parent
                                            radius: Theme.radiusSm
                                            color: "transparent"
                                            border.color: Theme.accent
                                            border.width: 2
                                            opacity: searchField.activeFocus ? 1 : 0

                                            Behavior on opacity {
                                                NumberAnimation {
                                                    duration: Theme.animNormal
                                                    easing.type: Easing.OutCubic
                                                }
                                            }
                                        }
                                    }
                                }

                                IconDeleteButton {
                                    id: searchClear
                                    anchors.right: parent.right
                                    anchors.rightMargin: 4
                                    anchors.verticalCenter: parent.verticalCenter
                                    focusPolicy: Qt.NoFocus
                                    opacity: searchField.text.length > 0 ? 1 : 0
                                    visible: opacity > 0.01
                                    enabled: searchField.text.length > 0
                                    onClicked: {
                                        searchField.clear()
                                        FilePicker.searchQuery = ""
                                    }

                                    Behavior on opacity {
                                        NumberAnimation {
                                            duration: Theme.animFast
                                            easing.type: Easing.OutCubic
                                        }
                                    }
                                }
                            }
                        }

                        // File list
                        Rectangle {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            radius: Theme.radiusMd
                            color: Theme.surfaceAlt
                            border.color: Theme.border
                            border.width: 1
                            clip: true

                            // Column header
                            Rectangle {
                                id: listHeader
                                anchors.top: parent.top
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.margins: 8
                                height: 30
                                radius: Theme.radiusSm
                                color: Theme.tabInactive

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: 10
                                    anchors.rightMargin: 10
                                    spacing: 8

                                    Text {
                                        Layout.fillWidth: true
                                        text: Theme.tr("pickerName")
                                        font: Theme.captionBoldFont
                                        color: Theme.textSecondary
                                        horizontalAlignment: Text.AlignLeft
                                    }
                                    Text {
                                        Layout.preferredWidth: 88
                                        Layout.rightMargin: 24
                                        text: Theme.tr("pickerSize")
                                        font: Theme.captionBoldFont
                                        color: Theme.textSecondary
                                        horizontalAlignment: Text.AlignRight
                                    }
                                    Text {
                                        Layout.preferredWidth: 132
                                        text: Theme.tr("pickerModified")
                                        font: Theme.captionBoldFont
                                        color: Theme.textSecondary
                                    }
                                }
                            }

                            Rectangle {
                                id: newFolderBar
                                anchors.top: listHeader.bottom
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.margins: 8
                                anchors.topMargin: 4
                                height: filePanel.creatingFolder ? 34 : 0
                                visible: filePanel.creatingFolder
                                radius: Theme.radiusSm
                                color: Theme.surface
                                border.color: Theme.accent
                                border.width: 1
                                clip: true

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: 10
                                    anchors.rightMargin: 10
                                    spacing: 8

                                    FileKindIcon {
                                        Layout.preferredWidth: 18
                                        Layout.preferredHeight: 18
                                        kind: "folder"
                                        tint: Theme.accent
                                    }

                                    TextField {
                                        id: newFolderField
                                        Layout.fillWidth: true
                                        font: Theme.mainFont
                                        color: Theme.text
                                        placeholderText: Theme.tr("pickerNewFolderName")
                                        placeholderTextColor: Theme.textSecondary
                                        focus: filePanel.creatingFolder
                                        selectByMouse: true
                                        background: Rectangle { color: "transparent" }

                                        onAccepted: filePanel.commitInlineNewFolder()

                                        Keys.onPressed: function(event) {
                                            if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                                                filePanel.commitInlineNewFolder()
                                                event.accepted = true
                                            } else if (event.key === Qt.Key_Escape) {
                                                filePanel.cancelInlineNewFolder()
                                                event.accepted = true
                                            }
                                        }
                                    }
                                }
                            }

                            ListView {
                                id: entryList
                                anchors.top: newFolderBar.bottom
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.bottom: parent.bottom
                                anchors.margins: 8
                                anchors.topMargin: filePanel.creatingFolder ? 4 : 4
                                clip: true
                                spacing: 2
                                model: FilePicker.entries
                                boundsBehavior: Flickable.StopAtBounds
                                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                                add: Transition {
                                    NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Theme.animFast; easing.type: Easing.OutCubic }
                                    NumberAnimation { property: "y"; from: 8; duration: Theme.animNormal; easing.type: Easing.OutCubic }
                                }
                                displaced: Transition {
                                    NumberAnimation { properties: "y"; duration: Theme.animNormal; easing.type: Easing.OutCubic }
                                }

                                function scrollToPath(path) {
                                    const idx = FilePicker.indexOfPath(path)
                                    if (idx < 0)
                                        return
                                    positionViewAtIndex(idx, ListView.Center)
                                }

                                delegate: Rectangle {
                                    required property string name
                                    required property string path
                                    required property bool isDir
                                    required property string sizeText
                                    required property string modifiedText
                                    required property string iconKind

                                    property string entryPath: path
                                    property bool entryIsDir: isDir
                                    property string entryName: name

                                    readonly property bool rowSelected: {
                                        if (isDir)
                                            return path === FilePicker.highlightedPath
                                        var paths = FilePicker.selectedPaths
                                        for (var i = 0; i < paths.length; ++i) {
                                            if (paths[i] === path)
                                                return true
                                        }
                                        return false
                                    }

                                    width: entryList.width
                                    height: 34
                                    radius: Theme.radiusSm
                                    color: entryMouse.containsMouse || rowSelected
                                           ? (rowSelected
                                              ? (Theme.dark ? "#505052" : "#D8D8DE")
                                              : Theme.menuHover)
                                           : "transparent"
                                    border.color: rowSelected
                                                  ? (Theme.dark ? "#8E8E93" : "#AEAEB2")
                                                  : "transparent"
                                    border.width: rowSelected ? 1 : 0
                                    scale: entryMouse.pressed && !rowSelected ? 0.985 : 1

                                    Behavior on color {
                                        ColorAnimation { duration: Theme.animFast; easing.type: Easing.OutCubic }
                                    }
                                    Behavior on border.color {
                                        ColorAnimation { duration: Theme.animFast; easing.type: Easing.OutCubic }
                                    }
                                    Behavior on scale {
                                        NumberAnimation { duration: Theme.animFast; easing.type: Easing.OutCubic }
                                    }

                                    RowLayout {
                                        anchors.fill: parent
                                        anchors.leftMargin: 10
                                        anchors.rightMargin: 10
                                        spacing: 8

                                        FileKindIcon {
                                            Layout.preferredWidth: 18
                                            Layout.preferredHeight: 18
                                            kind: iconKind
                                        }

                                        Item {
                                            id: nameCell
                                            Layout.fillWidth: true
                                            Layout.fillHeight: true

                                            readonly property bool renaming: filePanel.renamingPath === path

                                            Text {
                                                anchors.fill: parent
                                                verticalAlignment: Text.AlignVCenter
                                                visible: !nameCell.renaming
                                                text: name
                                                font: Theme.mainFont
                                                color: Theme.text
                                                elide: Text.ElideRight
                                            }

                                            TextField {
                                                id: renameField
                                                anchors.fill: parent
                                                anchors.topMargin: 3
                                                anchors.bottomMargin: 3
                                                visible: nameCell.renaming
                                                topPadding: 0
                                                bottomPadding: 0
                                                leftPadding: 6
                                                rightPadding: 6
                                                verticalAlignment: TextInput.AlignVCenter
                                                font: Theme.mainFont
                                                color: Theme.text
                                                selectByMouse: true
                                                background: Rectangle {
                                                    radius: Theme.radiusSm
                                                    color: Theme.surface
                                                    border.color: Theme.accent
                                                    border.width: 1
                                                }

                                                onVisibleChanged: {
                                                    if (visible) {
                                                        filePanel.renameFieldItem = renameField
                                                        text = name
                                                        Qt.callLater(function() {
                                                            renameField.forceActiveFocus()
                                                            renameField.selectAll()
                                                        })
                                                    } else if (filePanel.renameFieldItem === renameField) {
                                                        filePanel.renameFieldItem = null
                                                    }
                                                }

                                                Keys.onPressed: function(event) {
                                                    if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                                                        filePanel.commitRename()
                                                        event.accepted = true
                                                    } else if (event.key === Qt.Key_Escape) {
                                                        filePanel.cancelRename()
                                                        event.accepted = true
                                                    }
                                                }
                                            }
                                        }

                                        Text {
                                            Layout.preferredWidth: 88
                                            Layout.rightMargin: 24
                                            text: isDir ? "" : sizeText
                                            font: Theme.captionFont
                                            color: Theme.textBody
                                            elide: Text.ElideRight
                                            horizontalAlignment: Text.AlignRight
                                        }

                                        Text {
                                            Layout.preferredWidth: 132
                                            text: modifiedText
                                            font: Theme.captionFont
                                            color: Theme.textBody
                                            elide: Text.ElideRight
                                        }
                                    }

                                    MouseArea {
                                        id: entryMouse
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        enabled: filePanel.renamingPath !== path
                                        acceptedButtons: Qt.LeftButton
                                        cursorShape: isDir ? Qt.PointingHandCursor : Qt.ArrowCursor
                                        onClicked: function(mouse) {
                                            filePanel.blurSearch()
                                            FilePicker.selectEntry(path, isDir, mouse.modifiers)
                                        }
                                        onDoubleClicked: function(mouse) {
                                            if (isDir)
                                                FilePicker.navigateTo(path)
                                            else
                                                FilePicker.activateEntry(path, false)
                                        }
                                    }
                                }
                            }

                            MouseArea {
                                anchors.fill: entryList
                                acceptedButtons: Qt.RightButton
                                z: 5
                                onClicked: function(mouse) {
                                    filePanel.blurSearch()
                                    const cy = mouse.y + entryList.contentY
                                    const hit = entryList.itemAt(mouse.x, cy)
                                    const pos = mapToItem(root, mouse.x, mouse.y)
                                    if (!hit) {
                                        contextMenu.openForBlank(pos)
                                        return
                                    }
                                    const path = hit.entryPath
                                    const isDir = hit.entryIsDir
                                    const name = hit.entryName
                                    var targets = [path]
                                    if (!isDir) {
                                        const selected = FilePicker.selectedPaths
                                        if (selected.indexOf(path) >= 0 && selected.length > 1)
                                            targets = selected
                                        else
                                            FilePicker.selectEntry(path, isDir, 0)
                                    } else {
                                        FilePicker.selectEntry(path, isDir, 0)
                                    }
                                    contextMenu.openForEntry(path, isDir, name, targets, pos)
                                }
                            }

                            Text {
                                anchors.centerIn: parent
                                visible: !FilePicker.loading && FilePicker.entries.length === 0
                                text: FilePicker.recentFilterActive
                                      ? Theme.tr("pickerRecentEmpty")
                                      : Theme.tr("pickerEmpty")
                                color: Theme.textSecondary
                                font: Theme.mainFont
                            }

                            BusyIndicator {
                                anchors.centerIn: parent
                                running: FilePicker.loading
                                visible: FilePicker.loading
                            }
                        }

                        Text {
                            Layout.fillWidth: true
                            Layout.preferredHeight: visible ? implicitHeight : 0
                            visible: FilePicker.errorMessage.length > 0
                            text: FilePicker.errorMessage
                            wrapMode: Text.Wrap
                            color: Theme.danger
                            font: Theme.captionFont
                        }
                    }
                }

                // Bottom bar
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 1
                    color: Theme.border
                }

                RowLayout {
                    id: bottomBar
                    Layout.fillWidth: true
                    Layout.preferredHeight: 40
                    spacing: 12

                    readonly property bool splitMode: FilePicker.mode === "folder"
                                                      && FilePicker.exportKind === "split"

                    PickerFilterCombo {
                        id: filterCombo
                        Layout.preferredWidth: 156
                        Layout.preferredHeight: 38
                        visible: FilePicker.filters.length > 0
                        model: FilePicker.filters
                        onActivated: function(index) {
                            filePanel.blurSearch()
                            FilePicker.filterIndex = index
                        }
                    }

                    Item {
                        Layout.fillWidth: true
                        visible: !bottomBar.splitMode
                    }

                    Text {
                        visible: FilePicker.mode === "openMulti" && FilePicker.selectedPaths.length > 0
                        text: FilePicker.selectedPaths.length.toString()
                        font: Theme.captionBoldFont
                        color: Theme.accent
                    }

                    TextField {
                        id: splitBaseField
                        visible: bottomBar.splitMode
                        Layout.fillWidth: true
                        Layout.preferredHeight: 38
                        text: FilePicker.fileName
                        font: Theme.mainFont
                        color: Theme.text
                        placeholderText: Theme.tr("pickerFileName")
                        placeholderTextColor: Theme.textSecondary
                        selectByMouse: true
                        onTextChanged: FilePicker.fileName = text
                        Keys.onPressed: function(event) {
                            if (event.key === Qt.Key_Escape) {
                                focus = false
                                event.accepted = true
                            }
                        }
                        background: Item {
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
                                opacity: splitBaseField.activeFocus ? 1 : 0
                                Behavior on opacity {
                                    NumberAnimation {
                                        duration: Theme.animNormal
                                        easing.type: Easing.OutCubic
                                    }
                                }
                            }
                        }

                        // Live preview of the first exported file name.
                        Text {
                            anchors.top: parent.bottom
                            anchors.topMargin: 5
                            anchors.left: parent.left
                            anchors.leftMargin: 2
                            visible: bottomBar.splitMode && text.length > 0
                            text: {
                                const n = FilePicker.fileName
                                const s = FilePicker.splitSeparator
                                const st = FilePicker.splitNumberStyle
                                const l = AppSettings.languageRevision
                                return FilePicker.firstSplitOutputName()
                            }
                            font.pixelSize: 12
                            font.family: Theme.mainFont.family
                            color: Theme.text
                            elide: Text.ElideRight
                            width: parent.width
                        }
                    }

                    TextField {
                        id: splitSepField
                        visible: bottomBar.splitMode
                        Layout.preferredWidth: 38
                        Layout.maximumWidth: 38
                        Layout.preferredHeight: 38
                        text: FilePicker.splitSeparator
                        font: Theme.mainFont
                        color: Theme.text
                        placeholderText: "_"
                        placeholderTextColor: Theme.textSecondary
                        maximumLength: 4
                        leftPadding: 2
                        rightPadding: 2
                        horizontalAlignment: Text.AlignHCenter
                        selectByMouse: true
                        onTextChanged: FilePicker.splitSeparator = text
                        Keys.onPressed: function(event) {
                            if (event.key === Qt.Key_Escape) {
                                focus = false
                                event.accepted = true
                            }
                        }
                        background: Item {
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
                                opacity: splitSepField.activeFocus ? 1 : 0
                                Behavior on opacity {
                                    NumberAnimation {
                                        duration: Theme.animNormal
                                        easing.type: Easing.OutCubic
                                    }
                                }
                            }
                        }
                    }

                    PickerFilterCombo {
                        id: splitNumCombo
                        compact: true
                        visible: bottomBar.splitMode
                        Layout.preferredWidth: 38
                        Layout.preferredHeight: 38
                        readonly property var styleOptions: {
                            const _ = AppSettings.languageRevision
                            return [
                                { label: "1", value: 0 },
                                { label: Theme.tr("splitNumLower"), value: 1 },
                                { label: Theme.tr("splitNumUpper"), value: 2 }
                            ]
                        }
                        model: styleOptions
                        onActivated: function(index) {
                            filePanel.blurSearch()
                            FilePicker.splitNumberStyle = styleOptions[index].value
                        }
                    }

                    TextField {
                        id: fileNameField
                        visible: FilePicker.mode === "save"
                        Layout.fillWidth: true
                        Layout.minimumWidth: 240
                        Layout.preferredHeight: 38
                        text: FilePicker.fileName
                        font: Theme.mainFont
                        color: Theme.text
                        placeholderText: Theme.tr("pickerFileName")
                        placeholderTextColor: Theme.textSecondary
                        selectByMouse: true
                        onTextChanged: FilePicker.fileName = text
                        Keys.onPressed: function(event) {
                            if (event.key === Qt.Key_Escape) {
                                focus = false
                                event.accepted = true
                            }
                        }
                        background: Item {
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
                                opacity: fileNameField.activeFocus ? 1 : 0
                                Behavior on opacity {
                                    NumberAnimation {
                                        duration: Theme.animNormal
                                        easing.type: Easing.OutCubic
                                    }
                                }
                            }
                        }
                    }

                    StyledButton {
                        text: Theme.tr("pickerCancel")
                        onClicked: FilePicker.reject()
                    }

                    StyledButton {
                        text: FilePicker.acceptLabel
                        highlighted: true
                        enabled: FilePicker.acceptEnabled
                        onClicked: FilePicker.accept()
                    }
                }
            }

            // While naming or renaming, any click outside the input commits it
            MouseArea {
                anchors.fill: parent
                visible: (filePanel.creatingFolder || filePanel.renamingPath.length > 0)
                         && !folderExistsPopup.visible
                z: 40
                acceptedButtons: Qt.LeftButton | Qt.RightButton
                onPressed: function(mouse) {
                    if (filePanel.creatingFolder) {
                        const p = mapToItem(newFolderBar, mouse.x, mouse.y)
                        if (p.x >= 0 && p.y >= 0 && p.x <= newFolderBar.width && p.y <= newFolderBar.height) {
                            mouse.accepted = false
                            return
                        }
                        filePanel.commitInlineNewFolder()
                        return
                    }
                    const field = filePanel.renameFieldItem
                    if (field) {
                        const q = mapToItem(field, mouse.x, mouse.y)
                        if (q.x >= 0 && q.y >= 0 && q.x <= field.width && q.y <= field.height) {
                            mouse.accepted = false
                            return
                        }
                    }
                    filePanel.commitRename()
                }
            }
        }
    }

    component ContextMenuItem: Item {
        id: menuItem

        property string label
        property string iconName: ""
        property bool itemEnabled: true
        property color labelColor: Theme.text
        property color iconColor: Theme.textBody
        property bool danger: false
        signal triggered()

        width: parent ? parent.width : 0
        height: 34
        opacity: menuItem.itemEnabled ? 1 : 0.38

        Behavior on opacity {
            NumberAnimation { duration: Theme.animFast; easing.type: Easing.OutCubic }
        }

        Rectangle {
            id: menuItemBg
            anchors.fill: parent
            radius: 6
            color: {
                if (!menuItem.itemEnabled)
                    return "transparent"
                if (!menuItemMouse.containsMouse)
                    return "transparent"
                return menuItem.danger
                       ? (Theme.dark ? "#4A2020" : "#FDECEC")
                       : Theme.menuHover
            }

            Behavior on color {
                ColorAnimation { duration: Theme.animFast; easing.type: Easing.OutCubic }
            }
        }

        Rectangle {
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            width: 3
            height: 16
            radius: 1.5
            visible: menuItem.itemEnabled && menuItemMouse.containsMouse
            color: menuItem.danger ? Theme.danger : Theme.accent
            opacity: 0.9
        }

        LucideIcon {
            id: menuIcon
            anchors.left: parent.left
            anchors.leftMargin: 10
            anchors.verticalCenter: parent.verticalCenter
            anchors.verticalCenterOffset: 1
            width: 15
            height: 15
            icon: menuItem.iconName
            color: menuItem.danger && menuItemMouse.containsMouse
                   ? Theme.danger
                   : (menuItem.danger ? Theme.danger : menuItem.iconColor)
            visible: menuItem.iconName.length > 0
        }

        Text {
            anchors.fill: parent
            leftPadding: menuItem.iconName.length > 0 ? 32 : 12
            rightPadding: 10
            verticalAlignment: Text.AlignVCenter
            text: menuItem.label
            font: Theme.mainFont
            color: menuItem.danger && menuItemMouse.containsMouse
                   ? Theme.danger
                   : menuItem.labelColor
            elide: Text.ElideRight

            Behavior on color {
                ColorAnimation { duration: Theme.animFast; easing.type: Easing.OutCubic }
            }
        }

        MouseArea {
            id: menuItemMouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: menuItem.itemEnabled ? Qt.PointingHandCursor : Qt.ArrowCursor
            onClicked: {
                if (!menuItem.itemEnabled)
                    return
                menuItem.triggered()
                contextMenu.close()
            }
        }
    }

    Popup {
        id: contextMenu
        parent: root
        z: 7000
        padding: 6
        width: 148
        modal: false
        dim: false
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        transformOrigin: Item.TopLeft
        opacity: 1
        scale: 1

        enter: Transition {
            ParallelAnimation {
                NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Theme.animFast; easing.type: Easing.OutCubic }
                NumberAnimation { property: "scale"; from: 0.92; to: 1; duration: Theme.animNormal; easing.type: Easing.OutBack; easing.overshoot: 1.2 }
            }
        }
        exit: Transition {
            ParallelAnimation {
                NumberAnimation { property: "opacity"; to: 0; duration: Theme.animFast; easing.type: Easing.InCubic }
                NumberAnimation { property: "scale"; to: 0.96; duration: Theme.animFast; easing.type: Easing.InCubic }
            }
        }

        property var targetPaths: []
        property string targetPath: ""
        property string targetName: ""
        property bool targetIsDir: false
        readonly property bool hasTarget: targetPath.length > 0
        readonly property bool canPaste: FilePicker.clipboardHasFiles && !FilePicker.recentFilterActive

        function openForEntry(path, isDir, name, targets, pos) {
            targetPath = path
            targetIsDir = isDir
            targetName = name
            targetPaths = targets
            x = pos.x
            y = pos.y
            open()
        }

        function openForBlank(pos) {
            targetPath = ""
            targetName = ""
            targetIsDir = false
            targetPaths = []
            x = pos.x
            y = pos.y
            open()
        }

        onOpened: {
            if (x + width > root.width - 8)
                x = root.width - width - 8
            if (y + height > root.height - 8)
                y = root.height - height - 8
            if (x < 8)
                x = 8
            if (y < 8)
                y = 8
        }

        background: Item {
            Rectangle {
                anchors.fill: parent
                anchors.topMargin: 4
                radius: Theme.radiusMd
                color: Theme.shadowColor
                opacity: Theme.shadowOpacity1
            }
            Rectangle {
                anchors.fill: parent
                radius: Theme.radiusMd
                color: Theme.surface
                border.color: Theme.border
                border.width: 1
            }
        }

        contentItem: Column {
            spacing: 2

            ContextMenuItem {
                label: Theme.tr("pickerCopy")
                iconName: "copy"
                itemEnabled: contextMenu.hasTarget
                onTriggered: FilePicker.copyPaths(contextMenu.targetPaths)
            }

            ContextMenuItem {
                label: Theme.tr("pickerCut")
                iconName: "scissors"
                itemEnabled: contextMenu.hasTarget
                onTriggered: FilePicker.cutPaths(contextMenu.targetPaths)
            }

            ContextMenuItem {
                label: Theme.tr("pickerPaste")
                iconName: "clipboard-paste"
                itemEnabled: contextMenu.canPaste
                onTriggered: {
                    const pasted = FilePicker.pasteIntoCurrent()
                    if (pasted && pasted.length > 0) {
                        Qt.callLater(function() {
                            entryList.scrollToPath(pasted[0])
                        })
                    }
                }
            }

            ContextMenuItem {
                label: Theme.tr("pickerRename")
                iconName: "pencil"
                itemEnabled: contextMenu.hasTarget && contextMenu.targetPaths.length === 1
                onTriggered: filePanel.startRename(contextMenu.targetPath, contextMenu.targetName)
            }

            Rectangle {
                width: parent.width
                height: 9
                color: "transparent"
                visible: contextMenu.hasTarget
                Rectangle {
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.leftMargin: 4
                    anchors.rightMargin: 4
                    height: 1
                    color: Theme.border
                    opacity: 0.85
                }
            }

            ContextMenuItem {
                label: Theme.tr("pickerDelete")
                iconName: "trash-2"
                itemEnabled: contextMenu.hasTarget
                labelColor: Theme.danger
                iconColor: Theme.danger
                danger: true
                onTriggered: deleteConfirmPopup.openFor(contextMenu.targetPaths, contextMenu.targetName)
            }
        }
    }

    Popup {
        id: deleteConfirmPopup
        parent: dialogLayer
        z: 7000
        modal: true
        dim: true
        padding: 20
        width: 380
        x: (parent.width - width) / 2
        y: (parent.height - height) / 2
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        transformOrigin: Item.Center
        opacity: 1
        scale: 1

        property var pendingPaths: []
        property string pendingName: ""

        readonly property string confirmMessage: {
            const n = pendingPaths ? pendingPaths.length : 0
            if (n <= 1)
                return Theme.tr("pickerDeleteMessageOne").replace("%1", pendingName)
            return Theme.tr("pickerDeleteMessageMany").replace("%1", String(n))
        }

        function openFor(paths, name) {
            pendingPaths = paths
            pendingName = name && name.length > 0 ? name : Theme.tr("pickerDelete")
            open()
        }

        enter: Transition {
            ParallelAnimation {
                NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Theme.animNormal; easing.type: Easing.OutCubic }
                NumberAnimation { property: "scale"; from: 0.94; to: 1; duration: Theme.animSlow; easing.type: Easing.OutBack; easing.overshoot: 1.08 }
            }
        }
        exit: Transition {
            ParallelAnimation {
                NumberAnimation { property: "opacity"; to: 0; duration: Theme.animFast; easing.type: Easing.InCubic }
                NumberAnimation { property: "scale"; to: 0.96; duration: Theme.animFast; easing.type: Easing.InCubic }
            }
        }

        background: Item {
            Rectangle {
                anchors.fill: parent
                anchors.topMargin: 6
                radius: Theme.radiusMd
                color: Theme.shadowColor
                opacity: Theme.shadowOpacity1
            }
            Rectangle {
                anchors.fill: parent
                radius: Theme.radiusMd
                color: Theme.surface
                border.color: Theme.border
                border.width: 1
            }
        }

        contentItem: ColumnLayout {
            spacing: 14

            Text {
                Layout.fillWidth: true
                text: Theme.tr("pickerDeleteTitle")
                font: Theme.mainFontBold
                color: Theme.text
            }

            Text {
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                text: deleteConfirmPopup.confirmMessage
                font: Theme.mainFont
                color: Theme.textBody
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                Item { Layout.fillWidth: true }
                StyledButton {
                    text: Theme.tr("pickerCancel")
                    onClicked: deleteConfirmPopup.close()
                }
                Button {
                    id: deleteConfirmBtn
                    text: Theme.tr("pickerDelete")
                    implicitHeight: 36
                    leftPadding: 16
                    rightPadding: 16
                    onClicked: {
                        FilePicker.deletePaths(deleteConfirmPopup.pendingPaths)
                        deleteConfirmPopup.close()
                    }
                    background: Rectangle {
                        radius: Theme.radiusSm
                        color: deleteConfirmBtn.down ? "#D70015"
                               : (deleteConfirmBtn.hovered ? "#FF453A" : Theme.danger)
                        Behavior on color {
                            ColorAnimation { duration: Theme.animFast; easing.type: Easing.OutCubic }
                        }
                    }
                    contentItem: Text {
                        text: deleteConfirmBtn.text
                        font: Theme.mainFontBold
                        color: "#FFFFFF"
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                }
            }
        }
    }

    Popup {
        id: folderExistsPopup
        parent: dialogLayer
        z: 7000
        modal: true
        dim: true
        padding: 20
        width: 360
        x: (parent.width - width) / 2
        y: (parent.height - height) / 2
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        transformOrigin: Item.Center
        opacity: 1
        scale: 1

        property string messageKey: "pickerFolderExistsMessage"

        enter: Transition {
            ParallelAnimation {
                NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Theme.animNormal; easing.type: Easing.OutCubic }
                NumberAnimation { property: "scale"; from: 0.94; to: 1; duration: Theme.animSlow; easing.type: Easing.OutBack; easing.overshoot: 1.08 }
            }
        }
        exit: Transition {
            ParallelAnimation {
                NumberAnimation { property: "opacity"; to: 0; duration: Theme.animFast; easing.type: Easing.InCubic }
                NumberAnimation { property: "scale"; to: 0.96; duration: Theme.animFast; easing.type: Easing.InCubic }
            }
        }

        onClosed: {
            FilePicker.clearNameConflict()
            if (filePanel.creatingFolder) {
                Qt.callLater(function() {
                    newFolderField.forceActiveFocus()
                    newFolderField.selectAll()
                })
            } else if (filePanel.renamingPath.length > 0 && filePanel.renameFieldItem) {
                Qt.callLater(function() {
                    if (filePanel.renameFieldItem) {
                        filePanel.renameFieldItem.forceActiveFocus()
                        filePanel.renameFieldItem.selectAll()
                    }
                })
            }
        }

        background: Rectangle {
            radius: Theme.radiusMd
            color: Theme.surface
            border.color: Theme.border
            border.width: 1
        }

        contentItem: ColumnLayout {
            spacing: 14

            Text {
                Layout.fillWidth: true
                text: Theme.tr("pickerFolderExistsTitle")
                font: Theme.mainFontBold
                color: Theme.text
            }

            Text {
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                text: Theme.tr(folderExistsPopup.messageKey)
                font: Theme.mainFont
                color: Theme.textBody
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                Item { Layout.fillWidth: true }
                StyledButton {
                    text: Theme.tr("pickerOk")
                    highlighted: true
                    onClicked: folderExistsPopup.close()
                }
            }
        }
    }

    Connections {
        target: FilePicker
        function onShownChanged() {
            if (FilePicker.shown) {
                fileNameField.text = FilePicker.fileName
                splitBaseField.text = FilePicker.fileName
                splitSepField.text = FilePicker.splitSeparator
                splitNumCombo.currentIndex = FilePicker.splitNumberStyle
                hiddenSwitch.checked = FilePicker.showHidden
                searchField.text = FilePicker.searchQuery
                filterCombo.currentIndex = FilePicker.filterIndex
                pathBar.pathField.text = FilePicker.pathInput
                pathBar.pathDirection = 0
                pathBar._prevCrumbCount = FilePicker.breadcrumb.length
                pathBar.syncBreadcrumbs()
            }
        }
        function onCurrentPathChanged() {
            filePanel.blurSearch()
            filePanel.creatingFolder = false
            filePanel.renamingPath = ""
            contextMenu.close()
            if (searchField.text !== FilePicker.searchQuery)
                searchField.text = FilePicker.searchQuery
            const crumbs = FilePicker.breadcrumb.length
            if (crumbs > pathBar._prevCrumbCount)
                pathBar.pathDirection = 1
            else if (crumbs < pathBar._prevCrumbCount)
                pathBar.pathDirection = -1
            else
                pathBar.pathDirection = 0
            pathBar.syncBreadcrumbs()
            pathBar._prevCrumbCount = crumbs
            if (!pathBar.pathField.activeFocus)
                pathBar.pathField.text = FilePicker.pathInput
        }
        function onEntriesChanged() {
            if (FilePicker.highlightedPath.length > 0) {
                Qt.callLater(function() {
                    entryList.scrollToPath(FilePicker.highlightedPath)
                })
            }
        }
        function onSearchQueryChanged() {
            if (searchField.text !== FilePicker.searchQuery)
                searchField.text = FilePicker.searchQuery
        }
        function onFileNameChanged() {
            if (fileNameField.text !== FilePicker.fileName)
                fileNameField.text = FilePicker.fileName
            if (splitBaseField.text !== FilePicker.fileName)
                splitBaseField.text = FilePicker.fileName
        }
        function onSplitOptionsChanged() {
            splitSepField.text = FilePicker.splitSeparator
            splitNumCombo.currentIndex = FilePicker.splitNumberStyle
        }
        function onNameConflictPromptChanged() {
            if (!FilePicker.nameConflictPrompt)
                return
            folderExistsPopup.messageKey = FilePicker.mode === "save"
                    || FilePicker.exportKind === "split"
                ? "pickerFileExistsMessage"
                : "pickerFolderExistsMessage"
            folderExistsPopup.open()
        }
        function onFilterIndexChanged() {
            if (filterCombo.currentIndex !== FilePicker.filterIndex)
                filterCombo.currentIndex = FilePicker.filterIndex
        }
    }

    Keys.onEscapePressed: {
        if (filePanel.creatingFolder) {
            filePanel.cancelInlineNewFolder()
            return
        }
        FilePicker.reject()
    }
}
