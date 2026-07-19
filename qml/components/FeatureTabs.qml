import QtQuick
import QtQuick.Controls
import ProjectO

Item {
    id: tabs
    implicitWidth: chromeless ? row.implicitWidth : row.implicitWidth + 8
    implicitHeight: 44

    readonly property int tabWidth: 96
    readonly property int tabSpacing: 4
    property bool chromeless: false

    property int currentIndex: 0
    signal tabChanged(int index)

    readonly property var labels: {
        const _rev = AppSettings.languageRevision
        return [
            Theme.tr("tabSplit"),
            Theme.tr("tabMerge"),
            Theme.tr("tabRotate"),
            Theme.tr("tabConvert"),
            Theme.tr("tabCompress"),
            Theme.tr("tabWatermark")
        ]
    }

    Rectangle {
        anchors.fill: parent
        radius: Theme.radiusMd
        color: Theme.tabInactive
        visible: !tabs.chromeless
    }

    Rectangle {
        id: indicator
        z: 0
        visible: !AppSettings.maskMode
        width: tabs.tabWidth
        height: 36
        radius: Theme.radiusSm
        color: Theme.surface
        border.color: Theme.border
        border.width: 1
        y: 4
        x: (tabs.chromeless ? 0 : 4) + tabs.currentIndex * (tabs.tabWidth + tabs.tabSpacing)

        Behavior on x {
            NumberAnimation { duration: Theme.animNormal; easing.type: Easing.OutCubic }
        }
    }

    Row {
        id: row
        z: 1
        x: tabs.chromeless ? 0 : 4
        y: 4
        spacing: tabs.tabSpacing

        Repeater {
            model: tabs.labels.length

            Item {
                width: tabs.tabWidth
                height: 36

                Text {
                    anchors.centerIn: parent
                    text: tabs.labels[index]
                    font: Theme.tabFont
                    color: tabs.currentIndex === index && !AppSettings.maskMode
                           ? Theme.text : Theme.textBody
                }

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        tabs.currentIndex = index
                        tabs.tabChanged(index)
                    }
                }
            }
        }
    }
}
