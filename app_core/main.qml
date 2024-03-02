import QtQuick 2.15
import QtQuick.Window 2.15

import QtQml 2.15

import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

import operation.enums 1.0


Window {
    minimumWidth: 330
    minimumHeight: 180
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

        function onClearTempResult() {
            temp_result.text = ""
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
            validator: RegExpValidator {
                // Remove validator to allow entering text that doesn't match the regex
                regExp: /^[-]?([1-9]\d{0,2}(\s?\d{3})*|0)([.,]\d{1,3})?$/  // [.,]\d{1,W} ===>> W digits after the comma
            }

            focus: true
            visible: true

            Keys.onPressed: {
                if (event.key === Qt.Key_Plus) {
                    AppCore.cppSlot(OperationEnums.ADD, input.text);
                    event.accepted = true;
                }
                if (event.key === Qt.Key_Minus) {
                    AppCore.cppSlot(OperationEnums.SUB, input.text);
                    event.accepted = true;
                }
                if (event.key === Qt.Key_Asterisk) {
                    AppCore.cppSlot(OperationEnums.MULT, input.text);
                    event.accepted = true;
                }
                if (event.key === Qt.Key_Slash) {
                    AppCore.cppSlot(OperationEnums.DIV, input.text);
                    event.accepted = true;
                }
                if (event.key === Qt.Key_Underscore) {
                    if (input.text != "") {
                        AppCore.cppSlot(OperationEnums.NEGATION, input.text);
                        event.accepted = true;
                    }
                }
                if ((event.key === Qt.Key_Equal) || (event.key === Qt.Key_Return)) {
                    AppCore.cppSlot(OperationEnums.EQUAL, input.text);
                    event.accepted = true;
                }
                if ((event.key === Qt.Key_Escape) || (event.key === Qt.Key_C)) {
                    AppCore.cppSlot(OperationEnums.CLEAR_ALL, input.text);
                    event.accepted = true;
                }
            }
            Keys.onReleased: {
                // first, replace '.' by ','
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
            }
        }

        Label {
            id: temp_result
            text: ""
            font.pixelSize: 12
        }

        Pane {
            Layout.fillWidth: true
            implicitHeight: 55
            background: Rectangle {
                color: "transparent"
                border.color: "green"
                radius: 2
            }
            RowLayout {
                anchors.fill: parent
                Button {
                    background: Rect{text: "\u002b"}
//                    hoverEnabled: true
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    MouseArea {
                        anchors.fill: parent
                        id: add
                        onClicked: AppCore.cppSlot(OperationEnums.ADD, input.text)
                    }
                }

                Button {
                    background: Rect{text: "\u2212"}
//                    hoverEnabled: true
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    MouseArea {
                        anchors.fill: parent
                        id: sub
                        onClicked: AppCore.cppSlot(OperationEnums.SUB, input.text)
                    }
                }

                Button {
                    background: Rect{text: "\u00d7"}
//                    hoverEnabled: true
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    MouseArea {
                        anchors.fill: parent
                        id: mul
                        onClicked: AppCore.cppSlot(OperationEnums.MULT, input.text)
                    }
                }

                Button {
                    background: Rect{text: "\u00f7"}
//                    hoverEnabled: true
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    MouseArea {
                        anchors.fill: parent
                        id: div
                        onClicked: AppCore.cppSlot(OperationEnums.DIV, input.text)
                    }
                }

                Button {
                    background: Rect{text: "\u003d"}
//                    hoverEnabled: true
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    MouseArea {
                        anchors.fill: parent
                        id: equal
                        onClicked: AppCore.cppSlot(OperationEnums.EQUAL, input.text)
                    }
                }

                Button {
                    background: Rect{text: "C"}
                    hoverEnabled: false
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    // Tooltip {text: qsTr("Сброс: Esc")}

                    MouseArea {
                        anchors.fill: parent
                        id: clear
                        onClicked: AppCore.cppSlot(OperationEnums.CLEAR_ALL);
                    }
                }
            }
        }

        TextArea {
            id: help
            font.pixelSize: 11
            text: "* Нажмите _ для смены знака числа\n" +
                  "* Вводимые операции не имеют приоритета\n" +
                  "* Нажмите Esc или C для сброса.\n"
            readOnly: true
        }

        Item { Layout.fillHeight: true }
    }
}
