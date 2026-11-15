import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import "keys.js" as KeyMap

// One page for every converter category. The value being typed is in the
// active field (click either one to type there); the other follows. Below
// is a keypad, or the value in every unit of the category.
FocusScope {
    id: page

    property var ui
    property string category: "length"
    property string fromUnit
    property string toUnit
    property string fromText: "1"
    property string toText
    property bool editingTo: false
    // The shown value is a starting point: the next digit replaces it.
    property bool fresh: true
    property bool showAll: false
    // Unit choices per category, kept while the window is open.
    property var remembered: ({})

    readonly property var units: converter.currencyReady || category !== "currency"
        ? converter.units(category) : []
    readonly property bool unavailable: category === "currency" && !converter.currencyReady

    function activate() { keys.forceActiveFocus(); }
    function copy() { backend.copyText((editingTo ? fromText : toText).replace(/,/g, "").replace("−", "-")); }
    // Pastes a number into the active field, keeping it grouped like typing.
    function paste() {
        const text = backend.clipboardText().trim().replace(/,/g, "").replace("−", "-");
        if (!/^-?\d*\.?\d+$/.test(text) && !/^-?\d+\.?$/.test(text))
            return;
        fresh = true;
        if (text.startsWith("-")) {
            for (const c of text.slice(1)) type(c);
            type("sign");
        } else {
            for (const c of text) type(c);
        }
    }

    function recompute() {
        if (editingTo)
            fromText = converter.convertText(category, toUnit, fromUnit, toText);
        else
            toText = converter.convertText(category, fromUnit, toUnit, fromText);
    }

    function loadCategory() {
        const saved = remembered[category];
        const pair = saved || converter.defaultUnits(category);
        // The binding passes through an empty category while the mode changes.
        if (!pair || pair.length < 2)
            return;
        fromUnit = pair[0];
        toUnit = pair[1];
        editingTo = false;
        fromText = "1";
        fresh = true;
        if (category === "currency")
            converter.refreshCurrency(false);
        recompute();
    }
    onCategoryChanged: loadCategory()
    Component.onCompleted: loadCategory()

    function remember() {
        const next = Object.assign({}, remembered);
        next[category] = [fromUnit, toUnit];
        remembered = next;
    }

    Connections {
        target: converter
        function onCurrencyChanged() {
            if (page.category === "currency")
                page.recompute();
        }
    }

    // Typing edits the active field's text as plain digits.
    function type(key) {
        let text = editingTo ? toText : fromText;
        let plain = text.replace(/,/g, "");
        if (fresh && (key === "." || /^[0-9]$/.test(key)))
            plain = "";
        fresh = false;
        if (key === "backspace") {
            plain = plain.slice(0, -1);
            if (plain === "−" || plain === "-")
                plain = "";
        } else if (key === "clear") {
            plain = "0";
        } else if (key === "sign") {
            plain = plain.startsWith("−") ? plain.slice(1) : (plain === "" || plain === "0" ? plain : "−" + plain);
        } else if (key === ".") {
            if (plain.indexOf(".") < 0)
                plain = (plain === "" ? "0" : plain) + ".";
        } else {
            if (plain.replace(/[^0-9]/g, "").length >= 15)
                return;
            plain = (plain === "0" ? "" : (plain === "−0" ? "−" : plain)) + key;
        }
        if (plain === "")
            plain = "0";
        // Group the whole part as it's typed: 1234567 → 1,234,567.
        const negative = plain.startsWith("−");
        const body = negative ? plain.slice(1) : plain;
        const dot = body.indexOf(".");
        const whole = dot < 0 ? body : body.slice(0, dot);
        const grouped = whole.replace(/\B(?=(\d{3})+(?!\d))/g, ",") + (dot < 0 ? "" : body.slice(dot));
        const result = (negative ? "−" : "") + grouped;
        if (editingTo)
            toText = result;
        else
            fromText = result;
        recompute();
    }

    function swap() {
        const unit = fromUnit;
        fromUnit = toUnit;
        toUnit = unit;
        remember();
        recompute();
    }

    Item {
        id: keys
        anchors.fill: parent
        focus: true
        Keys.onPressed: function(event) {
            if (KeyMap.hasCommandModifier(event))
                return;
            const digit = KeyMap.numpadDigit(event);
            if (digit !== "") page.type(digit);
            else if (/^[0-9]$/.test(event.text)) page.type(event.text);
            else if (event.text === "." || event.text === ",") page.type(".");
            else if (event.text === "-" || event.key === Qt.Key_F9) page.type("sign");
            else if (event.key === Qt.Key_Backspace) page.type("backspace");
            else if (event.key === Qt.Key_Escape || event.key === Qt.Key_Delete) page.type("clear");
            else if (event.key === Qt.Key_Tab || event.key === Qt.Key_Up || event.key === Qt.Key_Down) {
                page.editingTo = !page.editingTo;
                page.fresh = true;
            }
            else if (event.text === "s" || event.text === "S") page.swap();
            else return;
            event.accepted = true;
        }
    }

    component ValueField: Rectangle {
        id: field
        property bool active
        property string value
        property string unitId
        property string symbol
        signal chosen()
        signal unitPicked(string id)
        Layout.fillWidth: true
        Layout.preferredHeight: page.ui.px(96)
        Layout.fillHeight: false
        radius: page.ui.radius
        color: active ? page.ui.mix(page.ui.pageColor, page.ui.inkColor, 0.06) : "transparent"
        border.width: 1
        border.color: active ? page.ui.accent : page.ui.lineColor

        MouseArea {
            anchors.fill: parent
            onClicked: { field.chosen(); page.activate(); }
        }
        Text {
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.margins: page.ui.px(10)
            height: page.ui.px(44)
            text: field.value === "" ? "—" : field.value
            color: page.ui.inkColor
            font.family: "iA Writer Mono S"
            font.bold: field.active
            font.pixelSize: page.ui.px(34)
            fontSizeMode: Text.HorizontalFit
            minimumPixelSize: page.ui.px(12)
            verticalAlignment: Text.AlignVCenter
        }
        ComboBox {
            id: picker
            anchors.left: parent.left
            anchors.bottom: parent.bottom
            anchors.leftMargin: page.ui.px(4)
            anchors.bottomMargin: page.ui.px(2)
            width: Math.min(parent.width - page.ui.px(8), page.ui.px(300))
            height: page.ui.px(40)
            model: page.units
            textRole: "name"
            valueRole: "id"
            font.family: "iA Writer Mono S"
            font.pixelSize: page.ui.px(14)
            currentIndex: {
                for (let i = 0; i < page.units.length; ++i)
                    if (page.units[i].id === field.unitId)
                        return i;
                return -1;
            }
            onActivated: function(index) {
                field.unitPicked(page.units[index].id);
                page.activate();
            }
            Accessible.name: qsTr("Unit")
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: page.ui.px(8)

        Text {
            visible: page.unavailable
            Layout.fillWidth: true
            Layout.topMargin: page.ui.px(20)
            text: converter.currencyStatus || qsTr("Loading exchange rates…")
            wrapMode: Text.Wrap
            color: page.ui.inkColor
            font.family: "iA Writer Mono S"
            font.pixelSize: page.ui.px(15)
        }

        ValueField {
            visible: !page.unavailable
            active: !page.editingTo
            value: page.fromText
            unitId: page.fromUnit
            onChosen: { page.editingTo = false; page.fresh = true; }
            onUnitPicked: function(id) { page.fromUnit = id; page.remember(); page.recompute(); }
        }

        RowLayout {
            visible: !page.unavailable
            Layout.fillWidth: true
            spacing: page.ui.px(8)
            Text {
                Layout.fillWidth: true
                text: page.category === "currency" ? converter.rateLine(page.fromUnit, page.toUnit) : ""
                color: page.ui.mutedColor
                elide: Text.ElideRight
                font.family: "iA Writer Mono S"
                font.pixelSize: page.ui.px(12)
            }
            Button {
                text: "⇅ " + qsTr("Swap")
                flat: true
                font.family: "iA Writer Mono S"
                font.pixelSize: page.ui.px(13)
                onClicked: { page.swap(); page.activate(); }
            }
        }

        ValueField {
            visible: !page.unavailable
            active: page.editingTo
            value: page.toText
            unitId: page.toUnit
            onChosen: { page.editingTo = true; page.fresh = true; }
            onUnitPicked: function(id) { page.toUnit = id; page.remember(); page.recompute(); }
        }

        RowLayout {
            visible: page.category === "currency"
            Layout.fillWidth: true
            Text {
                Layout.fillWidth: true
                text: converter.currencyReady ? converter.currencyStatus : ""
                color: page.ui.mutedColor
                wrapMode: Text.Wrap
                font.family: "iA Writer Mono S"
                font.pixelSize: page.ui.px(11)
            }
            Button {
                text: qsTr("Update rates")
                flat: true
                enabled: !converter.currencyLoading
                font.family: "iA Writer Mono S"
                font.pixelSize: page.ui.px(12)
                onClicked: { converter.refreshCurrency(true); page.activate(); }
            }
        }

        RowLayout {
            visible: !page.unavailable
            Layout.fillWidth: true
            spacing: page.ui.px(4)
            Repeater {
                model: [{ all: false, label: qsTr("Keypad") }, { all: true, label: qsTr("All units") }]
                Rectangle {
                    readonly property bool current: page.showAll === modelData.all
                    Layout.fillWidth: true
                    Layout.preferredHeight: page.ui.px(26)
                    Layout.fillHeight: false
                    radius: Math.min(4, page.ui.radius)
                    color: current ? page.ui.mix(page.ui.pageColor, page.ui.inkColor, 0.14) : "transparent"
                    Text {
                        anchors.centerIn: parent
                        text: modelData.label
                        color: parent.current ? page.ui.inkColor : page.ui.mutedColor
                        font.family: "iA Writer Mono S"
                        font.pixelSize: page.ui.px(13)
                    }
                    MouseArea {
                        anchors.fill: parent
                        onClicked: { page.showAll = modelData.all; page.activate(); }
                    }
                    Accessible.role: Accessible.PageTab
                    Accessible.name: modelData.label
                }
            }
        }

        GridLayout {
            visible: !page.showAll && !page.unavailable
            Layout.fillWidth: true
            Layout.fillHeight: true
            columns: 3
            rowSpacing: page.ui.px(5)
            columnSpacing: page.ui.px(5)
            Repeater {
                model: [
                    { label: "CE", key: "clear", kind: "function" },
                    { label: "", key: "backspace", kind: "function", icon: "backspace" },
                    { label: "±", key: "sign", kind: "function" },
                    { label: "7", key: "7" }, { label: "8", key: "8" }, { label: "9", key: "9" },
                    { label: "4", key: "4" }, { label: "5", key: "5" }, { label: "6", key: "6" },
                    { label: "1", key: "1" }, { label: "2", key: "2" }, { label: "3", key: "3" },
                    { label: "⇅", key: "swap", kind: "function", name: qsTr("Swap units") },
                    { label: "0", key: "0" }, { label: ".", key: "." }
                ]
                CalcButton {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.preferredWidth: 1
                    Layout.preferredHeight: 1
                    label: modelData.label
                    kind: modelData.kind || "number"
                    iconName: modelData.icon || ""
                    accessibleName: modelData.name || ""
                    pageColor: page.ui.pageColor
                    inkColor: page.ui.inkColor
                    onActivated: {
                        if (modelData.key === "swap") page.swap();
                        else page.type(modelData.key);
                        page.activate();
                    }
                }
            }
        }

        ListView {
            visible: page.showAll && !page.unavailable
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar {}
            model: converter.allConversions(page.category, page.editingTo ? page.toUnit : page.fromUnit,
                                            page.editingTo ? page.toText : page.fromText)
            delegate: Item {
                width: ListView.view.width
                height: page.ui.px(30)
                Text {
                    anchors.left: parent.left
                    anchors.right: valueText.left
                    anchors.rightMargin: page.ui.px(8)
                    anchors.verticalCenter: parent.verticalCenter
                    text: modelData.name
                    color: page.ui.mutedColor
                    elide: Text.ElideRight
                    font.family: "iA Writer Mono S"
                    font.pixelSize: page.ui.px(13)
                }
                Text {
                    id: valueText
                    anchors.right: parent.right
                    anchors.rightMargin: page.ui.px(10)
                    anchors.verticalCenter: parent.verticalCenter
                    width: Math.min(implicitWidth, parent.width * 0.6)
                    horizontalAlignment: Text.AlignRight
                    text: modelData.value + " " + modelData.symbol
                    color: page.ui.inkColor
                    elide: Text.ElideLeft
                    font.family: "iA Writer Mono S"
                    font.pixelSize: page.ui.px(13)
                }
            }
        }
    }
}
