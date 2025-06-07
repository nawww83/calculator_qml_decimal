import QtQuick
import QtQuick.Window
import QtQml
import QtQuick.Layouts
import QtQuick.Controls.Basic

import operation.enums 1.0 as Operations

import "global_vars.js" as Global

Window {
    width: 675
    height: 295
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

        function onSetEnableFactorButton(val) {
            factor.enabled = val
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

    function setToMemory(val){
        Global.memory_cell = val !== "" ? val : Global.memory_cell
        memory.text = " Memory: " + Global.memory_cell
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
                placeholderText: qsTr("Количество знаков после запятой, max " + Global.maxDecimalWidth)
                validator: RegularExpressionValidator { regularExpression: /(\d{2})/ }
                Keys.onReturnPressed: dialog.accept()
            }
        }

        onAccepted: { AppCore.change_decimal_width(newDecimalWidthInput.text, Global.maxDecimalWidth) }
        onOpened: newDecimalWidthInput.focus = true
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 1

        TextField {
            id: input
            text: ""
            font.pointSize: 16
            Layout.fillWidth: true
            maximumLength: 80
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

        Label {
            id: memory
            text: " Memory:"
            font.pixelSize: 12
        }

        Pane {
            Layout.fillWidth: true
            padding: 6
            implicitHeight: 50
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
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    background: Rectangle {
                       opacity: enabled ? 1 : 0.3
                       border.color: add.down ? "#ff0000" : (add.hovered ? "#0000ff" : "#00ff00")
                       border.width: 1
                       radius: 5
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
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    background: Rectangle {
                       opacity: enabled ? 1 : 0.3
                       border.color: sub.down ? "#ff0000" : (sub.hovered ? "#0000ff" : "#00ff00")
                       border.width: 1
                       radius: 5
                    }

                    MouseArea {
                        anchors.fill: parent
                        onClicked: AppCore.process(Operations.OperationEnums.SUB, input.text)
                    }
                }

                Button {
                    id: mul
                    text: "\u00d7"
                    font.pixelSize: 28
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    background: Rectangle {
                       opacity: enabled ? 1 : 0.3
                       border.color: mul.down ? "#ff0000" : (mul.hovered ? "#0000ff" : "#00ff00")
                       border.width: 1
                       radius: 5
                    }

                    MouseArea {
                        anchors.fill: parent
                        onClicked: AppCore.process(Operations.OperationEnums.MULT, input.text)
                    }
                }
                Button {
                    id: div
                    text: "\u00f7"
                    font.pixelSize: 28
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    background: Rectangle {
                       opacity: enabled ? 1 : 0.3
                       border.color: div.down ? "#ff0000" : (div.hovered ? "#0000ff" : "#00ff00")
                       border.width: 1
                       radius: 5
                    }

                    MouseArea {
                        anchors.fill: parent
                        onClicked: AppCore.process(Operations.OperationEnums.DIV, input.text)
                    }
                }
                Button {
                    id: equal
                    text: "="
                    font.pixelSize: 28
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    background: Rectangle {
                       opacity: enabled ? 1 : 0.3
                       border.color: equal.down ? "#ff0000" : (equal.hovered ? "#0000ff" : "#00ff00")
                       border.width: 1
                       radius: 5
                    }

                    MouseArea {
                        anchors.fill: parent
                        onClicked: AppCore.process(Operations.OperationEnums.EQUAL, input.text)
                    }
                }
                Button {
                    id: clear
                    text: "C"
                    font.pixelSize: 24
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Сброс / Остановка расчета")

                    background: Rectangle {
                       opacity: enabled ? 1 : 0.3
                       border.color: clear.down ? "#ff0000" : (clear.hovered ? "#0000ff" : "#00ff00")
                       border.width: 1
                       radius: 5
                    }

                    MouseArea {
                        anchors.fill: parent
                        onClicked: AppCore.process(Operations.OperationEnums.CLEAR_ALL);
                    }
                }
            } // RowLayout
        } // Pane

        Pane {
            Layout.fillWidth: true
            padding: 6
            implicitHeight: 50
            background: Rectangle {
                color: "transparent"
                border.color: "green"
                radius: 3
            }
            RowLayout {
                anchors.fill: parent
                Button {
                    id: sqrt
                    text: "\u221ax"
                    font.pixelSize: 24
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Корень квадратный из модуля числа")

                    background: Rectangle {
                       opacity: enabled ? 1 : 0.3
                       border.color: sqrt.down ? "#ff0000" : (sqrt.hovered ? "#0000ff" : "#00ff00")
                       border.width: 1
                       radius: 5
                    }

                    MouseArea {
                        anchors.fill: parent
                        onClicked: AppCore.process(Operations.OperationEnums.SQRT, input.text);
                    }
                }
                Button {
                    id: sqr
                    text: "x²"
                    font.pixelSize: 24
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Квадрат числа")

                    background: Rectangle {
                       opacity: enabled ? 1 : 0.3
                       border.color: sqr.down ? "#ff0000" : (sqr.hovered ? "#0000ff" : "#00ff00")
                       border.width: 1
                       radius: 5
                    }

                    MouseArea {
                        anchors.fill: parent
                        onClicked: AppCore.process(Operations.OperationEnums.SQR, input.text);
                    }
                }
                Button {
                    id: reciprocal
                    text: "1/x"
                    font.pixelSize: 24
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Обратное число")

                    background: Rectangle {
                       opacity: enabled ? 1 : 0.3
                       border.color: reciprocal.down ? "#ff0000" : (reciprocal.hovered ? "#0000ff" : "#00ff00")
                       border.width: 1
                       radius: 5
                    }

                    MouseArea {
                        anchors.fill: parent
                        onClicked: AppCore.process(Operations.OperationEnums.RECIPROC, input.text);
                    }
                }
                Button {
                    id: max_integer
                    text: "Max Int"
                    font.pixelSize: 24
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Наибольшее целое число")

                    background: Rectangle {
                       opacity: enabled ? 1 : 0.3
                       border.color: max_integer.down ? "#ff0000" : (max_integer.hovered ? "#0000ff" : "#00ff00")
                       border.width: 1
                       radius: 5
                    }

                    MouseArea {
                        anchors.fill: parent
                        onClicked: AppCore.process(Operations.OperationEnums.MAX_INT_VALUE);
                    }
                }
                Button {
                    id: random_integer
                    text: "RND"
                    font.pixelSize: 24
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Случайное целое 128-битное число")

                    background: Rectangle {
                       opacity: enabled ? 1 : 0.3
                       border.color: random_integer.down ? "#ff0000" : (random_integer.hovered ? "#0000ff" : "#00ff00")
                       border.width: 1
                       radius: 5
                    }

                    MouseArea {
                        anchors.fill: parent
                        onClicked: AppCore.process(Operations.OperationEnums.RANDINT);
                    }
                }
                Button {
                    id: random_half_integer
                    text: "RND64"
                    font.pixelSize: 24
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Случайное целое 64-битное число")

                    background: Rectangle {
                       opacity: enabled ? 1 : 0.3
                       border.color: random_half_integer.down ? "#ff0000" : (random_half_integer.hovered ? "#0000ff" : "#00ff00")
                       border.width: 1
                       radius: 5
                    }

                    MouseArea {
                        anchors.fill: parent
                        onClicked: AppCore.process(Operations.OperationEnums.RANDINT64);
                    }
                }
                Button {
                    id: factor
                    text: "Factor"
                    font.pixelSize: 24
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Разложить на простые множители целую часть числа")

                    background: Rectangle {
                       opacity: enabled ? 1 : 0.3
                       border.color: factor.down ? "#ff0000" : (factor.hovered ? "#0000ff" : "#00ff00")
                       border.width: 1
                       radius: 5
                    }

                    MouseArea {
                        anchors.fill: parent
                        onClicked: AppCore.process(Operations.OperationEnums.FACTOR, input.text);
                    }
                }
                Button {
                    id: to_memory
                    text: "MS"
                    font.pixelSize: 24
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Положить в память")

                    background: Rectangle {
                       opacity: enabled ? 1 : 0.3
                       border.color: to_memory.down ? "#ff0000" : (to_memory.hovered ? "#0000ff" : "#00ff00")
                       border.width: 1
                       radius: 5
                    }

                    MouseArea {
                        anchors.fill: parent
                        onClicked: setToMemory(input.text)
                    }
                }
                Button {
                    id: from_memory
                    text: "MR"
                    font.pixelSize: 24
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Извлечь из памяти")

                    background: Rectangle {
                       opacity: enabled ? 1 : 0.3
                       border.color: from_memory.down ? "#ff0000" : (from_memory.hovered ? "#0000ff" : "#00ff00")
                       border.width: 1
                       radius: 5
                    }

                    MouseArea {
                        anchors.fill: parent
                        onClicked: input.text = Global.memory_cell !== "" ? Global.memory_cell : input.text
                    }
                }
            } // RowLayout
        } // Pane

        TextArea {
            id: help
            font.pixelSize: 13
            textFormat: Text.RichText
            text: "Смена знака числа: нажмите underscore <b>&#95;</b><br>
                    Cброс: <b>Esc</b><br>
                    Смена количества знаков после запятой: <b>Ctrl+S</b><br>
                    Двухоперандные операции не имеют приоритета.
                    Работает правило округления всех девяток.
                    <br>
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
