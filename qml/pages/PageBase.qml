import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ProjectO

Item {
    id: pageBase
    clip: true

    property string desc: ""
    property bool showRotate: false
    property bool showConvertFormat: false
    property bool showCompress: false
    property bool showWatermark: false
    property bool showPageRange: false
    property bool enableReorder: false

    ShadowCard {
        anchors.fill: parent
        radius: Theme.radiusLg
        margins: 0

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 24
            spacing: 0

            RowLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 24

                ColumnLayout {
                    Layout.preferredWidth: 280
                    Layout.maximumWidth: 300
                    Layout.fillHeight: true
                    spacing: 12

                    Text {
                        text: Theme.tr("fileList")
                        font: Theme.tabFont
                        color: Theme.text
                    }

                    PdfFileDropZone {
                        Layout.fillWidth: true
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        radius: Theme.radiusMd
                        color: Theme.surfaceAlt
                        border.color: Theme.border
                        border.width: 1
                        clip: true

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 12
                            spacing: 8

                            RowLayout {
                                Layout.fillWidth: true

                                Text {
                                    text: Theme.tr("addedFiles")
                                    font: Theme.captionFont
                                    color: Theme.textBody
                                }

                                Item { Layout.fillWidth: true }

                                Text {
                                    visible: PdfController.fileCount > 0
                                    text: PdfController.fileCount.toString()
                                    color: Theme.accent
                                    font: Theme.captionBoldFont
                                }
                            }

                            PdfFileListView {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                enableReorder: pageBase.enableReorder
                            }
                        }
                    }
                }

                Rectangle {
                    Layout.preferredWidth: 1
                    Layout.fillHeight: true
                    color: Theme.border
                }

                ScrollPreview {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    showWatermarkOverlay: pageBase.showWatermark
                    watermarkText: pageActionBar.watermarkText
                    watermarkCount: pageActionBar.watermarkCount
                    watermarkColor: pageActionBar.watermarkColor
                    previewRotation: pageBase.showRotate ? pageActionBar.optionValue : 0
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 1
                Layout.topMargin: 20
                Layout.bottomMargin: 16
                color: Theme.border
            }

            ActionBar {
                id: pageActionBar
                Layout.fillWidth: true
                Layout.preferredHeight: 40
                desc: pageBase.desc
                showRotate: pageBase.showRotate
                showConvertFormat: pageBase.showConvertFormat
                showCompress: pageBase.showCompress
                showWatermark: pageBase.showWatermark
                showPageRange: pageBase.showPageRange
            }
        }
    }
}
