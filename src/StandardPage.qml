import QtQuick
import QtQuick.Layouts
import "keys.js" as KeyMap

// The everyday calculator: percent, CE, 1/x, x² and √x around the digits,
// with memory above. The arithmetic lives in Backend (exposed as `backend`).
FocusScope {
    id: page

    property var ui
    signal showMemory()

    function activate() { keys.forceActiveFocus(); }
    function copy() { backend.copyResult(); }
    function paste() { backend.pasteNumber(); }
    function currentNumber() { return backend.currentNumber(); }
    function recall(value) { backend.enterNumber(value); }

    Item {
        id: keys
        anchors.fill: parent
        focus: true

        Keys.onPressed: function(event) {
            if (KeyMap.hasCommandModifier(event))
                return;
            const digit = KeyMap.numpadDigit(event);
            if (digit !== "") {
                backend.pressKey(digit);
            } else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter || event.text === "=") {
                backend.pressKey("=");
            } else if (event.key === Qt.Key_Backspace) {
                backend.pressKey("backspace");
            } else if (event.key === Qt.Key_Escape) {
                backend.pressKey("clear");
            } else if (event.key === Qt.Key_Delete) {
                backend.pressKey("clearEntry");
            } else if (event.key === Qt.Key_F9) {
                backend.pressKey("sign");
            } else if (event.text === "," || event.text === ".") {
                backend.pressKey(".");
            } else if (event.text === "s" || event.text === "S") {
                backend.pressKey("sign");
            } else if (event.text === "c" || event.text === "C") {
                backend.pressKey("clear");
            } else if (event.text === "x" || event.text === "X") {
                backend.pressKey("*");
            } else if (event.text === "r" || event.text === "R") {
                backend.pressKey("reciprocal");
            } else if (event.text === "q" || event.text === "Q") {
                backend.pressKey("square");
            } else if (event.text === "@") {
                backend.pressKey("sqrt");
            } else if (/^[0-9+\-*/%^]$/.test(event.text)) {
                backend.pressKey(event.text);
            } else {
                return;
            }
            event.accepted = true;
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: page.ui.px(8)

        CalcDisplay {
            ui: page.ui
            Layout.fillWidth: true
            Layout.preferredHeight: Math.round(page.height * 0.22)
            Layout.leftMargin: page.ui.px(8)
            Layout.rightMargin: page.ui.px(8)
            expression: backend.expression
            display: backend.display
        }

        MemoryRow {
            ui: page.ui
            target: page
            Layout.fillWidth: true
            Layout.preferredHeight: page.ui.px(30)
            Layout.fillHeight: false
            onShowMemory: page.showMemory()
        }

        GridLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            columns: 4
            rowSpacing: page.ui.px(6)
            columnSpacing: page.ui.px(6)

            Repeater {
                model: [
                    { label: "%", key: "%", kind: "function" },
                    { label: "CE", key: "clearEntry", kind: "function" },
                    { label: "C", key: "clear", kind: "function" },
                    { label: "", key: "backspace", kind: "function", icon: "backspace" },
                    { label: "1/x", key: "reciprocal", kind: "function", name: qsTr("Reciprocal") },
                    { label: "x²", key: "square", kind: "function", name: qsTr("Square") },
                    { label: "√x", key: "sqrt", kind: "function", name: qsTr("Square root") },
                    { label: "÷", key: "÷", kind: "operator" },
                    { label: "7", key: "7", kind: "number" },
                    { label: "8", key: "8", kind: "number" },
                    { label: "9", key: "9", kind: "number" },
                    { label: "×", key: "×", kind: "operator" },
                    { label: "4", key: "4", kind: "number" },
                    { label: "5", key: "5", kind: "number" },
                    { label: "6", key: "6", kind: "number" },
                    { label: "−", key: "-", kind: "operator" },
                    { label: "1", key: "1", kind: "number" },
                    { label: "2", key: "2", kind: "number" },
                    { label: "3", key: "3", kind: "number" },
                    { label: "+", key: "+", kind: "operator" },
                    { label: "±", key: "sign", kind: "number" },
                    { label: "0", key: "0", kind: "number" },
                    { label: ".", key: ".", kind: "number" },
                    { label: "=", key: "=", kind: "equals" }
                ]

                CalcButton {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.preferredWidth: 1
                    Layout.preferredHeight: 1
                    label: modelData.label
                    keyValue: modelData.key
                    kind: modelData.kind
                    accessibleName: modelData.name || ""
                    iconName: modelData.icon || ""
                    pageColor: page.ui.pageColor
                    inkColor: page.ui.inkColor
                    onActivated: {
                        backend.pressKey(keyValue);
                        page.activate();
                    }
                }
            }
        }
    }
}
