import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Window 2.15
import QtQml 2.15
import QtQuick.Layouts 1.15

import AppCoreBackend 1.0
import operation.enums 1.0 as Operations

import "global_vars.js" as Global

Window {
    id: root
    width: 730

    // Высота окна в свёрнутом состоянии и высота выдвижных панелей.
    readonly property int collapsedHeight: 320
    readonly property int keypadHeight: 190
    readonly property int rareOpsHeight: 50
    readonly property int historyHeight: 170

    // Клавиатура и редкие операции выдвигаются: окно растёт вместе с ними.
    property bool keypadOpen: false
    property real keypadReveal: keypadOpen ? keypadHeight : 0

    property bool rareOpsOpen: false
    property real rareOpsReveal: rareOpsOpen ? rareOpsHeight : 0

    // История операций: выдвижная панель внизу окна, под экранной клавиатурой.
    property bool historyOpen: false
    property real historyReveal: historyOpen ? historyHeight : 0

    Behavior on keypadReveal {
        NumberAnimation { duration: 200; easing.type: Easing.OutCubic }
    }

    Behavior on rareOpsReveal {
        NumberAnimation { duration: 200; easing.type: Easing.OutCubic }
    }

    Behavior on historyReveal {
        NumberAnimation { duration: 200; easing.type: Easing.OutCubic }
    }

    height: collapsedHeight + Math.round(rareOpsReveal) + Math.round(keypadReveal)
            + Math.round(historyReveal)
    maximumHeight: collapsedHeight
                   + ((keypadOpen || keypadReveal > 0) ? keypadHeight : 0)
                   + ((rareOpsOpen || rareOpsReveal > 0) ? rareOpsHeight : 0)
                   + ((historyOpen || historyReveal > 0) ? historyHeight : 0)
    maximumWidth: width
    minimumHeight: collapsedHeight
    minimumWidth: width

    visible: true
    title: qsTr("Decimal калькулятор 128-bit, version " + Global.APP_VERSION)

    // Привязываемся напрямую к свойству C++
    property int userWidth: AppCore.userDisplayWidth

    // Реактивное свойство: пересчитывается АВТОМАТИЧЕСКИ при изменении userWidth
    readonly property var decimalRegEx: {
        var pattern = "^[-]?([1-9]\\d{0,2}(\\s?\\d{0,3})*|0)([.,]\\d{0," + userWidth + "})?$"
        if (userWidth < 1) {
            pattern = "^[-]?([1-9]\\d{0,2}(\\s?\\d{0,3})*|0)?$"
        }
        return new RegExp(pattern);
    }

    Connections {
        target: AppCore

        function onClearInputField() {
            input.text = ""
        }

        function onSetInput(val) {
            input.text = thousandSeparator(val)
            input.cursorPosition = input.text.length
        }

        function onSetEnableFactorButton(val) {
            factor.enabled = val
        }

        function onShowTempResult(val, is_number) {
            temp_result.isError = !is_number; // Выставляем флаг
            if (is_number) {
                temp_result.text = " " + thousandSeparator(val);
            } else {
                temp_result.text = " " + val;
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

    function setToMemory(val){
        Global.memory_cell = val !== "" ? val : Global.memory_cell
        memory.text = " Memory: " + Global.memory_cell
    }

    // Цвет строки истории операций по её типу.
    function historyColor(kind) {
        switch (kind) {
        case "request":  return "#1a7f37" // зелёный: исходящий запрос
        case "response": return "#0b57d0" // синий: ответ
        case "error":    return "#c62828" // красный: ошибка
        case "warning":  return "#b26a00" // оранжевый: предупреждение
        default:         return "#666666" // серый: информация
        }
    }

    // --- Ввод с выдвижной экранной клавиатуры ---------------------------

    function appendCharacter(ch) {
        var text = input.text
        var pos = (input.activeFocus && input.cursorPosition >= 0)
                ? input.cursorPosition : text.length

        if (ch >= "0" && ch <= "9") {
            // Не даём набрать "05": одиночный ноль заменяем цифрой.
            if (text === "0") {
                text = ""
                pos = 0
            } else if (text === "-0") {
                text = "-"
                pos = 1
            }
        }

        var candidate = text.substring(0, pos) + ch + text.substring(pos)
        if (!decimalRegEx.test(candidate))
            return

        setInputText(candidate, pos + 1)
    }

    function appendDecimalSeparator() {
        var text = input.text
        if (text.indexOf(",") >= 0 || text.indexOf(".") >= 0)
            return

        var pos = (input.activeFocus && input.cursorPosition >= 0)
                ? input.cursorPosition : text.length

        // Разделитель без целой части: подставляем ведущий ноль.
        if (text === "") {
            text = "0"
            pos = 1
        } else if (text.charAt(pos - 1) === "-") {
            text = text.substring(0, pos) + "0" + text.substring(pos)
            pos += 1
        }

        var candidate = text.substring(0, pos) + "," + text.substring(pos)
        if (!decimalRegEx.test(candidate))
            return

        setInputText(candidate, pos + 1)
    }

    function removeLastCharacter() {
        var text = input.text
        var selectionStart = input.selectionStart
        var selectionEnd = input.selectionEnd

        // Если выделен фрагмент — удаляем его.
        if (input.activeFocus && selectionStart !== selectionEnd) {
            setInputText(text.substring(0, selectionStart) + text.substring(selectionEnd),
                         selectionStart)
            return
        }

        var pos = (input.activeFocus && input.cursorPosition >= 0)
                ? input.cursorPosition : text.length
        if (pos <= 0)
            return

        var candidate = text.substring(0, pos - 1) + text.substring(pos)
        // После удаления слева от разделителя не должно остаться пустой целой части.
        if (candidate.charAt(0) === ",")
            candidate = "0" + candidate
        else if (candidate.substring(0, 2) === "-,")
            candidate = "-0" + candidate.substring(1)
        candidate = candidate.replace(/^(-?)0+(?=\d)/, "$1")

        setInputText(candidate, pos - 1)
    }

    // Записывает текст, расставляет разделители тысяч и сохраняет позицию курсора.
    function setInputText(value, caret) {
        var symbolsBeforeCaret = value.substring(0, caret).replace(/\s/g, "").length
        var formatted = thousandSeparator(value)
        input.text = formatted
        input.cursorPosition = positionAfterSymbols(formatted, symbolsBeforeCaret)
    }

    function positionAfterSymbols(value, count) {
        var seen = 0
        for (var i = 0; i < value.length; ++i) {
            if (value.charAt(i) !== " ")
                ++seen
            if (seen === count)
                return i + 1
        }
        return value.length
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
                validator: RegularExpressionValidator { regularExpression: /^(1[0-8]|[0-9])$/ }
                Keys.onReturnPressed: dialog.accept()
            }
        }

        onAccepted: { AppCore.change_decimal_width(parseInt(newDecimalWidthInput.text, 10)) }
        onOpened: {
            newDecimalWidthInput.text = root.userWidth
            newDecimalWidthInput.focus = true
        }
    }

    ColumnLayout {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: root.collapsedHeight + Math.round(root.rareOpsReveal)
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
                if (event.key === Qt.Key_AsciiCircum) {
                    AppCore.process(Operations.OperationEnums.POW, input.text);
                    event.accepted = true;
                }
                if (event.key === Qt.Key_Underscore) {
                    if (input.text !== "") {
                        AppCore.process(Operations.OperationEnums.NEGATION, input.text);
                        event.accepted = true;
                    }
                }
                if ((event.key === Qt.Key_Equal) || (event.key === Qt.Key_Return) || (event.key === Qt.Key_Enter)) {
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

            // Создаем внутреннее свойство для отслеживания ошибок
            property bool isError: false

            // Декларативно привязываем цвет через формулу.
            // Такая связь является реактивной и никогда не вызовет варнингов.
            color: isError ? "red" : "black"
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
            GridLayout {
                anchors.fill: parent
                columns: 8
                columnSpacing: 4
                rowSpacing: 4
                Button {
                    id: add
                    text: "\u002b"
                    font.pixelSize: 28
                    Layout.preferredWidth: 0
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
                    Layout.preferredWidth: 0
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
                    Layout.preferredWidth: 0
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
                    Layout.preferredWidth: 0
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
                    id: pow
                    text: "\u005e"
                    font.pixelSize: 28
                    Layout.preferredWidth: 0
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Возведение в степень")

                    background: Rectangle {
                       opacity: enabled ? 1 : 0.3
                       border.color: pow.down ? "#ff0000" : (pow.hovered ? "#0000ff" : "#00ff00")
                       border.width: 1
                       radius: 5
                    }

                    MouseArea {
                        anchors.fill: parent
                        onClicked: AppCore.process(Operations.OperationEnums.POW, input.text)
                    }
                }
                Button {
                    id: ln
                    text: "\u33d1"
                    font.pixelSize: 28
                    Layout.preferredWidth: 0
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Натуральный логарифм")

                    background: Rectangle {
                       opacity: enabled ? 1 : 0.3
                       border.color: ln.down ? "#ff0000" : (ln.hovered ? "#0000ff" : "#00ff00")
                       border.width: 1
                       radius: 5
                    }

                    MouseArea {
                        anchors.fill: parent
                        onClicked: AppCore.process(Operations.OperationEnums.LN, input.text)
                    }
                }
                Button {
                    id: exp
                    text: "\u212f\u02e3"
                    font.pixelSize: 28
                    Layout.preferredWidth: 0
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Экспонента")

                    background: Rectangle {
                       opacity: enabled ? 1 : 0.3
                       border.color: exp.down ? "#ff0000" : (exp.hovered ? "#0000ff" : "#00ff00")
                       border.width: 1
                       radius: 5
                    }

                    MouseArea {
                        anchors.fill: parent
                        onClicked: AppCore.process(Operations.OperationEnums.EXP, input.text)
                    }
                }
                Button {
                    id: clear
                    text: "C"
                    font.pixelSize: 24
                    Layout.preferredWidth: 0
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
            } // GridLayout
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
            GridLayout {
                anchors.fill: parent
                columns: 8
                columnSpacing: 4
                rowSpacing: 4
                Button {
                    id: sqrt
                    text: "\u221ax"
                    font.pixelSize: 24
                    Layout.preferredWidth: 0
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Квадратный корень")

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
                    Layout.preferredWidth: 0
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
                    Layout.preferredWidth: 0
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
                    id: round_btn
                    text: "[x]"
                    font.pixelSize: 20
                    Layout.preferredWidth: 0
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Округлить до ближайшего целого")

                    background: Rectangle {
                       opacity: enabled ? 1 : 0.3
                       border.color: round_btn.down ? "#ff0000" : (round_btn.hovered ? "#0000ff" : "#00ff00")
                       border.width: 1
                       radius: 5
                    }

                    MouseArea {
                        anchors.fill: parent
                        onClicked: AppCore.process(Operations.OperationEnums.ROUND, input.text);
                    }
                }
                Button {
                    id: fix_nines_btn
                    text: ".99\u2192"  // .99→
                    font.pixelSize: 20
                    Layout.preferredWidth: 0
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Схлопнуть машинный хвост девяток")

                    background: Rectangle {
                       opacity: enabled ? 1 : 0.3
                       border.color: fix_nines_btn.down ? "#ff0000" : (fix_nines_btn.hovered ? "#0000ff" : "#00ff00")
                       border.width: 1
                       radius: 5
                    }

                    MouseArea {
                        anchors.fill: parent
                        onClicked: AppCore.process(Operations.OperationEnums.FIX_NINES, input.text);
                    }
                }
                Button {
                    id: to_memory
                    text: "MS"
                    font.pixelSize: 24
                    Layout.preferredWidth: 0
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
                    Layout.preferredWidth: 0
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
                Button {
                    id: more_btn
                    text: "\u22ef"  // ⋯
                    font.pixelSize: 24
                    Layout.preferredWidth: 0
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    checkable: true
                    checked: root.rareOpsOpen
                    ToolTip.visible: hovered
                    ToolTip.text: root.rareOpsOpen ? qsTr("Скрыть редкие операции")
                                                    : qsTr("Показать редкие операции")

                    // Три точки рисуем сами: символ «⋯» в некоторых шрифтах
                    // получается слишком мелким и плохо заметен.
                    contentItem: Item {
                        Row {
                            anchors.centerIn: parent
                            spacing: 3

                            Repeater {
                                model: 3

                                Rectangle {
                                    width: 4
                                    height: 4
                                    radius: 2
                                    color: "black"
                                }
                            }
                        }
                    }

                    background: Rectangle {
                       opacity: enabled ? 1 : 0.3
                       color: more_btn.checked ? "#e8ffe8" : "transparent"
                       border.color: more_btn.down ? "#ff0000" : (more_btn.hovered ? "#0000ff" : "#00ff00")
                       border.width: 1
                       radius: 5
                    }

                    onCheckedChanged: root.rareOpsOpen = checked
                }
            } // GridLayout
        } // Pane

        // Выдвижная панель редких операций: открывается кнопкой «⋯».
        Item {
            id: rareOpsRow
            Layout.fillWidth: true
            Layout.preferredHeight: root.rareOpsReveal
            visible: root.rareOpsReveal > 0
            clip: true

            Pane {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                height: root.rareOpsHeight
                padding: 6
                background: Rectangle {
                    color: "transparent"
                    border.color: "green"
                    radius: 3
                }

                GridLayout {
                    anchors.fill: parent
                    columns: 4
                    columnSpacing: 4
                    rowSpacing: 4

                    Button {
                        id: max_integer
                        text: "M\u2081\u2082\u2088"  // M₁₂₈
                        font.pixelSize: 24
                        Layout.preferredWidth: 0
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
                        id: random_half_integer
                        text: "\u03BE\u2086\u2084"  // ξ₆₄
                        font.pixelSize: 24
                        Layout.preferredWidth: 0
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
                        id: random_integer
                        text: "\u03BE\u2081\u2082\u2088"  // ξ₁₂₈
                        font.pixelSize: 24
                        Layout.preferredWidth: 0
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
                        id: factor
                        text: "p\u00B7q"  // p·q
                        font.pixelSize: 24
                        Layout.preferredWidth: 0
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
                }
            }
        }

        TextArea {
            id: help
            font.pixelSize: 13
            Layout.bottomMargin: 8
            textFormat: Text.RichText
            text: "Смена знака числа: нажмите underscore <b>&#95;</b><br>
                    Cброс: <b>Esc</b><br>
                    Смена количества знаков после запятой: <b>Ctrl+S</b><br>
                    Экранная клавиатура: кнопка <b>123</b><br>
                    История операций: кнопка <b>История</b>
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

    // Выдвижная экранная клавиатура: цифры, разделитель, Backspace и "=".
    Item {
        id: keypad
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.bottomMargin: root.historyReveal
        height: root.keypadReveal
        clip: true
        visible: height > 0

        Rectangle {
            anchors.fill: parent
            color: "transparent"
            border.color: "green"
            border.width: 1
            radius: 3
        }

        GridLayout {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 6
            height: root.keypadHeight - 12
            columns: 4
            columnSpacing: 4
            rowSpacing: 4

            Repeater {
                model: [
                    { label: "7", action: "digit", row: 0, column: 0 },
                    { label: "8", action: "digit", row: 0, column: 1 },
                    { label: "9", action: "digit", row: 0, column: 2 },
                    { label: "\u2190", action: "backspace", row: 0, column: 3, rowSpan: 2,
                      tooltip: qsTr("Удалить символ слева от курсора") },
                    { label: "4", action: "digit", row: 1, column: 0 },
                    { label: "5", action: "digit", row: 1, column: 1 },
                    { label: "6", action: "digit", row: 1, column: 2 },
                    { label: "1", action: "digit", row: 2, column: 0 },
                    { label: "2", action: "digit", row: 2, column: 1 },
                    { label: "3", action: "digit", row: 2, column: 2 },
                    { label: "=", action: "equal", row: 2, column: 3, rowSpan: 2,
                      tooltip: qsTr("Равно") },
                    { label: ",", action: "separator", row: 3, column: 0,
                      tooltip: qsTr("Десятичный разделитель") },
                    { label: "0", action: "digit", row: 3, column: 1, columnSpan: 2 }
                ]

                delegate: Button {
                    id: keypadButton
                    text: modelData.label
                    font.pixelSize: 24
                    padding: 0
                    Layout.row: modelData.row
                    Layout.column: modelData.column
                    Layout.rowSpan: modelData.rowSpan || 1
                    Layout.columnSpan: modelData.columnSpan || 1
                    Layout.preferredWidth: 0
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    focusPolicy: Qt.NoFocus

                    ToolTip.visible: hovered && !!modelData.tooltip
                    ToolTip.text: modelData.tooltip || ""

                    background: Rectangle {
                        opacity: enabled ? 1 : 0.3
                        border.color: keypadButton.down ? "#ff0000"
                                    : (keypadButton.hovered ? "#0000ff" : "#00ff00")
                        border.width: 1
                        radius: 5
                    }

                    onClicked: {
                        switch (modelData.action) {
                        case "backspace":
                            root.removeLastCharacter()
                            break
                        case "separator":
                            root.appendDecimalSeparator()
                            break
                        case "equal":
                            AppCore.process(Operations.OperationEnums.EQUAL, input.text)
                            break
                        default:
                            root.appendCharacter(modelData.label)
                            break
                        }
                    }
                }
            }
        }
    } // keypad

    // Выдвижная панель истории операций (перенесена из консоли в QML).
    Item {
        id: historyPanel
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 0
        height: root.historyReveal
        clip: true
        visible: height > 0

        Rectangle {
            anchors.fill: parent
            color: "transparent"
            border.color: "green"
            border.width: 1
            radius: 3
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 6
            spacing: 4

            RowLayout {
                Layout.fillWidth: true
                spacing: 6

                Label {
                    text: qsTr("История операций")
                          + (historyView.count > 0 ? " (" + historyView.count + ")" : "")
                    font.pixelSize: 12
                    font.bold: true
                    Layout.fillWidth: true
                }

                Button {
                    id: clearHistoryButton
                    text: qsTr("Очистить")
                    font.pixelSize: 11
                    padding: 4
                    implicitWidth: 72
                    implicitHeight: 22
                    focusPolicy: Qt.NoFocus
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Очистить историю операций")

                    background: Rectangle {
                        opacity: enabled ? 1 : 0.3
                        border.color: clearHistoryButton.down ? "#ff0000"
                                    : (clearHistoryButton.hovered ? "#0000ff" : "#00ff00")
                        border.width: 1
                        radius: 5
                    }

                    onClicked: AppCore.clearHistory()
                }
            }

            ListView {
                id: historyView
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                spacing: 3
                boundsBehavior: Flickable.StopAtBounds
                model: AppCore.history
                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                onCountChanged: positionViewAtEnd()

                delegate: Item {
                    id: historyEntry
                    width: historyView.width
                    height: entryLayout.implicitHeight

                    ColumnLayout {
                        id: entryLayout
                        width: parent.width
                        spacing: 1

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 6

                            Label {
                                text: modelData.time
                                font.pixelSize: 11
                                color: "#909090"
                            }

                            Label {
                                visible: modelData.id >= 0
                                text: "[ID: " + modelData.id + "]"
                                font.pixelSize: 11
                                color: "#909090"
                            }

                            Label {
                                text: modelData.headline
                                font.pixelSize: 12
                                color: root.historyColor(modelData.kind)
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                            }
                        }

                        Label {
                            visible: modelData.detail !== ""
                            text: modelData.detail
                            font.pixelSize: 11
                            color: "#555555"
                            wrapMode: Text.WrapAnywhere
                            Layout.fillWidth: true
                        }
                    }
                }

                Label {
                    anchors.centerIn: parent
                    visible: historyView.count === 0
                    text: qsTr("История пуста")
                    font.pixelSize: 12
                    color: "#909090"
                }
            }
        }
    } // historyPanel

    // Кнопка-«ручка» для выдвижения и скрытия клавиатуры.
    Button {
        id: keypadToggle
        text: "123"
        font.pixelSize: 13
        padding: 0
        width: 46
        height: 26
        anchors.right: parent.right
        anchors.rightMargin: 6
        anchors.bottom: parent.bottom
        anchors.bottomMargin: root.keypadReveal + root.historyReveal + 6
        checkable: true
        checked: root.keypadOpen
        focusPolicy: Qt.NoFocus

        ToolTip.visible: hovered
        ToolTip.text: root.keypadOpen ? qsTr("Скрыть экранную клавиатуру")
                                      : qsTr("Показать экранную клавиатуру")

        // Текст задаём явно: в нажатом состоянии стиль делает его светлым,
        // и на светло-зелёном фоне «123» пропадает.
        contentItem: Item {
            Text {
                anchors.centerIn: parent
                text: keypadToggle.text
                font: keypadToggle.font
                color: "black"
            }
        }

        background: Rectangle {
            opacity: enabled ? 1 : 0.3
            color: keypadToggle.checked ? "#e8ffe8" : "transparent"
            border.color: keypadToggle.down ? "#ff0000"
                        : (keypadToggle.hovered ? "#0000ff" : "#00ff00")
            border.width: 1
            radius: 5
        }

        onCheckedChanged: root.keypadOpen = checked
    }

    // Кнопка-«ручка» для выдвижения и скрытия истории операций.
    Button {
        id: historyToggle
        text: qsTr("История")
        font.pixelSize: 12
        padding: 0
        width: 74
        height: 26
        anchors.right: keypadToggle.left
        anchors.rightMargin: 6
        anchors.bottom: parent.bottom
        anchors.bottomMargin: root.keypadReveal + root.historyReveal + 6
        checkable: true
        checked: root.historyOpen
        focusPolicy: Qt.NoFocus

        ToolTip.visible: hovered
        ToolTip.text: root.historyOpen ? qsTr("Скрыть историю операций")
                                       : qsTr("Показать историю операций")

        contentItem: Item {
            Text {
                anchors.centerIn: parent
                text: historyToggle.text
                font: historyToggle.font
                color: "black"
            }
        }

        background: Rectangle {
            opacity: enabled ? 1 : 0.3
            color: historyToggle.checked ? "#e8ffe8" : "transparent"
            border.color: historyToggle.down ? "#ff0000"
                        : (historyToggle.hovered ? "#0000ff" : "#00ff00")
            border.width: 1
            radius: 5
        }

        onCheckedChanged: {
            root.historyOpen = checked
            if (checked)
                historyView.positionViewAtEnd()
        }
    }
}
