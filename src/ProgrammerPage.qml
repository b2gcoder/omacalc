import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import "keys.js" as KeyMap

// Programmer: the value in hex, decimal, octal and binary at once, a word
// size, bitwise operators and shifts, and a keypad that can switch to the
// individual bits. The work is in ProgrammerCalc (exposed as `programmer`).
FocusScope {
    id: page

    property var ui
    property bool showBits: false

    readonly property var wordNames: ({ 64: "QWORD", 32: "DWORD", 16: "WORD", 8: "BYTE" })
    readonly property var shiftNames: [qsTr("Arithmetic shift"), qsTr("Logical shift"),
                                       qsTr("Rotate circular"), qsTr("Rotate through carry")]

    function activate() { keys.forceActiveFocus(); }
    function copy() { programmer.copyResult(); }
    function paste() { programmer.paste(); }
    function press(key) {
        programmer.pressKey(key);
        activate();
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
            if (digit !== "" && digit !== ".") programmer.pressKey(digit);
            else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter || t === "=") programmer.pressKey("=");
            else if (event.key === Qt.Key_Backspace) programmer.pressKey("backspace");
            else if (event.key === Qt.Key_Escape) programmer.pressKey("clear");
            else if (event.key === Qt.Key_Delete) programmer.pressKey("clearEntry");
            else if (event.key === Qt.Key_F9) programmer.pressKey("sign");
            else if (event.key === Qt.Key_F5) programmer.base = 16;
            else if (event.key === Qt.Key_F6) programmer.base = 10;
            else if (event.key === Qt.Key_F7) programmer.base = 8;
            else if (event.key === Qt.Key_F8) programmer.base = 2;
            else if (event.key === Qt.Key_F12) programmer.wordBits = 64;
            else if (event.key === Qt.Key_F2) programmer.wordBits = 32;
            else if (event.key === Qt.Key_F3) programmer.wordBits = 16;
            else if (event.key === Qt.Key_F4) programmer.wordBits = 8;
            else if (/^[0-9a-fA-F]$/.test(t)) programmer.pressKey(t);
            else if (/^[+\-*/%()]$/.test(t)) programmer.pressKey(t);
            else if (t === "&") programmer.pressKey("and");
            else if (t === "|") programmer.pressKey("or");
            else if (t === "^") programmer.pressKey("xor");
            else if (t === "~") programmer.pressKey("not");
            else if (t === "<") programmer.pressKey("lsh");
            else if (t === ">") programmer.pressKey("rsh");
            else if (t === "x" || t === "X") programmer.pressKey("*");
            else return;
            event.accepted = true;
        }
    }

    component SmallKey: Rectangle {
        id: small
        property string text
        property bool active: false
        signal clicked()
        Layout.fillWidth: true
        Layout.fillHeight: true
        radius: Math.min(4, page.ui.radius)
        color: active ? page.ui.mix(page.ui.pageColor, page.ui.inkColor, 0.14)
            : (hit.containsMouse ? page.ui.mix(page.ui.pageColor, page.ui.inkColor, 0.07) : "transparent")
        Text {
            anchors.centerIn: parent
            width: parent.width - 4
            horizontalAlignment: Text.AlignHCenter
            fontSizeMode: Text.HorizontalFit
            minimumPixelSize: 8
            text: small.text
            color: page.ui.inkColor
            font.family: "iA Writer Mono S"
            font.pixelSize: page.ui.px(13)
        }
        MouseArea {
            id: hit
            anchors.fill: parent
            hoverEnabled: true
            onClicked: small.clicked()
        }
        Accessible.role: Accessible.Button
        Accessible.name: text
        Accessible.onPressAction: clicked()
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: page.ui.px(5)

        CalcDisplay {
            ui: page.ui
            Layout.fillWidth: true
            Layout.preferredHeight: Math.round(page.height * 0.14)
            Layout.leftMargin: page.ui.px(8)
            Layout.rightMargin: page.ui.px(8)
            expression: programmer.expression
            display: programmer.display
            errored: programmer.errored
            badge: programmer.shiftMode === 3 && programmer.carry ? qsTr("carry") : ""
        }

        // Every base at once; the selected one is what the keypad types.
        Repeater {
            model: [
                { name: "HEX", base: 16 },
                { name: "DEC", base: 10 },
                { name: "OCT", base: 8 },
                { name: "BIN", base: 2 }
            ]
            Rectangle {
                id: baseRow
                readonly property bool current: programmer.base === modelData.base
                Layout.fillWidth: true
                Layout.preferredHeight: page.ui.px(22)
                Layout.fillHeight: false
                color: rowHit.containsMouse && !current ? page.ui.mix(page.ui.pageColor, page.ui.inkColor, 0.05) : "transparent"
                radius: Math.min(4, page.ui.radius)
                Rectangle {
                    visible: baseRow.current
                    width: page.ui.px(3)
                    height: parent.height - page.ui.px(6)
                    anchors.verticalCenter: parent.verticalCenter
                    radius: width / 2
                    color: page.ui.accent
                }
                Text {
                    id: baseName
                    anchors.left: parent.left
                    anchors.leftMargin: page.ui.px(10)
                    anchors.verticalCenter: parent.verticalCenter
                    text: modelData.name
                    color: baseRow.current ? page.ui.inkColor : page.ui.mutedColor
                    font.family: "iA Writer Mono S"
                    font.bold: baseRow.current
                    font.pixelSize: page.ui.px(13)
                }
                Text {
                    anchors.left: baseName.right
                    anchors.leftMargin: page.ui.px(12)
                    anchors.right: parent.right
                    anchors.rightMargin: page.ui.px(4)
                    anchors.verticalCenter: parent.verticalCenter
                    text: modelData.base === 16 ? programmer.hex : modelData.base === 10 ? programmer.dec
                        : modelData.base === 8 ? programmer.oct : programmer.bin
                    color: page.ui.inkColor
                    elide: Text.ElideLeft
                    fontSizeMode: Text.HorizontalFit
                    minimumPixelSize: 7
                    font.family: "iA Writer Mono S"
                    font.pixelSize: page.ui.px(13)
                }
                MouseArea {
                    id: rowHit
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: { programmer.base = modelData.base; page.activate(); }
                }
                Accessible.role: Accessible.RadioButton
                Accessible.name: modelData.name
                Accessible.checked: current
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: page.ui.px(28)
            Layout.fillHeight: false
            spacing: page.ui.px(4)
            SmallKey {
                text: page.showBits ? qsTr("Keypad") : qsTr("Bits")
                onClicked: { page.showBits = !page.showBits; page.activate(); }
            }
            SmallKey {
                text: page.wordNames[programmer.wordBits]
                onClicked: page.press("word")
                Accessible.name: qsTr("Word size %1").arg(text)
            }
            SmallKey {
                text: qsTr("Bitwise") + " ▾"
                active: bitwiseMenu.opened
                onClicked: bitwiseMenu.open()
            }
            SmallKey {
                text: qsTr("Shift") + " ▾"
                active: shiftMenu.opened
                onClicked: shiftMenu.open()
            }
        }

        GridLayout {
            id: keypad
            visible: !page.showBits
            Layout.fillWidth: true
            Layout.fillHeight: true
            columns: 5
            rowSpacing: page.ui.px(5)
            columnSpacing: page.ui.px(5)

            Repeater {
                model: [
                    { label: "A", key: "A", kind: "number" },
                    { label: programmer.shiftMode >= 2 ? "RoL" : "<<", key: "lsh", name: qsTr("Shift left") },
                    { label: programmer.shiftMode >= 2 ? "RoR" : ">>", key: "rsh", name: qsTr("Shift right") },
                    { label: "C", key: "clear" },
                    { label: "", key: "backspace", icon: "backspace" },
                    { label: "B", key: "B", kind: "number" },
                    { label: "(", key: "(" },
                    { label: ")", key: ")" },
                    { label: "%", key: "%", name: qsTr("Remainder") },
                    { label: "÷", key: "/", kind: "operator" },
                    { label: "C", key: "C", kind: "number", name: qsTr("Hex digit C") },
                    { label: "7", key: "7", kind: "number" },
                    { label: "8", key: "8", kind: "number" },
                    { label: "9", key: "9", kind: "number" },
                    { label: "×", key: "*", kind: "operator" },
                    { label: "D", key: "D", kind: "number" },
                    { label: "4", key: "4", kind: "number" },
                    { label: "5", key: "5", kind: "number" },
                    { label: "6", key: "6", kind: "number" },
                    { label: "−", key: "-", kind: "operator" },
                    { label: "E", key: "E", kind: "number" },
                    { label: "1", key: "1", kind: "number" },
                    { label: "2", key: "2", kind: "number" },
                    { label: "3", key: "3", kind: "number" },
                    { label: "+", key: "+", kind: "operator" },
                    { label: "F", key: "F", kind: "number" },
                    { label: "±", key: "sign", kind: "number" },
                    { label: "0", key: "0", kind: "number" },
                    { label: "NOT", key: "not" },
                    { label: "=", key: "=", kind: "equals" }
                ]

                CalcButton {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.preferredWidth: 1
                    Layout.preferredHeight: 1
                    label: modelData.label
                    kind: modelData.kind || "function"
                    iconName: modelData.icon || ""
                    accessibleName: modelData.name || ""
                    enabled: modelData.kind !== "number" || modelData.key.length !== 1
                        // Reading base re-runs the binding when it changes.
                        || (programmer.base > 0 && programmer.digitAllowed(modelData.key))
                    pageColor: page.ui.pageColor
                    inkColor: page.ui.inkColor
                    onActivated: page.press(modelData.key)
                }
            }
        }

        // Bit toggling: every bit of the word, most significant first, in
        // rows of sixteen. Bits past the word size are dimmed.
        ColumnLayout {
            visible: page.showBits
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: page.ui.px(4)

            Repeater {
                model: 4
                ColumnLayout {
                    id: bitRow
                    readonly property int topBit: 63 - index * 16
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    spacing: 0
                    RowLayout {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        spacing: page.ui.px(10)
                        Repeater {
                            model: 4
                            RowLayout {
                                id: nibble
                                readonly property int topBit: bitRow.topBit - index * 4
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                spacing: page.ui.px(2)
                                Repeater {
                                    model: 4
                                    Rectangle {
                                        id: bitCell
                                        readonly property int bitIndex: nibble.topBit - index
                                        readonly property bool usable: bitIndex < programmer.wordBits
                                        // Reading hex makes the binding re-run whenever the value changes.
                                        readonly property bool on: programmer.hex.length >= 0 && programmer.bit(bitIndex)
                                        Layout.fillWidth: true
                                        Layout.fillHeight: true
                                        radius: Math.min(3, page.ui.radius)
                                        opacity: usable ? 1 : 0.3
                                        color: bitHit.containsMouse && usable
                                            ? page.ui.mix(page.ui.pageColor, page.ui.inkColor, 0.1) : "transparent"
                                        Text {
                                            anchors.centerIn: parent
                                            text: bitCell.on ? "1" : "0"
                                            color: bitCell.on ? page.ui.inkColor : page.ui.mutedColor
                                            font.family: "iA Writer Mono S"
                                            font.bold: bitCell.on
                                            font.pixelSize: page.ui.px(17)
                                        }
                                        MouseArea {
                                            id: bitHit
                                            anchors.fill: parent
                                            hoverEnabled: true
                                            enabled: bitCell.usable
                                            onClicked: { programmer.toggleBit(bitCell.bitIndex); page.activate(); }
                                        }
                                        Accessible.role: Accessible.CheckBox
                                        Accessible.name: qsTr("Bit %1").arg(bitIndex)
                                        Accessible.checked: on
                                    }
                                }
                            }
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Text {
                            text: bitRow.topBit
                            color: page.ui.mutedColor
                            font.family: "iA Writer Mono S"
                            font.pixelSize: page.ui.px(11)
                        }
                        Item { Layout.fillWidth: true }
                        Text {
                            text: bitRow.topBit - 15
                            color: page.ui.mutedColor
                            font.family: "iA Writer Mono S"
                            font.pixelSize: page.ui.px(11)
                        }
                    }
                }
            }
        }
    }

    Popup {
        id: bitwiseMenu
        x: page.width - width
        y: keypad.y
        padding: page.ui.px(8)
        focus: true
        onClosed: page.activate()
        background: Rectangle {
            color: page.ui.mix(page.ui.pageColor, page.ui.inkColor, 0.04)
            border.width: 1
            border.color: page.ui.lineColor
            radius: page.ui.radius
        }
        contentItem: GridLayout {
            columns: 3
            rowSpacing: page.ui.px(6)
            columnSpacing: page.ui.px(6)
            Repeater {
                model: ["AND", "OR", "NOT", "NAND", "NOR", "XOR"]
                CalcButton {
                    Layout.preferredWidth: page.ui.px(72)
                    Layout.preferredHeight: page.ui.px(40)
                    Layout.fillHeight: false
                    label: modelData
                    kind: "function"
                    pageColor: page.ui.pageColor
                    inkColor: page.ui.inkColor
                    onActivated: {
                        programmer.pressKey(modelData.toLowerCase());
                        bitwiseMenu.close();
                    }
                }
            }
        }
    }

    Popup {
        id: shiftMenu
        x: page.width - width
        y: keypad.y
        padding: page.ui.px(8)
        focus: true
        onClosed: page.activate()
        background: Rectangle {
            color: page.ui.mix(page.ui.pageColor, page.ui.inkColor, 0.04)
            border.width: 1
            border.color: page.ui.lineColor
            radius: page.ui.radius
        }
        contentItem: ColumnLayout {
            spacing: page.ui.px(2)
            Repeater {
                model: page.shiftNames
                RadioButton {
                    text: modelData
                    checked: programmer.shiftMode === index
                    font.family: "iA Writer Mono S"
                    font.pixelSize: page.ui.px(14)
                    onClicked: {
                        programmer.shiftMode = index;
                        shiftMenu.close();
                    }
                }
            }
        }
    }
}
