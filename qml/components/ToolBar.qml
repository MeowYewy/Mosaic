import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ProjectO

RowLayout {
    id: bar
    spacing: 6

    readonly property bool masked: AppController.showMaskedPreview
    property bool deletePulse: false

    Timer {
        id: deleteFlashTimer
        interval: 200
        onTriggered: bar.deletePulse = false
    }

    Connections {
        target: AppController
        function onDeleteMarkPulse() {
            bar.deletePulse = true
            deleteFlashTimer.restart()
        }
    }

    component GroupBox_: Rectangle {
        implicitHeight: 40
        implicitWidth: row.implicitWidth + 4
        radius: Theme.radiusSm
        color: Theme.surfaceAlt
        border.color: Theme.border
        border.width: 1
        default property alias content: row.data

        Row {
            id: row
            anchors.centerIn: parent
            spacing: 1
        }
    }

    component Divider: Rectangle {
        Layout.preferredWidth: 1
        Layout.preferredHeight: 22
        Layout.alignment: Qt.AlignVCenter
        color: Theme.border
    }

    Text {
        visible: AppController.autoMarkCount > 0
        Layout.alignment: Qt.AlignVCenter
        text: Theme.tr("autoMarks") + ": " + AppController.autoMarkCount
        color: Theme.maskAuto
        font: Theme.captionBoldFont
    }

    GroupBox_ {
        ToolStripButton {
            iconName: "expand"
            label: Theme.tr("toolFitView")
            enabled: AppController.hasPreview
            onClicked: AppController.resetPreviewView()
        }
    }

    Divider {}

    GroupBox_ {
        enabled: !bar.masked
        opacity: enabled ? 1 : 0.45

        ToolStripButton {
            iconName: "draw"
            label: Theme.tr("toolDraw")
            active: AppController.toolMode === "draw"
            enabled: parent.enabled
            onClicked: AppController.toolMode = "draw"
        }
        ToolStripButton {
            iconName: "select"
            label: Theme.tr("toolSelect")
            active: AppController.toolMode === "select"
            enabled: parent.enabled
            onClicked: AppController.toolMode = "select"
        }
        ToolStripButton {
            iconName: "delete"
            label: Theme.tr("deleteMark")
            active: bar.deletePulse
            enabled: parent.enabled && AppController.redactions.selectedId >= 0
            onClicked: AppController.deleteSelectedMark()
        }
    }

    Divider {}

    GroupBox_ {
        ToolStripButton {
            iconName: "style-block"
            label: Theme.tr("styleBlock")
            active: AppController.mosaicStyle === 0
            onClicked: AppController.mosaicStyle = 0
        }
        ToolStripButton {
            iconName: "style-pixel"
            label: Theme.tr("stylePixel")
            active: AppController.mosaicStyle === 1
            onClicked: AppController.mosaicStyle = 1
        }
    }

    Divider {}

    ToolStripButton {
        iconName: "mask-eye"
        label: Theme.tr("showMaskPreview")
        active: AppController.showMaskedPreview
        enabled: AppController.hasPreview
        onClicked: AppController.showMaskedPreview = !AppController.showMaskedPreview
    }
}
