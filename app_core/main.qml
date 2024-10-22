import QtQuick
import QtQuick.Window
import QtQml
import QtQuick.Layouts
import QtQuick.Controls.Basic

import operation.enums

Window {
    width: 400
    height: 220
    maximumHeight: height
    maximumWidth: width
    minimumHeight: height
    minimumWidth: width

    visible: true
    title: qsTr("Decimal калькулятор 64-bit")

    Connections {
        target: AppCore

        function onClearInputField() {
            input.text = ""
        }

        function onSetInput(val) {
            input.text = thousandSeparator(val)
        }

        function onShowTempResult(val, is_number) {
            if (is_number) {
                temp_result.color = "black";
                temp_result.text = " " + thousandSeparator(val)
            } else {
                temp_result.color = "red";
                temp_result.text = " " + val
            }
        }

        function onShowCurrentOperation(operation) {
            current_operation.text = " " + operation
        }

        function onClearTempResult() {
            temp_result.text = ""
        }

        function onClearCurrentOperation() {
            current_operation.text = ""
        }
    }

    function thousandSeparator(inp){
        inp = inp.replace(/\s/g, "")
        var idx = inp.indexOf(",")
        if (idx === -1)
            return inp.replace(/(\d)(?=(\d{3})+(?!\d))/g, "$1 ")
        else
            return inp.substring(0, idx).replace(/(\d)(?=(\d{3})+(?!\d))/g, "$1 ") + inp.substring(idx)
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 3

        TextField {
            id: input
            text: ""
            font.pointSize: 14
            Layout.fillWidth: true
            maximumLength: 30
            validator: RegularExpressionValidator {
                regularExpression: /^[-]?([1-9]\d{0,2}(\s?\d{0,3})*|0)([.,]\d{1,3})?$/
                // [.,]\d{1,w} ===>> w - требуемое количество цифр после запятой.
            }

            focus: true
            visible: true

            Keys.onPressed: event => {
                if (event.key === Qt.Key_Plus) {
                    AppCore.process(OperationEnums.ADD, input.text);
                    event.accepted = true;
                }
                if (event.key === Qt.Key_Minus) {
                    AppCore.process(OperationEnums.SUB, input.text);
                    event.accepted = true;
                }
                if (event.key === Qt.Key_Asterisk) {
                    AppCore.process(OperationEnums.MULT, input.text);
                    event.accepted = true;
                }
                if (event.key === Qt.Key_Slash) {
                    AppCore.process(OperationEnums.DIV, input.text);
                    event.accepted = true;
                }
                if (event.key === Qt.Key_Underscore) {
                    if (input.text !== "") {
                        AppCore.process(OperationEnums.NEGATION, input.text);
                        event.accepted = true;
                    }
                }
                if ((event.key === Qt.Key_Equal) || (event.key === Qt.Key_Return)) {
                    AppCore.process(OperationEnums.EQUAL, input.text);
                    event.accepted = true;
                }
                if (event.key === Qt.Key_Escape) {
                    AppCore.process(OperationEnums.CLEAR_ALL, input.text);
                    event.accepted = true;
                }
            }
            Keys.onReleased: event => {
                var previous_pos = cursorPosition
                var previous_length = length
                // Сначала заменить '.' на ',' если есть.
                if (event.key === Qt.Key_Period) {
                    if (input.text.indexOf(',') >= 0) {
                        event.accepted = false;
                    }
                    else {
                        input.text = input.text.replace(/\./g, ",");
                        event.accepted = true;
                    }
                }
                input.text = thousandSeparator(input.text)
                // Коррекция позиции курсора.
                if (cursorPosition === length && previous_pos !== previous_length) {
                    var shift = previous_length - length
                    cursorPosition = previous_pos - shift
                }
            }
        }

        Label {
            id: temp_result
            text: ""
            font.pixelSize: 12
        }

        Label {
            id: current_operation
            text: ""
            font.pixelSize: 12
        }

        Pane {
            Layout.fillWidth: true
            implicitHeight: 65
            background: Rectangle {
                color: "transparent"
                border.color: "green"
                radius: 3
            }
            RowLayout {
                anchors.fill: parent

                Button {
                    id: add
                    text: "\u002b"
                    font.pixelSize: 28
                    bottomPadding: 10
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    background: Rectangle {
                       opacity: enabled ? 1 : 0.3
                       border.color: add.down ? "#ff0000" : (add.hovered ? "#0000ff" : "#00ff00")
                       border.width: 1
                       radius: 2
                    }

                    MouseArea {
                        anchors.fill: parent
                        onClicked: AppCore.process(OperationEnums.ADD, input.text)
                    }
                }

                Button {
                    id: sub
                    text: "\u2212"
                    font.pixelSize: 28
                    bottomPadding: 10
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    background: Rectangle {
                       opacity: enabled ? 1 : 0.3
                       border.color: sub.down ? "#ff0000" : (sub.hovered ? "#0000ff" : "#00ff00")
                       border.width: 1
                       radius: 2
                    }

                    MouseArea {
                        anchors.fill: parent
                        onClicked: AppCore.process(OperationEnums.SUB, input.text)
                    }
                }

                Button {
                    id: mul
                    text: "\u00d7"
                    bottomPadding: 10
                    font.pixelSize: 28
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    background: Rectangle {
                       opacity: enabled ? 1 : 0.3
                       border.color: mul.down ? "#ff0000" : (mul.hovered ? "#0000ff" : "#00ff00")
                       border.width: 1
                       radius: 2
                    }

                    MouseArea {
                        anchors.fill: parent
                        onClicked: AppCore.process(OperationEnums.MULT, input.text)
                    }
                }

                Button {
                    id: div
                    text: "\u00f7"
                    bottomPadding: 10
                    font.pixelSize: 28
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    background: Rectangle {
                       opacity: enabled ? 1 : 0.3
                       border.color: div.down ? "#ff0000" : (div.hovered ? "#0000ff" : "#00ff00")
                       border.width: 1
                       radius: 2
                    }

                    MouseArea {
                        anchors.fill: parent
                        onClicked: AppCore.process(OperationEnums.DIV, input.text)
                    }
                }

                Button {
                    id: equal
                    text: "\u003d"
                    bottomPadding: 10
                    font.pixelSize: 28
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    background: Rectangle {
                       opacity: enabled ? 1 : 0.3
                       border.color: equal.down ? "#ff0000" : (equal.hovered ? "#0000ff" : "#00ff00")
                       border.width: 1
                       radius: 2
                    }

                    MouseArea {
                        anchors.fill: parent
                        onClicked: AppCore.process(OperationEnums.EQUAL, input.text)
                    }
                }

                Button {
                    id: clear
                    text: "C"
                    bottomPadding: 10
                    font.pixelSize: 24
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    background: Rectangle {
                       opacity: enabled ? 1 : 0.3
                       border.color: clear.down ? "#ff0000" : (clear.hovered ? "#0000ff" : "#00ff00")
                       border.width: 1
                       radius: 2
                    }

                    MouseArea {
                        anchors.fill: parent
                        onClicked: AppCore.process(OperationEnums.CLEAR_ALL);
                    }
                }
            }
        }

        TextArea {
            id: help
            font.pixelSize: 13
            text: "* Нажмите _ для смены знака числа\n" +
                  "* Вводимые операции не имеют приоритета\n" +
                  "* Нажмите Esc для сброса."
            readOnly: true
        }

        Item { Layout.fillHeight: true }
    }
}
