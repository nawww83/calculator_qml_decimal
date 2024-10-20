import QtQuick 2.0

Rectangle {
    id: rect
    color: "transparent"
    property alias text: label.text
    border {color: "red"; width: 1}
    radius: 2
    anchors.fill: parent

    Text {
        id: label
        anchors.centerIn: parent
        font.pointSize: 18
    }
}
