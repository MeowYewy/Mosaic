import QtQuick
import QtQuick.Layouts
import ProjectO

Row {
    id: root
    Layout.fillWidth: true
    spacing: 3

    readonly property real cellWidth: root.width > 0
                                      ? Math.max(10, (root.width - spacing * 17) / 18)
                                      : 14
    readonly property real cellHeight: cellWidth * 4 / 3

    Repeater {
        model: 18
        ThemeToggleButton {
            width: root.cellWidth
            height: root.cellHeight
            label: String(index + 1)
            labelSize: Math.max(8, Math.round(root.cellWidth * 0.55))
            outlined: true
            selected: root.idCardDigitOn(index + 1)
            onClicked: root.toggleIdCardDigit(index + 1)
        }
    }

    function idCardDigitOn(digit) {
        var _rev = AppSettings.privacyPolicyRevision
        return AppSettings.idCardDigitEnabled(digit)
    }

    function toggleIdCardDigit(digit) {
        AppSettings.setIdCardDigitEnabled(digit, !idCardDigitOn(digit))
    }
}
