import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import "keys.js" as KeyMap

// Scientific: powers, roots, logarithms, factorials, parentheses, and the
// trigonometry and function menus. 2nd swaps the left column for its
// inverses (x³, ∛x, ʸ√x, 2ˣ, logᵧx, eˣ). The work is in ScientificCalc
// (exposed as `scientific`).
FocusScope {
    id: page

    property var ui
    property bool second: false
    property bool trigSecond: false
    property bool hyperbolic: false
    signal showMemory()

    readonly property var angleNames: ["DEG", "RAD", "GRAD"]

    function activate() { keys.forceActiveFocus(); }
    function copy() { scientific.copyResult(); }
    function paste() { scientific.paste(); }
    function currentNumber() { return scientific.currentNumber(); }
    function recall(value) { scientific.enterNumber(value); }

    function press(key) {
        scientific.pressKey(key);
        activate();
    }
    function apply(name) {
        scientific.applyFunction(name);
        activate();
    }
    function trigLabel(name) {
        return name + (hyperbolic ? "h" : "") + (trigSecond ? "⁻¹" : "");
    }
    // sin, with 2nd → asin and hyp → sinh, asinh.
    function trig(name) {
        apply((trigSecond ? "a" : "") + name + (hyperbolic ? "h" : ""));
    }

    Item {
        id: keys
        anchors.fill: parent
        focus: true

        Keys.onPressed: function(event) {
            if (KeyMap.hasCommandModifier(event))
                return;
            const digit = KeyMap.numpadDigit(event);
            const t = event.text;
            if (digit !== "") scientific.pressKey(digit);
            else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter || t === "=") scientific.pressKey("=");
            else if (event.key === Qt.Key_Backspace) scientific.pressKey("backspace");
            else if (event.key === Qt.Key_Escape) scientific.pressKey("clear");
            else if (event.key === Qt.Key_Delete) scientific.pressKey("clearEntry");
            else if (event.key === Qt.Key_F9) scientific.pressKey("sign");
            else if (t === "," || t === ".") scientific.pressKey(".");
            else if (/^[0-9+\-*/^()]$/.test(t)) scientific.pressKey(t);
            else if (t === "x" || t === "X") scientific.pressKey("*");
            else if (t === "%") scientific.pressKey("mod");
            else if (t === "!") scientific.applyFunction("factorial");
            else if (t === "s") scientific.applyFunction("sin");
            else if (t === "o") scientific.applyFunction("cos");
            else if (t === "t") scientific.applyFunction("tan");
            else if (t === "S") scientific.applyFunction("asin");
            else if (t === "O") scientific.applyFunction("acos");
            else if (t === "T") scientific.applyFunction("atan");
            else if (t === "l") scientific.applyFunction("log");
            else if (t === "n") scientific.applyFunction("ln");
            else if (t === "q") scientific.applyFunction("square");
            else if (t === "#") scientific.applyFunction("cube");
            else if (t === "r") scientific.applyFunction("reciprocal");
            else if (t === "@") scientific.applyFunction("sqrt");
            else if (t === "y") scientific.pressKey("root");
            else if (t === "p") scientific.pressKey("pi");
            else if (t === "e") scientific.pressKey("e");
            else if (t === "E") scientific.pressKey("exp");
            else if (t === "|") scientific.applyFunction("abs");
            else return;
            event.accepted = true;
        }
    }

    component MenuButton: Rectangle {
        id: menuButton
        property string text
        property bool open: false
        signal clicked()
        Layout.fillWidth: true
        Layout.fillHeight: true
        radius: Math.min(4, page.ui.radius)
        color: open ? page.ui.mix(page.ui.pageColor, page.ui.inkColor, 0.14)
            : (hit.containsMouse ? page.ui.mix(page.ui.pageColor, page.ui.inkColor, 0.07) : "transparent")
        Text {
            anchors.centerIn: parent
            width: parent.width - 4
            horizontalAlignment: Text.AlignHCenter
            fontSizeMode: Text.HorizontalFit
            minimumPixelSize: 8
            text: menuButton.text
            color: page.ui.inkColor
            font.family: "iA Writer Mono S"
            font.pixelSize: page.ui.px(13)
        }
        MouseArea {
            id: hit
            anchors.fill: parent
            hoverEnabled: true
            onClicked: menuButton.clicked()
        }
        Accessible.role: Accessible.ButtonMenu
        Accessible.name: text
        Accessible.onPressAction: clicked()
    }

    // A grid of keys that floats over the keypad, closed by any choice.
    component KeyMenu: Popup {
        id: menu
        property alias model: menuKeys.model
        property int columns: 3
        padding: page.ui.px(8)
        modal: false
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        onClosed: page.activate()
        background: Rectangle {
            color: page.ui.mix(page.ui.pageColor, page.ui.inkColor, 0.04)
            border.width: 1
            border.color: page.ui.lineColor
            radius: page.ui.radius
        }
        contentItem: GridLayout {
            columns: menu.columns
            rowSpacing: page.ui.px(6)
            columnSpacing: page.ui.px(6)
            Repeater {
                id: menuKeys
                CalcButton {
                    Layout.fillWidth: true
                    Layout.preferredHeight: page.ui.px(40)
                    Layout.fillHeight: false
                    Layout.preferredWidth: page.ui.px(64)
                    label: modelData.label
                    kind: modelData.latch ? "operator" : "function"
                    checked: modelData.latch === "second" ? page.trigSecond
                        : (modelData.latch === "hyp" ? page.hyperbolic : false)
                    accessibleName: modelData.name || ""
                    pageColor: page.ui.pageColor
                    inkColor: page.ui.inkColor
                    onActivated: {
                        if (modelData.latch === "second") {
                            page.trigSecond = !page.trigSecond;
                            return;
                        }
                        if (modelData.latch === "hyp") {
                            page.hyperbolic = !page.hyperbolic;
                            return;
                        }
                        if (modelData.trig) page.trig(modelData.trig);
                        else if (modelData.fn) page.apply(modelData.fn);
                        else page.press(modelData.key);
                        menu.close();
                    }
                }
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: page.ui.px(6)

        CalcDisplay {
            ui: page.ui
            Layout.fillWidth: true
            Layout.preferredHeight: Math.round(page.height * 0.19)
            Layout.leftMargin: page.ui.px(8)
            Layout.rightMargin: page.ui.px(8)
            expression: scientific.expression
            display: scientific.display
            errored: scientific.errored
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: page.ui.px(28)
            Layout.fillHeight: false
            spacing: page.ui.px(4)
            MenuButton {
                text: page.angleNames[scientific.angleUnit]
                onClicked: page.press("angle")
                Accessible.name: qsTr("Angle unit")
            }
            MenuButton {
                text: "F-E"
                open: scientific.scientificNotation
                onClicked: page.press("fe")
                Accessible.name: qsTr("Scientific notation")
            }
            MenuButton {
                text: qsTr("Trig") + " ▾"
                open: trigMenu.opened
                onClicked: trigMenu.open()
            }
            MenuButton {
                text: qsTr("Func") + " ▾"
                open: functionMenu.opened
                onClicked: functionMenu.open()
            }
        }

        MemoryRow {
            ui: page.ui
            target: page
            Layout.fillWidth: true
            Layout.preferredHeight: page.ui.px(26)
            Layout.fillHeight: false
            onShowMemory: page.showMemory()
        }

        GridLayout {
            id: keypad
            Layout.fillWidth: true
            Layout.fillHeight: true
            columns: 5
            rowSpacing: page.ui.px(5)
            columnSpacing: page.ui.px(5)

            Repeater {
                model: [
                    { label: "2nd", latch: true, name: qsTr("Second functions") },
                    { label: "π", key: "pi", name: qsTr("Pi") },
                    { label: "e", key: "e", name: qsTr("Euler's number") },
                    { label: "C", key: "clear" },
                    { label: "", key: "backspace", icon: "backspace" },

                    page.second ? { label: "x³", fn: "cube", name: qsTr("Cube") } : { label: "x²", fn: "square", name: qsTr("Square") },
                    { label: "1/x", fn: "reciprocal", name: qsTr("Reciprocal") },
                    { label: "|x|", fn: "abs", name: qsTr("Absolute value") },
                    { label: "exp", key: "exp", name: qsTr("Exponent") },
                    { label: "mod", key: "mod", name: qsTr("Modulo") },

                    page.second ? { label: "∛x", fn: "cbrt", name: qsTr("Cube root") } : { label: "√x", fn: "sqrt", name: qsTr("Square root") },
                    { label: "(", key: "(", badge: true },
                    { label: ")", key: ")" },
                    { label: "n!", fn: "factorial", name: qsTr("Factorial") },
                    { label: "÷", key: "÷", kind: "operator" },

                    page.second ? { label: "ʸ√x", key: "root", name: qsTr("Y root of x") } : { label: "xʸ", key: "^", name: qsTr("X to the power of y") },
                    { label: "7", key: "7", kind: "number" },
                    { label: "8", key: "8", kind: "number" },
                    { label: "9", key: "9", kind: "number" },
                    { label: "×", key: "×", kind: "operator" },

                    page.second ? { label: "2ˣ", fn: "twoPow", name: qsTr("Two to the power of x") } : { label: "10ˣ", fn: "tenPow", name: qsTr("Ten to the power of x") },
                    { label: "4", key: "4", kind: "number" },
                    { label: "5", key: "5", kind: "number" },
                    { label: "6", key: "6", kind: "number" },
                    { label: "−", key: "-", kind: "operator" },

                    page.second ? { label: "logᵧx", key: "logb", name: qsTr("Log base y of x") } : { label: "log", fn: "log", name: qsTr("Log") },
                    { label: "1", key: "1", kind: "number" },
                    { label: "2", key: "2", kind: "number" },
                    { label: "3", key: "3", kind: "number" },
                    { label: "+", key: "+", kind: "operator" },

                    page.second ? { label: "eˣ", fn: "ePow", name: qsTr("E to the power of x") } : { label: "ln", fn: "ln", name: qsTr("Natural log") },
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
                    label: modelData.badge && scientific.openParens > 0
                        ? "(" + "₀₁₂₃₄₅₆₇₈₉".charAt(Math.min(9, scientific.openParens)) : modelData.label
                    kind: modelData.kind || "function"
                    checked: modelData.latch === true && page.second
                    iconName: modelData.icon || ""
                    accessibleName: modelData.name || ""
                    pageColor: page.ui.pageColor
                    inkColor: page.ui.inkColor
                    onActivated: {
                        if (modelData.latch) {
                            // Toggling rebuilds the keypad, this key included,
                            // so nothing may run in this handler after it.
                            page.activate();
                            page.second = !page.second;
                        } else if (modelData.fn) {
                            page.apply(modelData.fn);
                        } else {
                            page.press(modelData.key);
                        }
                    }
                }
            }
        }
    }

    KeyMenu {
        id: trigMenu
        columns: 4
        x: page.width - width
        y: keypad.y
        model: [
            { label: "2nd", latch: "second", name: qsTr("Inverse functions") },
            { label: page.trigLabel("sin"), trig: "sin" },
            { label: page.trigLabel("cos"), trig: "cos" },
            { label: page.trigLabel("tan"), trig: "tan" },
            { label: "hyp", latch: "hyp", name: qsTr("Hyperbolic functions") },
            { label: page.trigLabel("sec"), trig: "sec" },
            { label: page.trigLabel("csc"), trig: "csc" },
            { label: page.trigLabel("cot"), trig: "cot" }
        ]
    }

    KeyMenu {
        id: functionMenu
        columns: 3
        x: page.width - width
        y: keypad.y
        model: [
            { label: "|x|", fn: "abs", name: qsTr("Absolute value") },
            { label: "⌊x⌋", fn: "floor", name: qsTr("Floor") },
            { label: "⌈x⌉", fn: "ceil", name: qsTr("Ceiling") },
            { label: "rand", key: "rand", name: qsTr("Random number") },
            { label: "→dms", fn: "dms", name: qsTr("Degrees to degrees, minutes, seconds") },
            { label: "→deg", fn: "deg", name: qsTr("Degrees, minutes, seconds to degrees") }
        ]
    }
}
