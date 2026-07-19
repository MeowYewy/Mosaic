import QtQuick
import QtQuick.Controls
import ProjectO

CompactComboBox {
    id: control

    implicitHeight: 38
    implicitWidth: Theme.compactControlWidth

    textRole: "label"
    valueRole: "value"

    readonly property var angleOptions: [
        { label: "90°", value: 90 },
        { label: "180°", value: 180 },
        { label: "270°", value: 270 }
    ]

    model: angleOptions
    currentIndex: 0
    onActivated: rotateValue = angleOptions[currentIndex].value

    property int rotateValue: 90

    palette.window: Theme.surfaceAlt
    palette.windowText: Theme.text
    palette.text: Theme.text
    palette.button: Theme.surfaceAlt
    palette.buttonText: Theme.text
    palette.highlight: Theme.accent
    palette.highlightedText: "#FFFFFF"
    palette.base: Theme.surfaceAlt
}
