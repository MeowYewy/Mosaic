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

    function privacyEnabled(key) {
        var _rev = AppSettings.privacyPolicyRevision
        return AppSettings.privacyMaskEnabled(key)
    }

    function setPrivacy(key, on) {
        AppSettings.setPrivacyMaskEnabled(key, on)
    }

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
        aiApiBaseRow.commit()
        aiApiKeyRow.commit()
        aiModelRow.commit()
        closeAnim.restart()
    }

    Keys.onEscapePressed: {
        if (redeemDialog.shown) {
            redeemDialog.close()
            return
        }
        root.close()
    }

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
        width: 380
        height: Math.min(settingsScroll.contentHeight + 32, parent.height * 0.86)
        radius: Theme.radiusLg
        color: Theme.surface
        border.color: Theme.border
        border.width: 1
        scale: root.cardScale
        transformOrigin: Item.Center
        clip: false

        ScrollView {
            id: settingsScroll
            anchors.fill: parent
            anchors.topMargin: 16
            anchors.bottomMargin: 16
            anchors.leftMargin: 16
            anchors.rightMargin: 3
            clip: true
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

            ColumnLayout {
                id: settingsColumn
                // Overlay scrollbar on Windows sits on content; reserve a fixed gutter.
                width: Math.max(0, settingsScroll.width - 13)
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

            Rectangle {
                Layout.fillWidth: true
                Layout.topMargin: 4
                height: 1
                color: Theme.border
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                Text {
                    Layout.fillWidth: true
                    text: Theme.tr("settingAiSection")
                    font: Theme.mainFontBold
                    color: Theme.text
                }

                StyledButton {
                    text: Theme.tr("settingRedeemButton")
                    onClicked: redeemDialog.open()
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                Text {
                    Layout.preferredWidth: 72
                    text: Theme.tr("settingAiMode")
                    font: Theme.captionFont
                    color: Theme.textSecondary
                }

                ThemeToggleButton {
                    Layout.preferredWidth: 80
                    Layout.preferredHeight: 28
                    label: Theme.tr("settingAiModeText")
                    selected: AppSettings.aiMarkMode === "text"
                    onClicked: {
                        aiApiKeyRow.commit()
                        AppSettings.setAiMarkMode("text")
                        if (AppSettings.aiModel.indexOf("ocr") >= 0)
                            AppSettings.applyAiPreset("qwen")
                    }
                }
                ThemeToggleButton {
                    Layout.preferredWidth: 88
                    Layout.preferredHeight: 28
                    label: Theme.tr("settingAiModeOcr")
                    selected: AppSettings.aiMarkMode === "qwen_ocr"
                    onClicked: {
                        aiApiKeyRow.commit()
                        AppSettings.applyAiPreset("qwen_ocr")
                    }
                }
                Item { Layout.fillWidth: true }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                visible: AppSettings.aiMarkMode === "qwen_ocr"

                Text {
                    Layout.preferredWidth: 72
                    text: Theme.tr("settingAiOcrCloud")
                    font: Theme.captionFont
                    color: Theme.textSecondary
                }

                ThemeToggleButton {
                    Layout.preferredWidth: 72
                    Layout.preferredHeight: 28
                    label: Theme.tr("settingAiOcrCloudSingle")
                    selected: AppSettings.aiOcrCloudMode === "single"
                    onClicked: AppSettings.setAiOcrCloudMode("single")
                }
                ThemeToggleButton {
                    Layout.preferredWidth: 72
                    Layout.preferredHeight: 28
                    label: Theme.tr("settingAiOcrCloudDual")
                    selected: AppSettings.aiOcrCloudMode === "dual"
                    onClicked: AppSettings.setAiOcrCloudMode("dual")
                }
                Item { Layout.fillWidth: true }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                visible: AppSettings.aiMarkMode === "text"

                Text {
                    Layout.preferredWidth: 72
                    text: Theme.tr("settingAiTextProvider")
                    font: Theme.captionFont
                    color: Theme.textSecondary
                }

                ThemeToggleButton {
                    Layout.preferredWidth: 64
                    Layout.preferredHeight: 28
                    label: "Kimi"
                    selected: AppSettings.aiApiBase.indexOf("moonshot") >= 0
                    onClicked: {
                        aiApiKeyRow.commit()
                        AppSettings.applyAiPreset("kimi")
                    }
                }
                ThemeToggleButton {
                    Layout.preferredWidth: 72
                    Layout.preferredHeight: 28
                    label: Theme.tr("settingAiQwen")
                    selected: AppSettings.aiApiBase.indexOf("dashscope") >= 0
                              && AppSettings.aiMarkMode === "text"
                    onClicked: {
                        aiApiKeyRow.commit()
                        AppSettings.applyAiPreset("qwen")
                    }
                }
                Item { Layout.fillWidth: true }
            }

            SettingsTextRow {
                id: aiApiBaseRow
                Layout.fillWidth: true
                label: Theme.tr("settingAiApiBase")
                value: AppSettings.aiApiBase
                placeholderText: AppSettings.aiMarkMode === "qwen_ocr"
                                 ? "https://dashscope.aliyuncs.com/api/v1"
                                 : "https://dashscope.aliyuncs.com/compatible-mode/v1"
                onEdited: function(text) { AppSettings.setAiApiBase(text) }
            }

            SettingsTextRow {
                id: aiApiKeyRow
                Layout.fillWidth: true
                label: Theme.tr("settingAiApiKey")
                value: AppSettings.aiApiKey
                placeholderText: "sk-..."
                secret: true
                onEdited: function(text) { AppSettings.setAiApiKey(text) }
            }

            SettingsTextRow {
                id: aiModelRow
                Layout.fillWidth: true
                label: Theme.tr("settingAiModel")
                value: AppSettings.aiModel
                placeholderText: AppSettings.aiMarkMode === "qwen_ocr" ? "qwen3.5-ocr" : "qwen-plus"
                onEdited: function(text) { AppSettings.setAiModel(text) }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.topMargin: 4
                height: 1
                color: Theme.border
            }

            Text {
                Layout.fillWidth: true
                text: Theme.tr("settingPrivacySection")
                font: Theme.mainFontBold
                color: Theme.text
            }

            SettingsOptionRow {
                Layout.fillWidth: true
                title: Theme.tr("privacyMaskName")
                value: root.privacyEnabled("name")
                onToggled: function(on) { root.setPrivacy("name", on) }
            }
            SettingsOptionRow {
                Layout.fillWidth: true
                title: Theme.tr("privacyMaskGender")
                value: root.privacyEnabled("gender")
                onToggled: function(on) { root.setPrivacy("gender", on) }
            }
            SettingsOptionRow {
                Layout.fillWidth: true
                title: Theme.tr("privacyMaskAge")
                value: root.privacyEnabled("age")
                onToggled: function(on) { root.setPrivacy("age", on) }
            }
            SettingsOptionRow {
                Layout.fillWidth: true
                title: Theme.tr("privacyMaskHospital")
                value: root.privacyEnabled("hospital")
                onToggled: function(on) { root.setPrivacy("hospital", on) }
            }
            SettingsOptionRow {
                Layout.fillWidth: true
                title: Theme.tr("privacyMaskDoctor")
                value: root.privacyEnabled("doctor")
                onToggled: function(on) { root.setPrivacy("doctor", on) }
            }
            SettingsOptionRow {
                Layout.fillWidth: true
                title: Theme.tr("privacyMaskInpatientId")
                value: root.privacyEnabled("inpatientId")
                onToggled: function(on) { root.setPrivacy("inpatientId", on) }
            }
            SettingsOptionRow {
                Layout.fillWidth: true
                title: Theme.tr("privacyMaskBedNumber")
                value: root.privacyEnabled("bedNumber")
                onToggled: function(on) { root.setPrivacy("bedNumber", on) }
            }
            SettingsOptionRow {
                Layout.fillWidth: true
                title: Theme.tr("privacyMaskIdCard")
                value: root.privacyEnabled("idCard")
                onToggled: function(on) { root.setPrivacy("idCard", on) }
            }

            SettingsIdCardDigitRow {
                Layout.fillWidth: true
                visible: root.privacyEnabled("idCard")
            }

            SettingsOptionRow {
                Layout.fillWidth: true
                title: Theme.tr("privacyMaskPhone")
                value: root.privacyEnabled("phone")
                onToggled: function(on) { root.setPrivacy("phone", on) }
            }
            SettingsOptionRow {
                Layout.fillWidth: true
                title: Theme.tr("privacyMaskAddress")
                value: root.privacyEnabled("address")
                onToggled: function(on) { root.setPrivacy("address", on) }
            }
            SettingsOptionRow {
                Layout.fillWidth: true
                title: Theme.tr("privacyMaskRecordIds")
                value: root.privacyEnabled("recordId")
                onToggled: function(on) { root.setPrivacy("recordId", on) }
            }
            SettingsOptionRow {
                Layout.fillWidth: true
                title: Theme.tr("privacyMaskBank")
                value: root.privacyEnabled("bank")
                onToggled: function(on) { root.setPrivacy("bank", on) }
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

    RedeemCodeDialog {
        id: redeemDialog
        anchors.fill: parent
    }
}
