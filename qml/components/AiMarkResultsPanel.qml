import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ProjectO

ColumnLayout {
    id: panel
    spacing: 6

    readonly property bool hasResults: AppController.redactions.autoCount > 0

    visible: hasResults
    Layout.fillWidth: true
    Layout.preferredHeight: hasResults ? implicitHeight : 0

    RowLayout {
        Layout.fillWidth: true
        spacing: 6

        Text {
            text: Theme.tr("aiMarkResults")
            font: Theme.captionBoldFont
            color: Theme.text
        }

        Text {
            text: AppController.redactions.autoCount.toString()
            font: Theme.captionFont
            color: Theme.maskAuto
        }

        Item { Layout.fillWidth: true }
    }

    Rectangle {
        Layout.fillWidth: true
        Layout.preferredHeight: Math.min(220, resultsList.contentHeight + 8)
        Layout.maximumHeight: 220
        radius: Theme.radiusSm
        color: Theme.surfaceAlt
        border.color: Theme.border
        border.width: 1
        clip: true

        ScrollView {
            anchors.fill: parent
            anchors.margins: 4
            clip: true
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

            Column {
                id: resultsList
                width: parent.width
                spacing: 6

                Repeater {
                    model: AppController.redactions

                    delegate: Rectangle {
                        required property int regionId
                        required property string source
                        required property string label
                        required property string content
                        required property bool isSelected

                        width: resultsList.width
                        height: row.implicitHeight + 12
                        radius: Theme.radiusSm
                        visible: source === "auto"
                        color: isSelected
                               ? (Theme.dark ? "#F59E0B22" : "#F59E0B18")
                               : Theme.surface
                        border.width: 1
                        border.color: isSelected ? Theme.maskAuto : Theme.border

                        RowLayout {
                            id: row
                            anchors.fill: parent
                            anchors.margins: 6
                            spacing: 6

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 2

                                Text {
                                    Layout.fillWidth: true
                                    text: {
                                        const body = content.length > 0 ? content : label
                                        return Theme.tr("aiMarkContentPrefix") + body
                                    }
                                    font: Theme.captionFont
                                    color: Theme.textBody
                                    wrapMode: Text.Wrap
                                    maximumLineCount: 2
                                    elide: Text.ElideRight
                                }

                                Text {
                                    Layout.fillWidth: true
                                    visible: content.length > 0 && label.length > 0
                                    text: label
                                    font: Theme.captionFont
                                    color: Theme.textSecondary
                                    elide: Text.ElideRight
                                }
                            }

                            IconDeleteButton {
                                Layout.alignment: Qt.AlignTop
                                onClicked: AppController.redactions.removeById(regionId)
                            }
                        }

                        MouseArea {
                            anchors.fill: parent
                            z: -1
                            onClicked: AppController.redactions.selectedId = regionId
                        }
                    }
                }
            }
        }
    }
}
