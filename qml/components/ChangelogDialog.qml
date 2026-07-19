import QtQuick
import QtQuick.Controls
import ProjectO

Item {
    id: root
    z: 3100

    property bool shown: false
    property real overlayOpacity: 0
    visible: shown || overlayOpacity > 0.001

    ListModel { id: changelogModel }

    Behavior on overlayOpacity {
        NumberAnimation { duration: Theme.animNormal; easing.type: Easing.OutCubic }
    }

    function localizedNotes(item) {
        const lang = AppSettings.language
        if (item.notesMap) {
            const map = item.notesMap
            if (map[lang])
                return map[lang]
            if (lang === "zh_TW" && map["zh_CN"])
                return map["zh_CN"]
            if (map["en"])
                return map["en"]
            if (map["zh_CN"])
                return map["zh_CN"]
        }
        return item.notes || ""
    }

    function versionLabel(version, date) {
        let label = version ? ("v" + version) : ""
        if (date)
            label += (label ? " " : "") + date
        return label
    }

    function refillChangelog() {
        const _ = AppSettings.languageRevision
        changelogModel.clear()
        const count = UpdateChecker.changelogEntryCount()
        for (let i = 0; i < count; ++i) {
            const item = UpdateChecker.changelogEntryAt(i)
            changelogModel.append({
                version: item.version || "",
                date: item.date || "",
                notes: localizedNotes(item)
            })
        }
    }

    function open() {
        refillChangelog()
        root.shown = true
        root.overlayOpacity = Theme.dimOpacity
        root.forceActiveFocus()
    }

    function close() {
        root.shown = false
        root.overlayOpacity = 0
    }

    Keys.onEscapePressed: root.close()

    Rectangle {
        anchors.fill: parent
        color: Theme.dimOverlay
        opacity: root.overlayOpacity
        MouseArea {
            anchors.fill: parent
            onClicked: root.close()
            onWheel: function(wheel) { wheel.accepted = true }
        }
    }

    Item {
        anchors.centerIn: parent
        width: card.width
        height: card.height
        scale: root.shown ? 1 : 0.92
        opacity: root.shown ? 1 : 0
        transformOrigin: Item.Center

        Behavior on scale {
            NumberAnimation {
                duration: Theme.animSlow
                easing.type: Easing.OutBack
                easing.overshoot: 1.08
            }
        }
        Behavior on opacity {
            NumberAnimation { duration: Theme.animNormal; easing.type: Easing.OutCubic }
        }

        Rectangle {
            anchors.fill: card
            anchors.topMargin: Theme.shadowOffset2
            radius: card.radius
            color: Theme.shadowColor
            opacity: Theme.shadowOpacity2
        }

        Rectangle {
            anchors.fill: card
            anchors.topMargin: Theme.shadowOffset1
            radius: card.radius
            color: Theme.shadowColor
            opacity: Theme.shadowOpacity1
        }

        Rectangle {
            id: card
            width: 380
            height: Math.max(300, Math.min(480, changelogList.contentHeight + 152))
            radius: Theme.radiusLg
            color: Theme.surface
            border.color: Theme.border
            border.width: 1

            MouseArea {
                anchors.fill: parent
                onClicked: {}
                onWheel: function(wheel) { wheel.accepted = true }
            }

            Column {
                anchors.fill: parent
                anchors.leftMargin: 20
                anchors.rightMargin: 20
                anchors.topMargin: 20
                anchors.bottomMargin: 28
                spacing: 12

                Text {
                    width: parent.width
                    horizontalAlignment: Text.AlignHCenter
                    text: Theme.tr("changelogTitle")
                    font: Theme.titleFont
                    color: Theme.text
                }

                Rectangle {
                    width: parent.width
                    height: 1
                    color: Theme.border
                }

                Item {
                    width: parent.width
                    height: parent.height - 100

                    ListView {
                        id: changelogList
                        anchors.fill: parent
                        clip: true
                        spacing: 10
                        model: changelogModel
                        ScrollBar.vertical: ScrollBar {
                            policy: ScrollBar.AsNeeded
                        }

                        delegate: Rectangle {
                            required property string version
                            required property string date
                            required property string notes

                            width: changelogList.width
                            radius: Theme.radiusSm
                            color: Theme.surfaceAlt
                            border.color: Theme.border
                            border.width: 1
                            implicitHeight: body.implicitHeight + 20

                            Column {
                                id: body
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.verticalCenter: parent.verticalCenter
                                anchors.margins: 10
                                spacing: 4

                                Text {
                                    width: parent.width
                                    text: root.versionLabel(version, date)
                                    font: Theme.captionBoldFont
                                    color: Theme.accent
                                }

                                Text {
                                    width: parent.width
                                    wrapMode: Text.Wrap
                                    text: notes
                                    font: Theme.mainFont
                                    color: Theme.textBody
                                }
                            }
                        }
                    }

                    Text {
                        anchors.centerIn: parent
                        width: parent.width - 24
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.Wrap
                        visible: changelogModel.count === 0
                        text: Theme.tr("changelogEmpty")
                        color: Theme.textSecondary
                        font: Theme.mainFont
                    }
                }

                Item { width: 1; height: 6 }

                StyledButton {
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: 120
                    text: Theme.tr("close")
                    highlighted: true
                    onClicked: root.close()
                }
            }
        }
    }
}
