import QtQuick
import QtQuick.Controls
import ProjectO

ComboBox {
    id: control

    readonly property int comboFontSize: 15

    hoverEnabled: true
    wheelEnabled: false
    focusPolicy: Qt.ClickFocus

    WheelHandler {
        onWheel: function(event) {
            event.accepted = true
        }
    }

    indicator: Text {
        x: control.width - width - 10
        y: (control.height - height) / 2
        text: "▴"
        font.pixelSize: 14
        color: Theme.text
    }

    background: Rectangle {
        radius: Theme.radiusSm
        color: control.hovered || control.popup.visible ? Theme.menuHover : Theme.surfaceAlt
        border.color: control.popup.visible ? Theme.accent : Theme.border
        border.width: control.popup.visible ? 2 : 1
        clip: true

        Behavior on color {
            ColorAnimation { duration: Theme.animFast; easing.type: Easing.OutCubic }
        }

        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.NoButton
            propagateComposedEvents: true
            onWheel: function(wheel) {
                wheel.accepted = true
            }
            onPressed: function(mouse) {
                mouse.accepted = false
            }
        }
    }

    onActivated: control.focus = false

    contentItem: Text {
        anchors.fill: parent
        leftPadding: control.indicator.width + 8
        rightPadding: control.indicator.width + 8
        text: control.displayText
        font.pixelSize: control.comboFontSize
        font.family: Theme.mainFont.family
        font.weight: Font.Normal
        color: Theme.text
        verticalAlignment: Text.AlignVCenter
        horizontalAlignment: Text.AlignHCenter
        elide: Text.ElideRight
    }

    delegate: ItemDelegate {
        id: del
        width: ListView.view ? ListView.view.width : control.width
        height: 36
        leftPadding: 0
        rightPadding: 0
        topPadding: 0
        bottomPadding: 0
        spacing: 0
        required property var modelData
        text: modelData && modelData.label !== undefined ? modelData.label : ""
        palette.text: del.highlighted ? "#FFFFFF" : Theme.text
        palette.highlight: Theme.accent
        palette.highlightedText: "#FFFFFF"
        font.pixelSize: control.comboFontSize
        font.family: Theme.mainFont.family
        font.weight: Font.Normal
        contentItem: Text {
            width: parent.width
            height: parent.height
            text: del.text
            font: del.font
            color: del.highlighted ? "#FFFFFF" : Theme.text
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }
        background: Rectangle {
            radius: 4
            anchors.fill: parent
            color: del.highlighted ? Theme.accent
                   : (del.hovered ? Theme.menuHover : "transparent")
        }
    }

    popup: Popup {
        id: comboPopup
        // Positioned above the control before it is ever shown, so the first
        // open never flashes at the default (below) position.
        y: -height - 4
        width: control.width
        padding: 0
        modal: false
        dim: false

        property bool reveal: false
        onAboutToShow: reveal = true
        onAboutToHide: reveal = false

        onClosed: control.focus = false

        enter: Transition {
            NumberAnimation {
                property: "opacity"; from: 0; to: 1
                duration: Theme.animFast; easing.type: Easing.OutCubic
            }
        }
        exit: Transition {
            NumberAnimation {
                property: "opacity"; from: 1; to: 0
                duration: Theme.animNormal; easing.type: Easing.InCubic
            }
        }

        background: null

        contentItem: Item {
            implicitHeight: comboList.height + 12

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: comboPopup.reveal ? parent.height : 0
                radius: Theme.radiusSm
                color: Theme.surface
                border.color: Theme.border
                border.width: 1
                clip: true

                Behavior on height {
                    NumberAnimation { duration: Theme.animNormal; easing.type: Easing.OutCubic }
                }

                ListView {
                    id: comboList
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    anchors.margins: 6
                    height: Math.min(contentHeight, 180)
                    clip: true
                    model: control.popup.visible ? control.delegateModel : null
                    currentIndex: control.highlightedIndex
                    spacing: 2
                }
            }
        }
    }
}
