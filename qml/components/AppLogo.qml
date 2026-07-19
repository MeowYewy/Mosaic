import QtQuick
import ProjectO

Item {
    property int logoSize: 32
    property int cornerRadius: 8
    width: logoSize
    height: logoSize

    Image {
        id: logoImage
        anchors.fill: parent
        source: "qrc:/qt/qml/ProjectO/resources/app-icon.png"
        sourceSize: Qt.size(logoSize * 2, logoSize * 2)
        fillMode: Image.PreserveAspectFit
        smooth: true
        mipmap: true
        asynchronous: true

        onStatusChanged: {
            if (status === Image.Error)
                logoImage.source = "qrc:/qt/qml/ProjectO/resources/logo.svg"
        }
    }
}
