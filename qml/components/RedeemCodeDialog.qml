import QtQuick

import QtQuick.Controls

import QtQuick.Layouts

import ProjectO



Item {

    id: root

    anchors.fill: parent

    visible: shown

    z: 20



    property bool shown: false

    property real overlayOpacity: 0



    function open() {

        codeField.text = ""

        RedemptionClient.clearStatus()

        shown = true

        overlayOpacity = Theme.dimOpacity

        forceActiveFocus()

        Qt.callLater(function() {

            codeField.forceActiveFocus()

            codeField.selectAll()

        })

    }



    function close() {

        if (!shown)

            return

        RedemptionClient.cancelRedeem()

        shown = false

        overlayOpacity = 0

    }



    function submit() {

        if (RedemptionClient.busy)

            return

        RedemptionClient.redeem(codeField.text.trim())

    }



    Behavior on overlayOpacity {

        NumberAnimation { duration: Theme.animNormal; easing.type: Easing.OutCubic }

    }



    Keys.onEscapePressed: root.close()

    Keys.onReturnPressed: root.submit()

    Keys.onEnterPressed: root.submit()



    Rectangle {

        anchors.fill: parent

        color: Theme.dimOverlay

        opacity: root.overlayOpacity



        MouseArea {

            anchors.fill: parent

            enabled: root.shown

            onClicked: root.close()

        }

    }



    Item {

        id: dialogLayer

        anchors.centerIn: parent

        width: card.width

        height: card.height

        scale: root.shown ? 1 : 0.94

        opacity: root.shown ? 1 : 0

        transformOrigin: Item.Center

        enabled: root.shown



        Behavior on scale {

            NumberAnimation {

                duration: Theme.animSlow

                easing.type: Easing.OutBack

                easing.overshoot: 1.06

            }

        }

        Behavior on opacity {

            NumberAnimation { duration: Theme.animNormal; easing.type: Easing.OutCubic }

        }



        Rectangle {

            id: card

            width: 320

            height: content.implicitHeight + 40

            radius: Theme.radiusLg

            color: Theme.surface

            border.color: Theme.border

            border.width: 1



            MouseArea {

                anchors.fill: parent

                enabled: root.shown

                onClicked: {}

            }



            ColumnLayout {

                id: content

                anchors.left: parent.left

                anchors.right: parent.right

                anchors.top: parent.top

                anchors.margins: 20

                spacing: 12



                Text {

                    Layout.fillWidth: true

                    text: Theme.tr("settingRedeemSection")

                    font: Theme.mainFontBold

                    color: Theme.text

                }



                ThemedTextField {

                    id: codeField

                    Layout.fillWidth: true

                    placeholderText: Theme.tr("settingRedeemCodePlaceholder")

                    enabled: !RedemptionClient.busy

                    onTextEdited: {

                        if (RedemptionClient.statusMessage.length > 0)

                            RedemptionClient.clearStatus()

                    }

                }



                Text {

                    Layout.fillWidth: true

                    wrapMode: Text.Wrap

                    visible: RedemptionClient.statusMessage.length > 0

                    text: RedemptionClient.statusMessage

                    font: Theme.captionFont

                    color: RedemptionClient.statusOk ? Theme.accent : Theme.danger

                }



                RowLayout {

                    Layout.fillWidth: true

                    spacing: 8



                    Item { Layout.fillWidth: true }



                    StyledButton {

                        text: Theme.tr("pickerCancel")

                        enabled: !RedemptionClient.busy

                        onClicked: root.close()

                    }



                    StyledButton {

                        text: RedemptionClient.busy

                              ? Theme.tr("redeemWorking")

                              : Theme.tr("pickerOk")

                        highlighted: true

                        enabled: !RedemptionClient.busy

                        onClicked: root.submit()

                    }

                }

            }

        }

    }



    Connections {

        target: RedemptionClient

        function onRedeemed() {

            closeTimer.start()

        }

    }



    Timer {

        id: closeTimer

        interval: 600

        repeat: false

        onTriggered: root.close()

    }

}


