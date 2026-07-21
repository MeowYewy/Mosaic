import QtQuick
import QtQuick.Effects
import ProjectO

Item {
    id: root

    property string icon: ""
    property string name: ""
    property color color: Theme.iconDefault
    property alias strokeColor: root.color
    property int iconSize: 18
    property real iconOpacity: 1

    implicitWidth: iconSize
    implicitHeight: iconSize

    readonly property string resolvedIcon: {
        if (icon.length > 0)
            return icon
        if (name.length > 0)
            return name
        return "file"
    }

    readonly property string iconFileName: {
        switch (resolvedIcon) {
        case "fit-view":
        case "expand": return "shrink.svg"
        case "draw": return "square-dashed-mouse-pointer.svg"
        case "select": return "mouse-pointer-2.svg"
        case "fixed": return "square-mouse-pointer.svg"
        case "delete": return "trash-2.svg"
        case "style-block": return "square-filled.svg"
        case "style-pixel": return "grid-3x3.svg"
        case "mask-eye": return "eye.svg"
        default: return resolvedIcon + ".svg"
        }
    }

    readonly property url iconSource:
        "qrc:/qt/qml/ProjectO/resources/icons/" + iconFileName

    Image {
        anchors.fill: parent
        source: root.iconSource
        fillMode: Image.PreserveAspectFit
        sourceSize.width: Math.max(48, Math.round(width * 3))
        sourceSize.height: Math.max(48, Math.round(height * 3))
        asynchronous: true
        smooth: true
        cache: true
        opacity: root.iconOpacity
        layer.enabled: true
        layer.smooth: true
        layer.effect: MultiEffect {
            colorization: 1.0
            colorizationColor: root.color
        }
    }
}
