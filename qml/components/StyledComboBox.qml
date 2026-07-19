import QtQuick
import QtQuick.Controls
import ProjectO

ComboBox {
    id: control

    property Item popupParent: null
    property bool popupOpensUpward: false
    property bool blockWheel: false

    wheelEnabled: !blockWheel

    WheelHandler {
        enabled: control.blockWheel
        onWheel: function(event) {
            event.accepted = true
        }
    }

    implicitHeight: 36
    implicitWidth: 100

    palette.window: Theme.surface
    palette.windowText: Theme.text
    palette.text: Theme.text
    palette.button: Theme.surface
    palette.buttonText: Theme.text
    palette.highlight: Theme.accent
    palette.highlightedText: "#FFFFFF"
    palette.base: Theme.surface
    palette.alternateBase: Theme.surfaceAlt

    background: Rectangle {
        radius: Theme.radiusSm
        color: Theme.surface
        border.color: control.activeFocus || control.popup.visible ? Theme.accent : Theme.border
        border.width: control.activeFocus || control.popup.visible ? 2 : 1

        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.NoButton
            propagateComposedEvents: true
            enabled: control.blockWheel
            onWheel: function(wheel) {
                wheel.accepted = true
            }
        }
    }

    contentItem: Text {
        leftPadding: 12
        rightPadding: control.indicator.width + 12
        text: control.displayText
        font.pixelSize: Theme.mainFont.pixelSize
        font.family: Theme.mainFont.family
        font.weight: Font.DemiBold
        color: Theme.text
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    delegate: ItemDelegate {
        id: del
        width: control.width
        height: 34
        required property var modelData
        required property int index
        text: {
            if (control.textRole && control.model && control.model[index] !== undefined) {
                const item = control.model[index]
                if (item && typeof item === "object" && item[control.textRole] !== undefined)
                    return String(item[control.textRole])
            }
            return String(modelData)
        }
        palette.text: del.highlighted ? "#FFFFFF" : Theme.text
        palette.highlight: Theme.accent
        palette.highlightedText: "#FFFFFF"
        font.weight: del.highlighted ? Font.DemiBold : Font.Medium
        background: Rectangle {
            radius: 4
            color: del.highlighted ? Theme.accent
                   : (del.hovered ? Theme.menuHover : "transparent")
        }
    }

    popup: Popup {
        parent: control.popupParent
        z: control.popupParent ? 10050 : 0
        x: control.popupParent
           ? control.mapToItem(control.popupParent, 0, 0).x
           : 0
        y: control.popupParent
           ? (control.popupOpensUpward
              ? control.mapToItem(control.popupParent, 0, 0).y - height - 4
              : control.mapToItem(control.popupParent, 0, 0).y + control.height + 4)
           : control.height + 4
        width: Math.max(control.width, 100)
        padding: 6
        modal: control.popupParent !== null
        dim: false
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        onOpened: {
            if (!control.popupParent)
                return
            x = control.mapToItem(control.popupParent, 0, 0).x
            if (control.popupOpensUpward)
                y = control.mapToItem(control.popupParent, 0, 0).y - height - 4
            else
                y = control.mapToItem(control.popupParent, 0, 0).y + control.height + 4
        }

        background: Rectangle {
            radius: Theme.radiusSm
            color: Theme.surface
            border.color: Theme.border
            border.width: 1
        }

        contentItem: ListView {
            clip: true
            implicitHeight: Math.min(contentHeight, 180)
            model: control.popup.visible ? control.delegateModel : null
            currentIndex: control.highlightedIndex
            spacing: 2
            ScrollIndicator.vertical: ScrollIndicator { }
        }
    }
}
