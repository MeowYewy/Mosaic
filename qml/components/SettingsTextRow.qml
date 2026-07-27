import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ProjectO

ColumnLayout {
    id: row
    Layout.fillWidth: true
    spacing: 6

    property string label: ""
    property string value: ""
    property string placeholderText: ""
    property bool secret: false
    property bool enabled: true

    signal edited(string text)

    function commit() {
        if (field.text !== row.value)
            row.edited(field.text)
    }

    onValueChanged: {
        if (field.text !== row.value)
            field.text = row.value
    }

    Text {
        Layout.fillWidth: true
        text: row.label
        font: Theme.captionBoldFont
        color: Theme.textSecondary
    }

    TextField {
        id: field
        Layout.fillWidth: true
        Layout.preferredHeight: 38
        text: row.value
        placeholderText: row.placeholderText
        font: Theme.mainFont
        color: Theme.text
        placeholderTextColor: Theme.textSecondary
        echoMode: row.secret ? TextInput.Password : TextInput.Normal
        selectByMouse: true
        enabled: row.enabled
        readOnly: !row.enabled
        onTextChanged: saveTimer.restart()
        onEditingFinished: row.commit()
        onAccepted: row.commit()
        background: Rectangle {
            radius: Theme.radiusSm
            color: Theme.surfaceAlt
            border.color: field.activeFocus ? Theme.accent : Theme.border
            border.width: field.activeFocus ? 2 : 1
        }
    }

    Timer {
        id: saveTimer
        interval: 400
        repeat: false
        onTriggered: row.commit()
    }
}
