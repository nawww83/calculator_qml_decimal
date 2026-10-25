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
    width: 760

    // Высота окна в свёрнутом состоянии и высота выдвижных панелей.
    readonly property int collapsedHeight: 334
    readonly property int keypadHeight: 236
    readonly property int rareOpsHeight: 58
    readonly property int historyHeight: 190

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
    color: "#202428"
    title: qsTr("Decimal калькулятор 128-bit, version " + Global.APP_VERSION)

    // --- Оформление «бухгалтерского» настольного калькулятора ---------------

    readonly property string uiFont: Qt.platform.os === "windows" ? "Segoe UI"
                                                                 : "DejaVu Sans"
    readonly property string monoFont: Qt.platform.os === "windows" ? "Consolas"
                                                                     : "DejaVu Sans Mono"
    readonly property string lcdFont: Qt.platform.os === "windows" ? "Lucida Console"
                                                                    : "DejaVu Sans Mono"

    // Индикатор памяти в дисплее.
    property bool memoryUsed: false
    property string memoryValue: ""
    // Доступность кнопки факторизации (управляется из AppCore).
    property bool factorAvailable: true
    // Признак того, что текст поля ввода меняет само приложение, а не пользователь.
    property bool inputIsSynced: false

    // Привязываемся напрямую к свойству C++
    property int userWidth: AppCore.userDisplayWidth

    // Крупные цифры автоматически уменьшаются, чтобы длинное число
    // целиком помещалось на ЖК-экране.
    readonly property int displayFontSize: {
        var n = input.text.length
        return n > 32 ? 19 : n > 26 ? 22 : n > 20 ? 26 : 32
    }

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
            root.inputIsSynced = true
            input.text = ""
            root.inputIsSynced = false
        }

        function onSetInput(val) {
            root.inputIsSynced = true
            input.text = thousandSeparator(val)
            input.cursorPosition = input.text.length
            root.inputIsSynced = false
        }

        function onSetEnableFactorButton(val) {
            root.factorAvailable = val
        }

        function onShowTempResult(val, is_number) {
            temp_result.isFactorNote = false
            temp_result.isError = !is_number; // Выставляем флаг
            if (is_number) {
                temp_result.text = " " + thousandSeparator(val);
            } else {
                temp_result.text = " " + val;
            }
        }

        function onShowFactorResult(val) {
            temp_result.isError = false
            temp_result.isFactorNote = true
            temp_result.text = " " + val
        }

        function onShowCurrentOperation(operation) {
            current_operation.text = " " + operation
        }

        function onClearTempResult() {
            temp_result.isFactorNote = false
            temp_result.text = ""
        }

        function onClearCurrentOperation() {
            current_operation.text = ""
        }
    }

    // Разделитель триад: узкий пробел, чтобы группы цифр не расползались
    // на ширину полноценного моноширинного знака.
    readonly property string groupSeparator: "\u2004"

    function thousandSeparator(inp){
        inp = inp.replace(/\s/g, "")
        var idx = inp.indexOf(",")
        if (idx === -1)
            return inp.replace(/(\d)(?=(\d{3})+(?!\d))/g, "$1" + groupSeparator)
        else
            return inp.substring(0, idx).replace(/(\d)(?=(\d{3})+(?!\d))/g, "$1" + groupSeparator)
                   + inp.substring(idx)
    }

    function setToMemory(val){
        if (val === "")
            return

        Global.memory_cell = val.replace(/\s/g, "")
        root.memoryValue = Global.memory_cell
        root.memoryUsed = true
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

    // Пара нулей — привычная клавиша бухгалтерских калькуляторов.
    function appendDoubleZero() {
        var text = input.text
        var pos = (input.activeFocus && input.cursorPosition >= 0)
                ? input.cursorPosition : text.length

        // Ведущих нулей не набираем: пустой ввод и "0" остаются нулём.
        if (text === "" || text === "0" || text === "-0")
            return

        var candidate = text.substring(0, pos) + "00" + text.substring(pos)
        if (!decimalRegEx.test(candidate))
            return

        setInputText(candidate, pos + 2)
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
            if (!/\s/.test(value.charAt(i)))
                ++seen
            if (seen === count)
                return i + 1
        }
        return value.length
    }

    // Корпус прибора: тёмный пластик с мягким верхним бликом.
    Rectangle {
        anchors.fill: parent
        radius: 10
        border.width: 1
        border.color: "#15181b"
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#3d434a" }
            GradientStop { position: 0.45; color: "#2e3339" }
            GradientStop { position: 1.0; color: "#24282d" }
        }

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 1
            height: 2
            radius: 2
            color: "#ffffff"
            opacity: 0.10
        }
    }

    // Клавиша в стиле бумажного диалога настроек.
    component DialogKey: Button {
        id: dialogKey
        implicitWidth: 92
        implicitHeight: 30
        focusPolicy: Qt.NoFocus

        contentItem: Text {
            text: dialogKey.text
            font.pixelSize: 12
            color: "#3a352c"
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }

        background: Rectangle {
            radius: 5
            color: dialogKey.down ? "#ddd3ba"
                   : (dialogKey.hovered ? "#ece4d1" : "#f9f5ea")
            border.width: 1
            border.color: "#b9b09a"
        }
    }

    Dialog {
        id: dialog
        title: qsTr("Настройки")
        modal: true
        width: 360
        padding: 16
        x: Math.round((root.width - width) / 2)
        y: Math.round((root.height - height) / 2)
        closePolicy: Popup.CloseOnEscape
        header: null
        footer: null

        background: Rectangle {
            color: "#f4f1e7"
            radius: 10
            border.width: 1
            border.color: "#8d8778"
        }

        contentItem: ColumnLayout {
            spacing: 10

            Text {
                text: qsTr("Настройки")
                font.pixelSize: 15
                font.bold: true
                color: "#3a352c"
            }

            Text {
                text: qsTr("Количество знаков после запятой (0–18)")
                font.pixelSize: 12
                color: "#5c5546"
            }

            TextField {
                id: newDecimalWidthInput
                Layout.fillWidth: true
                font.pixelSize: 15
                placeholderText: qsTr("от 0 до 18")
                validator: RegularExpressionValidator { regularExpression: /^(1[0-8]|[0-9])$/ }
                Keys.onReturnPressed: dialog.accept()

                background: Rectangle {
                    color: "#fffdf6"
                    radius: 4
                    border.width: 1
                    border.color: "#b9b09a"
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.topMargin: 4
                spacing: 8

                Item { Layout.fillWidth: true }

                DialogKey {
                    text: qsTr("Отмена")
                    onClicked: dialog.reject()
                }

                DialogKey {
                    text: "OK"
                    onClicked: dialog.accept()
                }
            }
        }

        onAccepted: {
            var value = parseInt(newDecimalWidthInput.text, 10)
            if (!isNaN(value))
                AppCore.change_decimal_width(value)
        }
        onOpened: {
            newDecimalWidthInput.text = root.userWidth
            newDecimalWidthInput.focus = true
        }
    }

    // Подсказка «клавиша + описание». Клавиша рисуется в рамке — так подсказка
    // читается как клавиатурное сочетание, а не как строка текста.
    component Hint: RowLayout {
        id: hint
        property string key: ""
        property string description: ""
        spacing: 6

        Rectangle {
            Layout.alignment: Qt.AlignVCenter
            implicitWidth: Math.max(20, keyText.implicitWidth + 10)
            implicitHeight: keyText.implicitHeight + 4
            radius: 3
            color: "#4b535b"
            border.width: 1
            border.color: "#2a2f34"

            Text {
                id: keyText
                anchors.centerIn: parent
                text: hint.key
                font.family: Qt.platform.os === "windows" ? "Consolas" : "DejaVu Sans Mono"
                font.pixelSize: 11
                font.bold: true
                color: "#e6ebef"
            }
        }

        Text {
            Layout.alignment: Qt.AlignVCenter
            text: hint.description
            font.pixelSize: 12
            color: "#aeb6bd"
        }
    }

    // Объёмная клавиша калькулятора. Роль задаёт цвет и «характер» клавиши.
    component CalcKey: Button {
        id: key

        property string role: "digit"       // digit | digitAlt | operator | memory | clear | equal | accent | function
        property bool compact: false        // мелкая клавиша функционального ряда
        property string hint: ""

        readonly property bool lightFace: role === "digit" || role === "digitAlt"
        readonly property bool active: checkable && checked

        readonly property color faceTop: {
            switch (role) {
            case "digit":    return "#fbfaf4"
            case "digitAlt": return "#e9e5d8"
            case "operator": return "#8e98a2"
            case "memory":   return "#6d7884"
            case "clear":    return "#c95f45"
            case "equal":    return "#3d8a66"
            case "accent":   return "#b98b32"
            default:         return "#4e565e"
            }
        }

        readonly property color faceBottom: {
            switch (role) {
            case "digit":    return "#dedacd"
            case "digitAlt": return "#cec9ba"
            case "operator": return "#6c7681"
            case "memory":   return "#525c66"
            case "clear":    return "#a8472f"
            case "equal":    return "#2a664a"
            case "accent":   return "#946c22"
            default:         return "#3b4249"
            }
        }

        readonly property color faceBorder: {
            switch (role) {
            case "digit":    return "#b3afa2"
            case "digitAlt": return "#a8a395"
            case "operator": return "#4c545c"
            case "memory":   return "#3e474f"
            case "clear":    return "#7e3220"
            case "equal":    return "#1f4f39"
            case "accent":   return "#6d4e14"
            default:         return "#262b30"
            }
        }

        readonly property color faceText: lightFace ? "#22262a" : "#f2f6f8"

        text: ""
        focusPolicy: Qt.NoFocus
        padding: 0
        opacity: enabled ? 1.0 : 0.4
        scale: down ? 0.975 : 1.0

        Layout.preferredWidth: 0
        Layout.fillWidth: true
        Layout.fillHeight: true

        Behavior on scale { NumberAnimation { duration: 60; easing.type: Easing.OutQuad } }

        ToolTip.visible: hovered && hint !== ""
        ToolTip.text: hint
        ToolTip.delay: 600

        contentItem: Text {
            text: key.text
            font.family: Qt.platform.os === "windows" ? "Segoe UI" : "DejaVu Sans"
            font.pixelSize: key.compact ? 15 : (key.text.length > 3 ? 17 : 22)
            font.weight: key.lightFace ? Font.Normal : Font.DemiBold
            color: key.active ? "#ffd98a" : key.faceText
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }

        background: Rectangle {
            radius: 6
            border.width: key.active ? 2 : 1
            border.color: key.active ? "#c9a227" : key.faceBorder
            gradient: Gradient {
                GradientStop {
                    position: 0.0
                    color: key.down ? Qt.darker(key.faceBottom, 1.10) : key.faceTop
                }
                GradientStop {
                    position: 0.55
                    color: key.down ? Qt.darker(key.faceBottom, 1.18) : key.faceBottom
                }
                GradientStop {
                    position: 1.0
                    color: key.down ? Qt.darker(key.faceBottom, 1.24)
                                    : Qt.darker(key.faceBottom, 1.10)
                }
            }

            // Верхний блик и нижняя тень создают ощущение выпуклой клавиши.
            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: 1
                height: 1
                radius: 1
                color: "#ffffff"
                opacity: key.lightFace ? 0.8 : 0.25
            }

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.margins: 1
                height: 2
                radius: 1
                color: "#000000"
                opacity: 0.14
            }
        }
    }

    ColumnLayout {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: root.collapsedHeight + Math.round(root.rareOpsReveal)
        spacing: 8

        // Шильдик прибора.
        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: 14
            Layout.rightMargin: 14
            Layout.topMargin: 10
            spacing: 8

            Text {
                text: "DECIMAL 128"
                font.family: root.uiFont
                font.pixelSize: 11
                font.bold: true
                font.letterSpacing: 2.4
                color: "#b9c2c9"
            }

            // Индикатор питания — маленький зелёный светодиод.
            Rectangle {
                Layout.alignment: Qt.AlignVCenter
                width: 7
                height: 7
                radius: 3.5
                color: "#84ca6e"
                border.width: 1
                border.color: "#3c5c34"

                Rectangle {
                    anchors.centerIn: parent
                    width: 13
                    height: 13
                    radius: 6.5
                    color: "#84ca6e"
                    opacity: 0.16
                }
            }

            Item { Layout.fillWidth: true }

            Text {
                text: "FINANCIAL DESK CALCULATOR"
                font.family: root.uiFont
                font.pixelSize: 9
                font.letterSpacing: 1.4
                color: "#77828b"
            }
        }

        // ЖК-дисплей в утопленном тёмном безеле.
        Rectangle {
            Layout.fillWidth: true
            Layout.leftMargin: 10
            Layout.rightMargin: 10
            Layout.preferredHeight: 100
            radius: 10
            color: "#15181b"
            border.width: 1
            border.color: "#0b0d0f"

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: 1
                height: 2
                radius: 2
                color: "#ffffff"
                opacity: 0.05
            }

            Rectangle {
                id: lcd
                anchors.fill: parent
                anchors.margins: 7
                radius: 5
                border.width: 1
                border.color: "#6e7d63"
                gradient: Gradient {
                    GradientStop { position: 0.0; color: "#d3dfc8" }
                    GradientStop { position: 0.55; color: "#c4d3b8" }
                    GradientStop { position: 1.0; color: "#b6c7a9" }
                }

                // Блик стекла.
                Rectangle {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: 2
                    height: parent.height * 0.45
                    radius: 4
                    opacity: 0.18
                    gradient: Gradient {
                        GradientStop { position: 0.0; color: "#ffffff" }
                        GradientStop { position: 1.0; color: "transparent" }
                    }
                }

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 8
                    spacing: 0

                    // Строка индикаторов: память, точность, текущая операция.
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 10

                        Rectangle {
                            implicitWidth: 18
                            implicitHeight: 14
                            radius: 2
                            color: root.memoryUsed ? "#26402a" : "transparent"
                            border.width: 1
                            border.color: "#7e8f76"

                            Text {
                                anchors.centerIn: parent
                                text: "M"
                                font.family: root.monoFont
                                font.pixelSize: 9
                                font.bold: true
                                color: root.memoryUsed ? "#dcebd2" : "#8a9a82"
                            }

                            MouseArea {
                                id: memoryMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                acceptedButtons: Qt.NoButton
                            }

                            ToolTip.visible: memoryMouse.containsMouse && root.memoryUsed
                            ToolTip.text: qsTr("Память: ") + root.memoryValue
                            ToolTip.delay: 400
                        }

                        Text {
                            text: "DEC " + root.userWidth
                            font.family: root.monoFont
                            font.pixelSize: 10
                            color: "#5f7059"
                        }

                        Item { Layout.fillWidth: true }

                        Text {
                            id: current_operation
                            text: ""
                            font.family: root.monoFont
                            font.pixelSize: 12
                            color: "#3d5539"
                        }
                    }

                    TextField {
                        id: input
                        text: ""
                        font.family: root.lcdFont
                        font.pixelSize: root.displayFontSize
                        font.letterSpacing: 1
                        color: "#1c3320"
                        horizontalAlignment: TextInput.AlignRight
                        verticalAlignment: TextInput.AlignVCenter
                        selectByMouse: true
                        selectionColor: "#7fa27a"
                        selectedTextColor: "#10200f"
                        placeholderText: "0"
                        placeholderTextColor: "#7d917a"
                        padding: 0
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        maximumLength: 80
                        validator: RegularExpressionValidator { regularExpression: root.decimalRegEx }

                        focus: true
                        visible: true

                        background: Item {}

                        property bool ctrlPressed: false

                        // Пользователь начал править число: подсказка о разложении
                        // предыдущего числа больше не относится к содержимому экрана.
                        onTextChanged: {
                            if (!root.inputIsSynced && temp_result.isFactorNote) {
                                temp_result.isFactorNote = false
                                temp_result.text = ""
                            }
                        }

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

                    // Нижняя строка ЖК: вспомогательный результат и разрядность.
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        Text {
                            id: temp_result
                            text: ""
                            font.family: root.monoFont
                            font.pixelSize: 12
                            elide: Text.ElideRight
                            Layout.fillWidth: true

                            // Создаем внутреннее свойство для отслеживания ошибок
                            property bool isError: false

                            // В строке показано разложение на простые множители.
                            property bool isFactorNote: false

                            // Декларативно привязываем цвет через формулу.
                            // Такая связь является реактивной и никогда не вызовет варнингов.
                            color: isError ? "#8e2f22" : "#556b4f"
                        }

                        Text {
                            text: "128-BIT"
                            font.family: root.monoFont
                            font.pixelSize: 9
                            font.letterSpacing: 1
                            color: "#6d7f66"
                        }
                    }
                }
            }
        }

        // Функциональные клавиши: два ряда мелких тёмных клавиш.
        Rectangle {
            Layout.fillWidth: true
            Layout.leftMargin: 10
            Layout.rightMargin: 10
            Layout.preferredHeight: 112
            radius: 8
            color: "#2a2f34"
            border.width: 1
            border.color: "#1a1e22"

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: 1
                height: 1
                color: "#ffffff"
                opacity: 0.06
            }

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 6
                spacing: 6

                GridLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    columns: 8
                    columnSpacing: 5
                    rowSpacing: 5

                    Repeater {
                        model: [
                            { label: "\u002b", op: Operations.OperationEnums.ADD,
                              hint: qsTr("Сложение") },
                            { label: "\u2212", op: Operations.OperationEnums.SUB,
                              hint: qsTr("Вычитание") },
                            { label: "\u00d7", op: Operations.OperationEnums.MULT,
                              hint: qsTr("Умножение") },
                            { label: "\u00f7", op: Operations.OperationEnums.DIV,
                              hint: qsTr("Деление") },
                            { label: "\u005e", op: Operations.OperationEnums.POW,
                              hint: qsTr("Возведение в степень") },
                            { label: "\u33d1", op: Operations.OperationEnums.LN,
                              hint: qsTr("Натуральный логарифм") },
                            { label: "\u212f\u02e3", op: Operations.OperationEnums.EXP,
                              hint: qsTr("Экспонента") },
                            { label: "C", op: Operations.OperationEnums.CLEAR_ALL, role: "clear",
                              hint: qsTr("Сброс / Остановка расчета") }
                        ]

                        delegate: CalcKey {
                            text: modelData.label
                            role: modelData.role || "operator"
                            compact: true
                            hint: modelData.hint
                            onClicked: AppCore.process(modelData.op, input.text)
                        }
                    }
                }

                GridLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    columns: 8
                    columnSpacing: 5
                    rowSpacing: 5

                    Repeater {
                        model: [
                            { label: "\u221ax", op: Operations.OperationEnums.SQRT,
                              hint: qsTr("Квадратный корень") },
                            { label: "x\u00b2", op: Operations.OperationEnums.SQR,
                              hint: qsTr("Квадрат числа") },
                            { label: "1/x", op: Operations.OperationEnums.RECIPROC,
                              hint: qsTr("Обратное число") },
                            { label: "[x]", op: Operations.OperationEnums.ROUND,
                              hint: qsTr("Округлить до ближайшего целого") },
                            { label: ".99\u2192", op: Operations.OperationEnums.FIX_NINES,
                              hint: qsTr("Схлопнуть машинный хвост девяток") },
                            { label: "MS", action: "memoryStore", role: "memory",
                              hint: qsTr("Положить в память") },
                            { label: "MR", action: "memoryRecall", role: "memory",
                              hint: qsTr("Извлечь из памяти") }
                        ]

                        delegate: CalcKey {
                            text: modelData.label
                            role: modelData.role || "function"
                            compact: true
                            hint: modelData.hint
                            onClicked: {
                                if (modelData.action === "memoryStore")
                                    root.setToMemory(input.text)
                                else if (modelData.action === "memoryRecall") {
                                    if (root.memoryValue !== "")
                                        root.setInputText(root.memoryValue,
                                                          root.memoryValue.length)
                                }
                                else
                                    AppCore.process(modelData.op, input.text)
                            }
                        }
                    }

                    // Кнопка «⋯»: показывает и скрывает редкие операции.
                    CalcKey {
                        id: more_btn
                        role: "function"
                        compact: true
                        checkable: true
                        checked: root.rareOpsOpen
                        hint: root.rareOpsOpen ? qsTr("Скрыть редкие операции")
                                               : qsTr("Показать редкие операции")

                        // Три точки рисуем сами: символ «⋯» в некоторых шрифтах
                        // получается слишком мелким и плохо заметен.
                        contentItem: Item {
                            Row {
                                anchors.centerIn: parent
                                spacing: 4

                                Repeater {
                                    model: 3

                                    Rectangle {
                                        width: 4
                                        height: 4
                                        radius: 2
                                        color: more_btn.checked ? "#ffd98a" : "#e8ecef"
                                    }
                                }
                            }
                        }

                        onCheckedChanged: root.rareOpsOpen = checked
                    }
                }
            }
        }

        // Выдвижная панель редких операций: открывается кнопкой «⋯».
        Item {
            id: rareOpsRow
            Layout.fillWidth: true
            Layout.preferredHeight: root.rareOpsReveal
            visible: root.rareOpsReveal > 0
            clip: true

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.leftMargin: 10
                anchors.rightMargin: 10
                height: root.rareOpsHeight
                radius: 8
                color: "#2a2f34"
                border.width: 1
                border.color: "#1a1e22"

                GridLayout {
                    anchors.fill: parent
                    anchors.margins: 6
                    columns: 4
                    columnSpacing: 5
                    rowSpacing: 5

                    Repeater {
                        model: [
                            { label: "M\u2081\u2082\u2088", op: Operations.OperationEnums.MAX_INT_VALUE,
                              hint: qsTr("Наибольшее целое число") },
                            { label: "\u03be\u2086\u2084", op: Operations.OperationEnums.RANDINT64,
                              hint: qsTr("Случайное целое 64-битное число") },
                            { label: "\u03be\u2081\u2082\u2088", op: Operations.OperationEnums.RANDINT,
                              hint: qsTr("Случайное целое 128-битное число") },
                            { label: "p\u00b7q", op: Operations.OperationEnums.FACTOR,
                              needsFactor: true,
                              hint: qsTr("Разложить на простые множители целую часть числа") }
                        ]

                        delegate: CalcKey {
                            text: modelData.label
                            role: "function"
                            compact: true
                            hint: modelData.hint
                            enabled: !modelData.needsFactor || root.factorAvailable
                            onClicked: AppCore.process(modelData.op, input.text)
                        }
                    }
                }
            }
        }

        // Пустое место отжимает подсказки к нижнему краю окна.
        Item { Layout.fillHeight: true }

        // Подсказки по горячим клавишам: две строки по три ячейки.
        // Справа оставлено место под кнопки «История» и «123».
        GridLayout {
            id: help
            Layout.fillWidth: true
            Layout.leftMargin: 14
            Layout.rightMargin: 158
            Layout.bottomMargin: 10
            columns: 3
            columnSpacing: 14
            rowSpacing: 3

            Hint {
                Layout.fillWidth: true
                key: "Shift+\u2212"
                description: qsTr("смена знака")
            }

            Hint {
                Layout.fillWidth: true
                key: "Esc"
                description: qsTr("сброс")
            }

            Hint {
                Layout.fillWidth: true
                key: "Ctrl+S"
                description: qsTr("точность")
            }

            Hint {
                Layout.fillWidth: true
                key: "123"
                description: qsTr("экранная клавиатура")
            }

            Hint {
                Layout.fillWidth: true
                key: qsTr("История")
                description: qsTr("журнал операций")
            }

            Label {
                Layout.fillWidth: true
                font.pixelSize: 12
                color: "#aeb6bd"
                textFormat: Text.RichText
                text: "<a href=\"https://github.com/nawww83/calculator_qml_decimal\" style=\"color:#7fb3e0;text-decoration:none\">See github: nawww83</a>"
                onLinkActivated: link => Qt.openUrlExternally(link)
                MouseArea {
                    anchors.fill: parent
                    acceptedButtons: Qt.NoButton
                    cursorShape: parent.hoveredLink ? Qt.PointingHandCursor : Qt.ArrowCursor
                }
            }
        }
    }

    // Выдвижная экранная клавиатура: цифровой блок, операторы, Backspace и "=".
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
            anchors.margins: 1
            radius: 8
            color: "#22262b"
            border.width: 1
            border.color: "#0f1114"

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: 1
                height: 1
                color: "#ffffff"
                opacity: 0.06
            }
        }

        GridLayout {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 8
            height: root.keypadHeight - 16
            columns: 5
            columnSpacing: 5
            rowSpacing: 5

            Repeater {
                model: [
                    { label: "7", action: "digit", row: 0, column: 0 },
                    { label: "8", action: "digit", row: 0, column: 1 },
                    { label: "9", action: "digit", row: 0, column: 2 },
                    { label: "\u00f7", action: "div", row: 0, column: 3, role: "operator",
                      hint: qsTr("Деление") },
                    { label: "\u2190", action: "backspace", row: 0, column: 4, role: "operator",
                      hint: qsTr("Удалить символ слева от курсора") },

                    { label: "4", action: "digit", row: 1, column: 0 },
                    { label: "5", action: "digit", row: 1, column: 1 },
                    { label: "6", action: "digit", row: 1, column: 2 },
                    { label: "\u00d7", action: "mul", row: 1, column: 3, role: "operator",
                      hint: qsTr("Умножение") },
                    { label: "C", action: "clear", row: 1, column: 4, role: "clear",
                      hint: qsTr("Сброс / Остановка расчета") },

                    { label: "1", action: "digit", row: 2, column: 0 },
                    { label: "2", action: "digit", row: 2, column: 1 },
                    { label: "3", action: "digit", row: 2, column: 2 },
                    { label: "\u2212", action: "sub", row: 2, column: 3, role: "operator",
                      hint: qsTr("Вычитание") },
                    { label: "=", action: "equal", row: 2, column: 4, rowSpan: 2,
                      role: "equal", hint: qsTr("Равно") },

                    { label: "0", action: "digit", row: 3, column: 0 },
                    { label: "00", action: "doubleZero", row: 3, column: 1, role: "digitAlt",
                      hint: qsTr("Два нуля") },
                    { label: ",", action: "separator", row: 3, column: 2, role: "digitAlt",
                      hint: qsTr("Десятичный разделитель") },
                    { label: "\u002b", action: "add", row: 3, column: 3, role: "operator",
                      hint: qsTr("Сложение") }
                ]

                delegate: CalcKey {
                    id: keypadButton
                    text: modelData.label
                    role: modelData.role || "digit"
                    hint: modelData.hint || ""
                    Layout.row: modelData.row
                    Layout.column: modelData.column
                    Layout.rowSpan: modelData.rowSpan || 1
                    Layout.columnSpan: modelData.columnSpan || 1

                    onClicked: {
                        switch (modelData.action) {
                        case "backspace":
                            root.removeLastCharacter()
                            break
                        case "separator":
                            root.appendDecimalSeparator()
                            break
                        case "doubleZero":
                            root.appendDoubleZero()
                            break
                        case "equal":
                            AppCore.process(Operations.OperationEnums.EQUAL, input.text)
                            break
                        case "clear":
                            AppCore.process(Operations.OperationEnums.CLEAR_ALL)
                            break
                        case "add":
                            AppCore.process(Operations.OperationEnums.ADD, input.text)
                            break
                        case "sub":
                            AppCore.process(Operations.OperationEnums.SUB, input.text)
                            break
                        case "mul":
                            AppCore.process(Operations.OperationEnums.MULT, input.text)
                            break
                        case "div":
                            AppCore.process(Operations.OperationEnums.DIV, input.text)
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

    // Выдвижная панель истории операций в виде бумажного журнала.
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
            anchors.margins: 1
            radius: 8
            color: "#f7f2e4"
            border.width: 1
            border.color: "#a39a83"
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 10
            spacing: 6

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                Label {
                    text: qsTr("ЖУРНАЛ ОПЕРАЦИЙ")
                          + (historyView.count > 0 ? "  (" + historyView.count + ")" : "")
                    font.family: root.monoFont
                    font.pixelSize: 12
                    font.bold: true
                    font.letterSpacing: 1
                    color: "#4a4438"
                    Layout.fillWidth: true
                }

                Button {
                    id: clearHistoryButton
                    text: qsTr("Очистить")
                    font.pixelSize: 11
                    padding: 4
                    implicitWidth: 76
                    implicitHeight: 24
                    focusPolicy: Qt.NoFocus
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Очистить историю операций")

                    contentItem: Text {
                        text: clearHistoryButton.text
                        font: clearHistoryButton.font
                        color: "#4a4438"
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }

                    background: Rectangle {
                        radius: 4
                        color: clearHistoryButton.down ? "#e4dbc4"
                               : (clearHistoryButton.hovered ? "#efe8d6" : "transparent")
                        border.width: 1
                        border.color: "#b9b09a"
                    }

                    onClicked: AppCore.clearHistory()
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 1
                color: "#c9c0a9"
            }

            ListView {
                id: historyView
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                spacing: 4
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
                                font.family: root.monoFont
                                font.pixelSize: 10
                                color: "#8b8474"
                            }

                            Label {
                                visible: modelData.id >= 0
                                text: "[ID: " + modelData.id + "]"
                                font.family: root.monoFont
                                font.pixelSize: 10
                                color: "#8b8474"
                            }

                            Label {
                                text: modelData.headline
                                font.family: root.monoFont
                                font.pixelSize: 11
                                color: root.historyColor(modelData.kind)
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                            }

                            Label {
                                visible: modelData.duration !== undefined
                                         && modelData.duration !== ""
                                text: "[" + modelData.duration + "]"
                                font.family: root.monoFont
                                font.pixelSize: 10
                                color: "#8b8474"
                            }
                        }

                        Label {
                            visible: modelData.detail !== ""
                            text: modelData.detail
                            font.family: root.monoFont
                            font.pixelSize: 10
                            color: "#5c5546"
                            wrapMode: Text.WrapAnywhere
                            Layout.fillWidth: true
                        }
                    }
                }

                Label {
                    anchors.centerIn: parent
                    visible: historyView.count === 0
                    text: qsTr("История пуста")
                    font.family: root.monoFont
                    font.pixelSize: 12
                    color: "#8b8474"
                }
            }
        }
    } // historyPanel

    // Кнопки-«ручки» для выдвижения клавиатуры и журнала операций.
    Row {
        anchors.right: parent.right
        anchors.rightMargin: 10
        anchors.bottom: parent.bottom
        anchors.bottomMargin: root.keypadReveal + root.historyReveal + 6
        spacing: 6

        Button {
            id: historyToggle
            text: qsTr("История")
            font.pixelSize: 12
            padding: 0
            width: 78
            height: 28
            checkable: true
            checked: root.historyOpen
            focusPolicy: Qt.NoFocus

            ToolTip.visible: hovered
            ToolTip.text: root.historyOpen ? qsTr("Скрыть историю операций")
                                           : qsTr("Показать историю операций")

            contentItem: Text {
                text: historyToggle.text
                font: historyToggle.font
                color: historyToggle.checked ? "#1e2a1a" : "#dfe5ea"
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }

            background: Rectangle {
                radius: 5
                border.width: 1
                border.color: historyToggle.checked ? "#5d6f4f" : "#262b30"
                gradient: Gradient {
                    GradientStop {
                        position: 0.0
                        color: historyToggle.checked ? "#c3d3b4" : "#4e565e"
                    }
                    GradientStop {
                        position: 1.0
                        color: historyToggle.checked ? "#9db28c" : "#3b4249"
                    }
                }
            }

            onCheckedChanged: {
                root.historyOpen = checked
                if (checked)
                    historyView.positionViewAtEnd()
            }
        }

        Button {
            id: keypadToggle
            text: "123"
            font.pixelSize: 13
            font.bold: true
            padding: 0
            width: 48
            height: 28
            checkable: true
            checked: root.keypadOpen
            focusPolicy: Qt.NoFocus

            ToolTip.visible: hovered
            ToolTip.text: root.keypadOpen ? qsTr("Скрыть экранную клавиатуру")
                                          : qsTr("Показать экранную клавиатуру")

            // Текст задаём явно: в нажатом состоянии стиль делает его светлым,
            // и на светло-зелёном фоне «123» пропадает.
            contentItem: Text {
                text: keypadToggle.text
                font: keypadToggle.font
                color: keypadToggle.checked ? "#1e2a1a" : "#dfe5ea"
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }

            background: Rectangle {
                radius: 5
                border.width: 1
                border.color: keypadToggle.checked ? "#5d6f4f" : "#262b30"
                gradient: Gradient {
                    GradientStop {
                        position: 0.0
                        color: keypadToggle.checked ? "#c3d3b4" : "#4e565e"
                    }
                    GradientStop {
                        position: 1.0
                        color: keypadToggle.checked ? "#9db28c" : "#3b4249"
                    }
                }
            }

            onCheckedChanged: root.keypadOpen = checked
        }
    }
}
