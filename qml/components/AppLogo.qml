import QtQuick
import ProjectO

Item {
    property int logoSize: 32
    property int cornerRadius: 8
    width: logoSize
    height: logoSize

    Image {
        anchors.fill: parent
        source: "qrc:/qt/qml/ProjectO/resources/logo.svg"
        sourceSize.width: logoSize
        sourceSize.height: logoSize
        fillMode: Image.PreserveAspectFit
        asynchronous: true
    }
}
