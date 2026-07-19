import QtQuick
import ProjectO

Item {
    id: root

    property string kind: "file"
    // Callers may override; PDF uses theme violet (slightly cooler than pink-leaning wash).
    property color tint: kind === "pdf" ? Theme.accent : Theme.textSecondary

    implicitWidth: 18
    implicitHeight: 18

    readonly property string lucideName: {
        switch (kind) {
        case "folder":
        case "place-home":
        case "place-desktop":
        case "place-documents":
        case "place-downloads":
            return "folder"
        case "drive":
        case "computer":
            return "hard-drive"
        case "pdf":
            return "pdf"
        case "image":
            return "file-image"
        case "md":
        case "office":
            return "file-text"
        case "txt":
        case "text":
            return "file-type"
        case "video":
            return "file-video"
        case "audio":
            return "file-volume"
        case "code":
            return "file-braces"
        default:
            return "file"
        }
    }

    LucideIcon {
        anchors.fill: parent
        icon: root.lucideName
        color: root.tint
    }
}
