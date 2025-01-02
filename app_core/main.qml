import QtQuick
import QtQuick.Window
import QtQml
import QtQuick.Layouts
import QtQuick.Controls.Basic

import operation.enums 1.0 as Operations

import "global_vars.js" as Global

Window {
    width: 640
    height: 245
    maximumHeight: height
    maximumWidth: width
    minimumHeight: height
    minimumWidth: width

    visible: true
    title: qsTr("Decimal калькулятор 128-bit, version " + Global.APP_VERSION)

    function regex_1() {
        var pattern = "^[-]?([1-9]\\d{0,2}(\\s?\\d{0,3})*|0)([.,]\\d{0,w_original})?$"
        if (Global.decimalWidth < 1) {
            pattern = "^[-]?([1-9]\\d{0,2}(\\s?\\d{0,3})*|0)?$"
        } else {
            pattern = pattern.replace("w_original", Global.decimalWidth)
        }
        return pattern
        // Global.decimalWidth - требуемое количество цифр после запятой.
    }

    property var decimalRegEx: RegExp(regex_1());

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

        function onChangeDecimalWidth(width) {
            Global.decimalWidth = width
            decimalRegEx = RegExp(regex_1());
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

    Dialog {
        id: dialog
        title: qsTr("Настройки")
        font.pointSize: 12
        standardButtons: Dialog.Ok | Dialog.Cancel
        width: parent.width * 1.

        ColumnLayout {
            anchors.fill: parent
            TextField {
                id: newDecimalWidthInput
                font.pointSize: 13
                Layout.fillWidth: true
                placeholderText: qsTr("Количество знаков после запятой")
                validator: RegularExpressionValidator { regularExpression: /(\d)/ }
                Keys.onReturnPressed: dialog.accept()
            }
        }

        onAccepted: { AppCore.change_decimal_width(newDecimalWidthInput.text) }
        onOpened: newDecimalWidthInput.focus = true
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 3

        TextField {
            id: input
            text: ""
            font.pointSize: 16
            Layout.fillWidth: true
            maximumLength: 64
            validator: RegularExpressionValidator { regularExpression: decimalRegEx }

            focus: true
            visible: true

            property bool ctrlPressed: false

            Keys.onPressed: event => {
                if (event.key === Qt.Key_Control) {
                    ctrlPressed = true
                }
                if (event.key === Qt.Key_Plus) {
                    AppCore.process(Operations.OperationEnums.ADD, input.text);
                    event.accepted = true;
                }
                if (event.key === Qt.Key_Minus) {
                    AppCore.process(Operations.OperationEnums.SUB, input.text);
                    event.accepted = true;
                }
                if (event.key === Qt.Key_Asterisk) {
                    AppCore.process(Operations.OperationEnums.MULT, input.text);
                    event.accepted = true;
                }
                if (event.key === Qt.Key_Slash) {
                    AppCore.process(Operations.OperationEnums.DIV, input.text);
                    event.accepted = true;
                }
                if (event.key === Qt.Key_Underscore) {
                    if (input.text !== "") {
                        AppCore.process(Operations.OperationEnums.NEGATION, input.text);
                        event.accepted = true;
                    }
                }
                if ((event.key === Qt.Key_Equal) || (event.key === Qt.Key_Return)) {
                    AppCore.process(Operations.OperationEnums.EQUAL, input.text);
                    event.accepted = true;
                }
                if (event.key === Qt.Key_Escape) {
                    AppCore.process(Operations.OperationEnums.CLEAR_ALL, input.text);
                    event.accepted = true;
                }
                if (event.key === Qt.Key_S && ctrlPressed) {
                    ctrlPressed = false
                    dialog.open()
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
                if (event.key === Qt.Key_Control) {
                    ctrlPressed = false
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
                        onClicked: AppCore.process(Operations.OperationEnums.ADD, input.text)
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
                        onClicked: AppCore.process(Operations.OperationEnums.SUB, input.text)
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
                        onClicked: AppCore.process(Operations.OperationEnums.MULT, input.text)
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
                        onClicked: AppCore.process(Operations.OperationEnums.DIV, input.text)
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
                        onClicked: AppCore.process(Operations.OperationEnums.EQUAL, input.text)
                    }
                }

                Button {
                    id: clear
                    text: "Clear"
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
                        onClicked: AppCore.process(Operations.OperationEnums.CLEAR_ALL);
                    }
                }
                Button {
                    id: max_integer
                    text: "Max Int"
                    bottomPadding: 10
                    font.pixelSize: 24
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    background: Rectangle {
                       opacity: enabled ? 1 : 0.3
                       border.color: max_integer.down ? "#ff0000" : (max_integer.hovered ? "#0000ff" : "#00ff00")
                       border.width: 1
                       radius: 2
                    }

                    MouseArea {
                        anchors.fill: parent
                        onClicked: AppCore.process(Operations.OperationEnums.MAX_INT_VALUE);
                    }
                }
            }
        }

        TextArea {
            id: help
            font.pixelSize: 13
            textFormat: Text.RichText
            text: "Смена знака числа: нажмите underscore <b>&#95;</b><br>
                    Cброс: <b>Esc</b><br>
                    Смена количества знаков после запятой: <b>Ctrl+S</b><br>
                    Вводимые операции не имеют приоритета.<br>
                    <a href=\"https://github.com/nawww83/calculator_qml_decimal\">See github: nawww83</a>"
            onLinkActivated: link => Qt.openUrlExternally(link)
            MouseArea {
                anchors.fill: parent
                acceptedButtons: Qt.NoButton
                cursorShape: parent.hoveredLink ? Qt.PointingHandCursor : Qt.ArrowCursor
            }
            readOnly: true
        }

        Item { Layout.fillHeight: true }
    }
}
