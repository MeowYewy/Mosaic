import QtQuick
import QtQuick.Controls
import ProjectO

CompactComboBox {
    id: control

    implicitHeight: 38
    implicitWidth: Theme.compactControlWidth

    textRole: "label"
    valueRole: "value"

    readonly property var levelOptions: {
        const _ = AppSettings.languageRevision
        return [
            { label: Theme.tr("compressLow"), value: 0 },
            { label: Theme.tr("compressMid"), value: 1 },
            { label: Theme.tr("compressHigh"), value: 2 }
        ]
    }

    model: levelOptions
    currentIndex: 0
    onActivated: levelValue = levelOptions[currentIndex].value

    property int levelValue: 0

    palette.window: Theme.surfaceAlt
    palette.windowText: Theme.text
    palette.text: Theme.text
    palette.button: Theme.surfaceAlt
    palette.buttonText: Theme.text
    palette.highlight: Theme.accent
    palette.highlightedText: "#FFFFFF"
    palette.base: Theme.surfaceAlt
}
