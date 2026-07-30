pragma Singleton
import QtQuick

QtObject {
    readonly property int languageRevision: AppSettings.languageRevision
    readonly property int _themeRev: AppSettings.themeRevision
    readonly property bool dark: AppSettings.isDark
    readonly property bool maskMode: AppSettings.maskMode

    function tr(key) {
        const _ = AppSettings.languageRevision
        return AppSettings.trKey(key)
    }

    readonly property string headerName: {
        const _ = AppSettings.languageRevision
        return tr("appName")
    }

    readonly property color bg: dark ? "#141818" : "#F3F7F6"
    readonly property color surface: dark ? "#1E2625" : "#FFFFFF"
    readonly property color surfaceAlt: dark ? "#252D2C" : "#F7FBFA"
    readonly property color text: dark ? "#E8F5F2" : "#134E4A"
    readonly property color textSecondary: dark ? "#9DB5B1" : "#5F7A76"
    readonly property color textBody: dark ? "#C5D9D5" : "#3F5E5A"
    readonly property color iconDefault: {
        const _ = AppSettings.themeRevision
        return dark ? "#FFFFFF" : "#000000"
    }
    readonly property color iconHover: {
        const _ = AppSettings.themeRevision
        return dark ? "#FFFFFF" : "#000000"
    }
    readonly property color accent: dark ? "#14B8A6" : "#0F766E"
    readonly property color accentLight: dark ? "#2DD4BF" : "#14B8A6"
    readonly property color accentSoft: dark ? "#0F766E33" : "#0F766E14"
    readonly property color bgAccent: dark ? "#0F766E18" : "#0F766E0D"
    readonly property color border: dark ? "#343E3D" : "#D7E5E2"
    readonly property color tabInactive: dark ? "#343E3D" : "#E5F2EF"
    readonly property color menuHover: dark ? "#343E3D" : "#E5F2EF"
    readonly property color menuUnselectedText: text
    readonly property color dimOverlay: "#000000"
    readonly property real dimOpacity: dark ? 0.45 : 0.30
    readonly property color shadowColor: "#000000"
    readonly property real shadowOpacity1: dark ? 0.35 : 0.10
    readonly property real shadowOpacity2: dark ? 0.18 : 0.05
    readonly property int shadowOffset1: 4
    readonly property int shadowOffset2: 12
    readonly property color success: dark ? "#3ECF9A" : "#34C759"
    readonly property color danger: "#FF3B30"
    readonly property color maskAuto: "#F59E0B"
    readonly property color maskAutoFill: "transparent"
    readonly property color maskManual: dark ? "#14B8A6" : "#0F766E"
    readonly property color maskManualFill: dark ? "#0F766E66" : "#0F766E77"
    readonly property color maskManualDraft: dark ? "#0F766E55" : "#0F766E55"
    readonly property color maskAccent: accent

    readonly property int compactControlWidth: 96
    readonly property real watermarkFontHeightRatio: 0.048
    readonly property real watermarkOpacity: 0.22
    readonly property real watermarkAngle: -35
    readonly property color watermarkDefaultColor: "#5A5A5A"

    readonly property int animFast: 140
    readonly property int animNormal: 220
    readonly property int animSlow: 320
    readonly property int radiusSm: 8
    readonly property int radiusMd: 12
    readonly property int radiusLg: 16
    readonly property int headerHeight: 64

    readonly property string uiFontFamily: "Microsoft YaHei"
    readonly property font mainFont: Qt.font({ family: uiFontFamily, pixelSize: 15 })
    readonly property font mainFontBold: Qt.font({ family: uiFontFamily, pixelSize: 15, weight: Font.DemiBold })
    readonly property font titleFont: Qt.font({ family: uiFontFamily, pixelSize: 22, weight: Font.DemiBold })
    readonly property font brandTitleFont: Qt.font({ family: uiFontFamily, pixelSize: 22, weight: Font.DemiBold })
    readonly property font tabFont: Qt.font({ family: uiFontFamily, pixelSize: 14, weight: Font.Medium })
    readonly property font captionFont: Qt.font({ family: uiFontFamily, pixelSize: 13 })
    readonly property font captionBoldFont: Qt.font({ family: uiFontFamily, pixelSize: 13, weight: Font.DemiBold })
}
