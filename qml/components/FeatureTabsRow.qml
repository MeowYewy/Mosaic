import QtQuick
import QtQuick.Controls
import ProjectO

Item {
    id: row
    implicitWidth: barRow.implicitWidth + 8
    implicitHeight: 44

    property int currentIndex: 0
    property bool maskPinned: false
    signal tabChanged(int index)
    signal maskRequested()

    property alias maskButton: maskBtn
    readonly property int tabWidth: pdfTabs.tabWidth

    Rectangle {
        anchors.fill: parent
        radius: Theme.radiusMd
        color: Theme.tabInactive
    }

    Row {
        id: barRow
        anchors.left: parent.left
        anchors.leftMargin: 4
        anchors.verticalCenter: parent.verticalCenter
        spacing: 0

        Item {
            id: maskBtn
            width: row.tabWidth
            height: 44

            Item {
                id: maskTab
                anchors.top: parent.top
                anchors.topMargin: 4
                width: parent.width
                height: 36
                scale: maskArea.pressed ? 0.97 : 1.0
                opacity: row.maskPinned ? 0 : 1
                Behavior on scale { NumberAnimation { duration: Theme.animFast } }

                Text {
                    anchors.centerIn: parent
                    text: Theme.tr("tabMask")
                    font: Theme.tabFont
                    color: Theme.textBody
                }

                MouseArea {
                    id: maskArea
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: row.maskRequested()
                }
            }
        }

        Item {
            width: tabSep.implicitWidth + 12
            height: 44

            Text {
                id: tabSep
                anchors.centerIn: parent
                anchors.verticalCenterOffset: 0
                text: "|"
                font: Theme.tabFont
                color: Theme.textSecondary
                opacity: 0.45
            }
        }

        FeatureTabs {
            id: pdfTabs
            chromeless: true
            currentIndex: row.currentIndex
            onTabChanged: function(index) {
                row.currentIndex = index
                row.tabChanged(index)
            }
        }
    }
}
