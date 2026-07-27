import QtQuick
import ProjectO

Item {
    id: logoRoot
    property int logoSize: 32
    property int cornerRadius: 8
    width: logoSize
    height: logoSize

    readonly property int sourcePx: Math.min(256, Math.max(64, logoSize * 3))

    Image {
        id: logoImage
        anchors.fill: parent
        source: "qrc:/qt/qml/ProjectO/resources/app-icon.png"
        sourceSize: Qt.size(logoRoot.sourcePx, logoRoot.sourcePx)
        fillMode: Image.PreserveAspectFit
        smooth: true
        mipmap: true
        asynchronous: true
    }
}
