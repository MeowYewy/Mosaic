import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ProjectO

Item {
    id: maskRoot

    signal exitRequested(var anchorItem)

    readonly property bool shortcutsEnabled: true

    readonly property var editShortcutRows: {
        const _ = AppSettings.languageRevision
        return [
            { labelKey: "shortcutPrevPage", keys: "Q / ←" },
            { labelKey: "shortcutNextPage", keys: "E / →" },
            { labelKey: "shortcutFitView", keys: "R" },
            { labelKey: "shortcutZoom", keys: Theme.tr("shortcutZoomKeys") },
            { labelKey: "shortcutPan", keys: Theme.tr("shortcutPanKeys") },
            { labelKey: "shortcutDraw", keys: "1" },
            { labelKey: "shortcutFixedDraw", keys: "2" },
            { labelKey: "shortcutSelect", keys: "3" },
            { labelKey: "shortcutDeleteMark", keys: "4" },
            { labelKey: "shortcutMaskedPreview", keys: "Enter" },
            { labelKey: "shortcutToggleStyle", keys: "Tab" }
        ]
    }

    readonly property var maskedShortcutRows: {
        const _ = AppSettings.languageRevision
        return [
            { labelKey: "shortcutToggleStyle", keys: "Tab" },
            { labelKey: "shortcutFitView", keys: "R" },
            { labelKey: "shortcutBackEdit", keys: "Enter" },
            { labelKey: "shortcutDeleteSelection", keys: "Delete" }
        ]
    }

    Item {
        anchors.fill: parent
        anchors.margins: 12

        RowLayout {
            anchors.fill: parent
            spacing: 12

            ShadowCard {
                Layout.preferredWidth: 300
                Layout.maximumWidth: 320
                Layout.fillHeight: true
                radius: Theme.radiusLg
                margins: 0

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 8

                    RowLayout {
                        Layout.fillWidth: true
                        StyledButton {
                            id: backBtn
                            text: Theme.tr("back")
                            onClicked: maskRoot.exitRequested(backBtn)
                        }
                        Item { Layout.fillWidth: true }
                        Text {
                            visible: AppController.fileCount > 0
                            text: AppController.fileCount + " " + Theme.tr("filesAdded")
                            color: Theme.accent
                            font: Theme.captionBoldFont
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        Text {
                            text: Theme.tr("fileList")
                            font: Theme.mainFontBold
                            color: Theme.text
                        }
                        Item { Layout.fillWidth: true }
                    }

                    FileDropZone { Layout.fillWidth: true }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        StyledButton {
                            text: Theme.tr("sortByType")
                            enabled: AppController.fileCount > 1
                            onClicked: AppController.sortFilesByType()
                        }

                        Item {
                            Layout.preferredWidth: contentSortBtn.implicitWidth
                            Layout.preferredHeight: contentSortBtn.implicitHeight

                            StyledButton {
                                id: contentSortBtn
                                anchors.fill: parent
                                text: Theme.tr("sortByContent")
                                enabled: AppController.fileCount > 1
                                         && AppController.contentSortReady
                                         && !AppController.contentSortRunning
                                onClicked: AppController.sortFilesByContent()
                            }

                            Item {
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.bottom: parent.bottom
                                anchors.margins: 5
                                height: 2
                                visible: AppController.fileCount > 1 && !AppController.contentSortReady
                                clip: true

                                Rectangle {
                                    anchors.fill: parent
                                    radius: 1
                                    color: Theme.dark ? "#1A3D38" : "#D1EAE5"
                                }
                                Rectangle {
                                    height: parent.height
                                    width: parent.width * Math.max(0.04, AppController.contentSortProgress)
                                    radius: 1
                                    color: Theme.accent
                                }
                            }
                        }

                        Item { Layout.fillWidth: true }
                        StyledButton {
                            text: Theme.tr("clear")
                            enabled: AppController.fileCount > 0
                            onClicked: AppController.clearAll()
                        }
                    }

                    FileListView {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                    }

                    StyledButton {
                        Layout.fillWidth: true
                        text: Theme.tr("export")
                        enabled: AppController.hasPreview && !AppController.processing
                                 && AppController.activeTask !== "export"
                                 && !AppController.backgroundLoading
                        onClicked: AppController.exportRedacted()
                    }
                }
            }

            ShadowCard {
                Layout.fillWidth: true
                Layout.fillHeight: true
                radius: Theme.radiusLg
                margins: 0

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 8

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 6

                        Text {
                            text: Theme.tr("maskPreview")
                            font: Theme.mainFontBold
                            color: Theme.text
                        }

                        ShortcutInfoTip {
                            popupParent: maskRoot
                            rows: AppController.showMaskedPreview
                                  ? maskRoot.maskedShortcutRows
                                  : maskRoot.editShortcutRows
                        }

                        Item { Layout.fillWidth: true }
                        ToolBar {}
                    }

                    PreviewCanvas {
                        id: previewCanvas
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        visible: AppController.pageCount > 1
                        StyledButton {
                            text: "‹"
                            enabled: AppController.currentPage > 0
                            onClicked: AppController.currentPage = AppController.currentPage - 1
                        }
                        Text {
                            Layout.fillWidth: true
                            horizontalAlignment: Text.AlignHCenter
                            text: (AppController.currentPage + 1) + " / " + AppController.pageCount
                            color: Theme.textBody
                            font: Theme.captionFont
                        }
                        StyledButton {
                            text: "›"
                            enabled: AppController.currentPage < AppController.pageCount - 1
                            onClicked: AppController.currentPage = AppController.currentPage + 1
                        }
                    }
                }
            }
        }
    }
}
