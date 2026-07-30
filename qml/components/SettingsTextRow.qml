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

    readonly property string currentText: field.text

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

    ThemedTextField {
        id: field
        Layout.fillWidth: true
        text: row.value
        placeholderText: row.placeholderText
        echoMode: row.secret ? TextInput.Password : TextInput.Normal
        enabled: row.enabled
        readOnly: !row.enabled
        onTextEdited: saveTimer.restart()
        onEditingFinished: row.commit()
        onAccepted: row.commit()
    }

    Timer {
        id: saveTimer
        interval: 400
        repeat: false
        onTriggered: row.commit()
    }
}
