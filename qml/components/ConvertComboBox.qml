import QtQuick
import QtQuick.Controls
import ProjectO

CompactComboBox {
    id: control

    implicitHeight: 38
    implicitWidth: Theme.compactControlWidth

    textRole: "label"
    valueRole: "value"

    readonly property var formatOptions: {
        const _ = AppSettings.languageRevision
        return [
            { label: Theme.tr("formatPdf"), value: 0 },
            { label: Theme.tr("formatWord"), value: 3 },
            { label: Theme.tr("formatPng"), value: 1 },
            { label: Theme.tr("formatJpeg"), value: 2 }
        ]
    }

    model: formatOptions
    currentIndex: 0
    onActivated: formatValue = formatOptions[currentIndex].value

    property int formatValue: 0

    palette.window: Theme.surfaceAlt
    palette.windowText: Theme.text
    palette.text: Theme.text
    palette.button: Theme.surfaceAlt
    palette.buttonText: Theme.text
    palette.highlight: Theme.accent
    palette.highlightedText: "#FFFFFF"
    palette.base: Theme.surfaceAlt
}
