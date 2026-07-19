import QtQuick
import ProjectO

Item {
    id: bar
    height: visible ? 3 : 0
    visible: !AppSettings.maskMode
             && PdfController.fileCount > 0
             && (PdfController.processing || PdfController.previewLoading
                 || PdfController.progress > 0.001)
    opacity: visible ? 1 : 0

    Behavior on height { NumberAnimation { duration: Theme.animFast } }
    Behavior on opacity { NumberAnimation { duration: Theme.animFast } }

    Rectangle {
        anchors.fill: parent
        color: Theme.border
    }

    Rectangle {
        id: fill
        height: parent.height
        width: parent.width * (PdfController.processing
                                ? Math.max(0.04, PdfController.progress)
                                : PdfController.progress)
        color: Theme.accent

        Behavior on width {
            NumberAnimation { duration: Theme.animNormal; easing.type: Easing.OutCubic }
        }
    }

    Rectangle {
        anchors.fill: parent
        visible: PdfController.processing && PdfController.progress < 0.99
                 && PdfController.fileCount > 0
        opacity: 0.35
        color: Theme.accent

        SequentialAnimation on opacity {
            loops: Animation.Infinite
            NumberAnimation { from: 0.15; to: 0.45; duration: 700 }
            NumberAnimation { from: 0.45; to: 0.15; duration: 700 }
        }
    }
}
