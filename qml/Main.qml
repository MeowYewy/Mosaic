import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ProjectO

ApplicationWindow {
    id: root
    width: 1180
    height: 760
    minimumWidth: 960
    minimumHeight: 640
    visible: true
    title: Theme.headerName
    color: Theme.bg

    property int activeTab: 0

    Behavior on color { ColorAnimation { duration: Theme.animNormal } }

    palette: Palette {
        window: Theme.bg
        windowText: Theme.text
        base: Theme.surface
        text: Theme.text
        button: Theme.surfaceAlt
        buttonText: Theme.text
        highlight: Theme.accent
        highlightedText: "#FFFFFF"
        alternateBase: Theme.surfaceAlt
    }

    function syncFilesToMask() {
        const paths = PdfController.maskedPreview
                      ? PdfController.sourceFilePaths
                      : PdfController.filePaths
        AppController.replaceFiles(paths)
    }

    function syncFilesToPdf() {
        PdfController.replaceFiles(AppController.filePaths)
    }

    function finishMaskExit(anchorItem) {
        if (AppSettings.modeTransition) {
            themeTransition.startExit(anchorItem, null)
        }
        AppSettings.setMaskMode(false)
    }

    function enterMaskMode() {
        if (AppSettings.maskMode || themeTransition.active)
            return
        if (PdfController.maskedPreview)
            PdfController.clearMaskedPreview()
        syncFilesToMask()
        if (AppSettings.modeTransition) {
            featureTabsRow.maskPinned = true
            themeTransition.startEnter(featureTabsRow.maskButton, function() {
                AppSettings.setMaskMode(true)
            })
        } else {
            AppSettings.setMaskMode(true)
        }
    }

    function exitMaskMode(anchorItem) {
        if (!AppSettings.maskMode || themeTransition.active)
            return
        const hasMarks = AppController.redactions.count > 0
        finishMaskExit(anchorItem)
        if (hasMarks) {
            syncFilesToPdf()
            Qt.callLater(AppController.prepareMaskedPdfForPdfTools)
        } else {
            syncFilesToPdf()
        }
    }

    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        onPressed: function(mouse) {
            root.contentItem.forceActiveFocus()
            mouse.accepted = false
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        AppHeader {
            Layout.fillWidth: true
            Layout.preferredHeight: Theme.headerHeight
            onMenuRequested: function(anchor) {
                if (settingsDropdown.opacity > 0.5)
                    settingsDropdown.close()
                else
                    settingsDropdown.openAt(anchor)
            }
        }

        GlobalProgressBar {
            Layout.fillWidth: true
        }

        Item {
            id: loadBarMask
            Layout.fillWidth: true
            Layout.preferredHeight: (AppSettings.maskMode
                                     && (AppController.backgroundLoading
                                         || AppController.activeTask === "export"
                                         || (AppController.processing
                                             && AppController.activeTask.length === 0))) ? 3 : 0
            visible: Layout.preferredHeight > 0
            clip: true
            Rectangle { anchors.fill: parent; color: Theme.dark ? "#1A3D38" : "#D1EAE5" }
            Rectangle {
                height: parent.height
                width: parent.width * Math.max(0.04, AppController.progress)
                color: Theme.accent
            }
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 32
                spacing: 20
                visible: !AppSettings.maskMode

                FeatureTabsRow {
                    id: featureTabsRow
                    Layout.alignment: Qt.AlignHCenter
                    currentIndex: root.activeTab
                    onTabChanged: function(index) {
                        root.activeTab = index
                        PdfController.currentTab = index
                    }
                    onMaskRequested: root.enterMaskMode()
                }

                StackLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    currentIndex: root.activeTab

                    SplitPage { Layout.fillWidth: true; Layout.fillHeight: true }
                    MergePage { Layout.fillWidth: true; Layout.fillHeight: true }
                    RotatePage { Layout.fillWidth: true; Layout.fillHeight: true }
                    ConvertPage { Layout.fillWidth: true; Layout.fillHeight: true }
                    CompressPage { Layout.fillWidth: true; Layout.fillHeight: true }
                    WatermarkPage { Layout.fillWidth: true; Layout.fillHeight: true }
                }
            }

            MaskView {
                anchors.fill: parent
                visible: AppSettings.maskMode
                onExitRequested: function(anchorItem) { root.exitMaskMode(anchorItem) }
            }

            MaskShortcutHandler {
                active: AppSettings.maskMode
            }
        }
    }

    ThemeTransitionOverlay {
        id: themeTransition
        parent: root.contentItem
        anchors.fill: parent
        onActiveChanged: if (!active) featureTabsRow.maskPinned = false
    }

    StatusToast {
        id: statusToast
        parent: root.contentItem
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.topMargin: Theme.headerHeight + 12
        anchors.rightMargin: 32
        z: 2500
    }

    Connections {
        target: PdfController
        function onActionFinished(ok, message) {
            if (!AppSettings.maskMode)
                statusToast.show(message, ok)
        }
    }

    Connections {
        target: AppController
        function onActionFinished(ok, message) {
            if (AppSettings.maskMode)
                statusToast.show(message, ok)
        }
        function onMaskedPdfPathsReady(ok, paths, readOnlyPreview) {
            if (ok && paths.length > 0) {
                if (readOnlyPreview)
                    PdfController.applyMaskedPreview(paths, AppController.filePaths)
                else
                    syncFilesToPdf()
            } else {
                syncFilesToPdf()
            }
        }
    }

    MouseArea {
        anchors.fill: parent
        visible: settingsDropdown.opacity > 0.5
        z: 1999
        onClicked: settingsDropdown.close()
    }

    SettingsDropdown {
        id: settingsDropdown
        parent: root.contentItem
        onAboutRequested: aboutDialog.open()
        onSettingsRequested: settingsDialog.open()
        onInstallRequested: installConfirmPopup.open()
    }

    Popup {
        id: installConfirmPopup
        parent: Overlay.overlay
        modal: true
        dim: true
        padding: 20
        width: 360
        anchors.centerIn: parent
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        background: Rectangle {
            radius: Theme.radiusMd
            color: Theme.surface
            border.color: Theme.border
        }
        contentItem: ColumnLayout {
            spacing: 14
            Text {
                Layout.fillWidth: true
                text: Theme.tr("installConfirmTitle")
                font: Theme.mainFontBold
                color: Theme.text
            }
            Text {
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                text: Theme.tr("installConfirmMessage")
                font: Theme.mainFont
                color: Theme.textSecondary
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                Item { Layout.fillWidth: true }
                StyledButton {
                    text: Theme.tr("pickerCancel")
                    onClicked: installConfirmPopup.close()
                }
                StyledButton {
                    text: Theme.tr("installUpdate")
                    highlighted: true
                    onClicked: {
                        installConfirmPopup.close()
                        UpdateChecker.installUpdate()
                    }
                }
            }
        }
    }

    AboutDialog {
        id: aboutDialog
        parent: Overlay.overlay
        anchors.fill: parent
        onChangelogRequested: {
            aboutDialog.close()
            changelogDialog.open()
        }
    }

    SettingsDialog {
        id: settingsDialog
        parent: Overlay.overlay
        anchors.fill: parent
    }

    ChangelogDialog {
        id: changelogDialog
        parent: Overlay.overlay
        anchors.fill: parent
    }

    FilePickerDialog {
        parent: Overlay.overlay
        anchors.fill: parent
    }

    Component.onCompleted: {
        Qt.callLater(function() { UpdateChecker.checkForUpdates() })
    }
}
