import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ProjectO

Item {
    id: root
    visible: false
    z: 3000
    opacity: 0
    property real cardScale: 0.96

    function open() {
        opacity = 0
        cardScale = 0.96
        visible = true
        openAnim.restart()
        forceActiveFocus()
    }

    function close() {
        if (!visible || closeAnim.running)
            return
        closeAnim.restart()
    }

    Keys.onEscapePressed: root.close()

    ParallelAnimation {
        id: openAnim
        NumberAnimation { target: root; property: "opacity"; to: 1; duration: Theme.animNormal }
        NumberAnimation { target: root; property: "cardScale"; to: 1; duration: Theme.animNormal }
    }
    SequentialAnimation {
        id: closeAnim
        ParallelAnimation {
            NumberAnimation { target: root; property: "opacity"; to: 0; duration: Theme.animFast }
            NumberAnimation { target: root; property: "cardScale"; to: 0.96; duration: Theme.animFast }
        }
        ScriptAction { script: root.visible = false }
    }

    Rectangle {
        anchors.fill: parent
        color: Theme.dimOverlay
        opacity: Theme.dimOpacity
        MouseArea { anchors.fill: parent; onClicked: root.close() }
    }

    Rectangle {
        id: card
        anchors.centerIn: parent
        width: 300
        height: settingsColumn.implicitHeight + 36
        radius: Theme.radiusLg
        color: Theme.surface
        border.color: Theme.border
        border.width: 1
        scale: root.cardScale
        clip: false

        ColumnLayout {
            id: settingsColumn
            anchors.fill: parent
            anchors.margins: 16
            spacing: 12

            Text {
                Layout.fillWidth: true
                text: Theme.tr("settings")
                font: Theme.mainFontBold
                color: Theme.text
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                Text {
                    Layout.fillWidth: true
                    text: Theme.tr("language")
                    font: Theme.mainFont
                    color: Theme.text
                }

                RowLayout {
                    spacing: 4

                    ThemeToggleButton {
                        Layout.preferredWidth: 72
                        Layout.preferredHeight: 28
                        label: "简体中文"
                        selected: AppSettings.language === "zh_CN"
                        onClicked: AppSettings.setLanguage("zh_CN")
                    }
                    ThemeToggleButton {
                        Layout.preferredWidth: 72
                        Layout.preferredHeight: 28
                        label: "English"
                        selected: AppSettings.language === "en"
                        onClicked: AppSettings.setLanguage("en")
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                Text {
                    Layout.fillWidth: true
                    text: Theme.tr("theme")
                    font: Theme.mainFont
                    color: Theme.text
                }

                RowLayout {
                    spacing: 4

                    ThemeToggleButton {
                        Layout.preferredWidth: 52
                        Layout.preferredHeight: 28
                        label: Theme.tr("light")
                        selected: AppSettings.theme === "light"
                        onClicked: AppSettings.setTheme("light")
                    }
                    ThemeToggleButton {
                        Layout.preferredWidth: 52
                        Layout.preferredHeight: 28
                        label: Theme.tr("dark")
                        selected: AppSettings.theme === "dark"
                        onClicked: AppSettings.setTheme("dark")
                    }
                }
            }

            SettingsOptionRow {
                Layout.fillWidth: true
                title: Theme.tr("settingCustomPicker")
                value: AppSettings.customFilePicker
                onToggled: function(on) { AppSettings.setCustomFilePicker(on) }
            }

            SettingsOptionRow {
                Layout.fillWidth: true
                title: Theme.tr("settingModeTransition")
                value: AppSettings.modeTransition
                onToggled: function(on) { AppSettings.setModeTransition(on) }
            }

            StyledButton {
                Layout.alignment: Qt.AlignHCenter
                Layout.topMargin: 4
                Layout.preferredWidth: 100
                text: Theme.tr("close")
                highlighted: true
                onClicked: root.close()
            }
        }
    }
}
