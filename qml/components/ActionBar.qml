import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ProjectO

RowLayout {
    id: actionBar
    spacing: 12

    readonly property int controlWidth: Theme.compactControlWidth

    property string desc: ""
    property bool showRotate: false
    property bool showConvertFormat: false
    property bool showCompress: false
    property bool showWatermark: false
    property bool showPageRange: false
    property int optionValue: 90
    property int watermarkCount: 1
    property string watermarkText: ""
    property string watermarkColor: Theme.watermarkDefaultColor

    function commitPageRange() {
        if (actionBar.showPageRange && pageRangeField.boundPath.length > 0)
            PdfController.setPageRange(pageRangeField.boundPath, pageRangeField.text)
    }

    Text {
        Layout.fillWidth: true
        Layout.minimumWidth: 120
        Layout.maximumHeight: 40
        text: actionBar.desc
        elide: Text.ElideRight
        maximumLineCount: 1
        verticalAlignment: Text.AlignVCenter
        color: Theme.textBody
        font: Theme.mainFont
    }

    RowLayout {
        spacing: 10
        Layout.alignment: Qt.AlignRight | Qt.AlignVCenter

        WatermarkColorPicker {
            id: colorPicker
            visible: actionBar.showWatermark
            Layout.preferredWidth: 38
            Layout.preferredHeight: 38
            onColorValueChanged: actionBar.watermarkColor = colorPicker.colorValue
        }

        WatermarkCountCombo {
            id: countPicker
            visible: actionBar.showWatermark
            compact: true
            Layout.preferredWidth: 38
            Layout.preferredHeight: 38
            onCountValueChanged: actionBar.watermarkCount = countPicker.countValue
        }

        WatermarkInputField {
            id: watermarkInput
            visible: actionBar.showWatermark
            Layout.preferredWidth: 168
            Layout.preferredHeight: 38
            placeholderText: Theme.tr("watermarkPlaceholder")
            text: actionBar.watermarkText
            onTextEdited: function(value) { actionBar.watermarkText = value }
        }

        CompressComboBox {
            id: compressPicker
            visible: actionBar.showCompress
            Layout.preferredWidth: actionBar.controlWidth
            onLevelValueChanged: actionBar.optionValue = compressPicker.levelValue
        }

        ConvertComboBox {
            id: formatPicker
            visible: actionBar.showConvertFormat
            Layout.preferredWidth: actionBar.controlWidth
            onFormatValueChanged: actionBar.optionValue = formatPicker.formatValue
        }

        TextField {
            id: pageRangeField
            visible: actionBar.showPageRange
            enabled: PdfController.fileCount > 0 && !PdfController.busy
            Layout.preferredWidth: 168
            Layout.preferredHeight: 38
            placeholderText: "2,3,5 ; 1-5 ; 1~5 ;"
            font: Theme.mainFont
            color: Theme.text
            placeholderTextColor: Theme.textSecondary
            maximumLength: 64
            selectByMouse: true

            // The field always edits the page range of the file currently
            // shown in the preview.
            readonly property string boundPath: PdfController.preview.currentFile
            onBoundPathChanged: text = PdfController.pageRange(boundPath)
            Component.onCompleted: text = PdfController.pageRange(boundPath)
            onTextEdited: PdfController.setPageRange(boundPath, text)
            onEditingFinished: PdfController.setPageRange(boundPath, text)

            Keys.onPressed: function(event) {
                if (event.key === Qt.Key_Escape) {
                    focus = false
                    event.accepted = true
                }
            }

            // Same styling as the file picker search box: static base border
            // with an accent overlay that fades in/out on focus.
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
                    opacity: pageRangeField.activeFocus ? 1 : 0

                    Behavior on opacity {
                        NumberAnimation {
                            duration: Theme.animNormal
                            easing.type: Easing.OutCubic
                        }
                    }
                }
            }
        }

        RotateComboBox {
            id: anglePicker
            visible: actionBar.showRotate
            Layout.preferredWidth: actionBar.controlWidth
            onRotateValueChanged: actionBar.optionValue = anglePicker.rotateValue
        }

        StyledButton {
            Layout.preferredWidth: actionBar.controlWidth
            text: Theme.tr("clear")
            enabled: PdfController.fileCount > 0 && !PdfController.busy
            onClicked: PdfController.clearFiles()
        }

        StyledButton {
            Layout.preferredWidth: actionBar.controlWidth
            text: Theme.tr("run")
            highlighted: true
            enabled: PdfController.fileCount > 0 && !PdfController.busy
                     && (!actionBar.showWatermark || actionBar.watermarkText.trim().length > 0)
            onClicked: {
                actionBar.commitPageRange()
                const rangeText = pageRangeField.visible ? pageRangeField.text.trim() : ""
                if (actionBar.showWatermark)
                    PdfController.runCurrentAction(actionBar.watermarkCount, actionBar.watermarkText,
                                                  actionBar.watermarkColor, rangeText)
                else
                    PdfController.runCurrentAction(actionBar.optionValue, "", "", rangeText)
            }
        }
    }
}
