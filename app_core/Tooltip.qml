import QtQuick 2.0

import QtQuick.Controls 2.12

ToolTip {
    background: Rectangle {border.color: "transparent"}
    visible: parent.hovered
    font.pointSize: 12
    timeout: 1700
    delay: 1200
}
