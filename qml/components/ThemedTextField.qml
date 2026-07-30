import QtQuick
import QtQuick.Controls
import ProjectO

Item {
    id: root

    implicitHeight: 38
    implicitWidth: 240

    property string text: ""
    property string placeholderText: ""
    property int echoMode: TextInput.Normal
    property bool readOnly: false

    readonly property alias length: input.length

    signal textEdited(string text)
    signal accepted()
    signal editingFinished()

    function forceActiveFocus() {
        input.forceActiveFocus()
    }

    function selectAll() {
        input.selectAll()
    }

    function clear() {
        text = ""
    }

    Rectangle {
        anchors.fill: parent
        radius: Theme.radiusSm
        color: Theme.surfaceAlt
        border.color: Theme.border
        border.width: 1
        opacity: root.enabled ? 1 : 0.52
    }

    Rectangle {
        anchors.fill: parent
        radius: Theme.radiusSm
        color: "transparent"
        border.color: Theme.accent
        border.width: 2
        opacity: input.activeFocus && root.enabled ? 1 : 0
        z: 1

        Behavior on opacity {
            NumberAnimation {
                duration: Theme.animNormal
                easing.type: Easing.OutCubic
            }
        }
    }

    TextField {
        id: input
        anchors.fill: parent
        z: 2
        text: root.text
        placeholderText: root.placeholderText
        echoMode: root.echoMode
        enabled: root.enabled
        readOnly: root.readOnly
        font: Theme.mainFont
        color: Theme.text
        placeholderTextColor: Theme.textSecondary
        selectByMouse: true
        verticalAlignment: TextInput.AlignVCenter
        leftPadding: 10
        rightPadding: 10
        background: null

        onTextChanged: {
            if (text !== root.text) {
                root.text = text
                root.textEdited(text)
            }
        }
        onAccepted: root.accepted()
        onEditingFinished: root.editingFinished()
    }

    onTextChanged: {
        if (input.text !== root.text)
            input.text = root.text
    }
}
