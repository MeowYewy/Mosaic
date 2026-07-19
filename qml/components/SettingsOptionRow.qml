import QtQuick
import QtQuick.Layouts
import ProjectO

RowLayout {
    id: row
    spacing: 8

    property string title: ""
    property bool value: false

    signal toggled(bool on)

    Text {
        Layout.fillWidth: true
        text: row.title
        font: Theme.mainFont
        color: Theme.text
        elide: Text.ElideRight
    }

    RowLayout {
        spacing: 4

        ThemeToggleButton {
            Layout.preferredWidth: 52
            Layout.preferredHeight: 28
            label: Theme.tr("settingOn")
            selected: row.value
            onClicked: row.toggled(true)
        }
        ThemeToggleButton {
            Layout.preferredWidth: 52
            Layout.preferredHeight: 28
            label: Theme.tr("settingOff")
            selected: !row.value
            onClicked: row.toggled(false)
        }
    }
}
