import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

// Date arithmetic: the span between two dates, a date moved forward or back,
// and facts about one date. Every result recomputes as you type; the work
// happens in DateCalc (exposed as `dates`).
Flickable {
    id: page

    property color pageColor
    property color inkColor
    property real uiScale: 1
    property string mode: "between" // "between", "shift" or "info"

    function mixColors(base, tint, amount) {
        return Qt.rgba(
            base.r + (tint.r - base.r) * amount,
            base.g + (tint.g - base.g) * amount,
            base.b + (tint.b - base.b) * amount, 1);
    }
    function px(pixels) { return Math.max(1, Math.round(pixels * uiScale)); }

    readonly property color mutedColor: mixColors(pageColor, inkColor, 0.5)
    readonly property color lineColor: mixColors(pageColor, inkColor, 0.16)
    readonly property color fieldColor: mixColors(pageColor, inkColor, 0.05)

    function focusFirstField() {
        if (mode === "between") fromField.forceFocus();
        else if (mode === "shift") startField.forceFocus();
        else infoField.forceFocus();
    }
    onModeChanged: Qt.callLater(focusFirstField)

    contentWidth: width
    contentHeight: column.implicitHeight + px(8)
    clip: true
    boundsBehavior: Flickable.StopAtBounds
    ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

    // ---- Shared pieces -------------------------------------------------

    component Label: Text {
        color: page.mutedColor
        font.family: "iA Writer Mono S"
        font.pixelSize: page.px(13)
    }

    component DateField: ColumnLayout {
        id: dateField
        property alias text: input.text
        property string caption
        readonly property var parsed: dates.describe(input.text)
        function forceFocus() { input.forceActiveFocus(); input.selectAll(); }
        spacing: page.px(4)
        Layout.fillWidth: true

        Label { text: dateField.caption }
        RowLayout {
            Layout.fillWidth: true
            spacing: page.px(8)
            Field {
                id: input
                Layout.fillWidth: true
                hint: "2026-11-14, 11/14/26, +30, -2w"
                // Tidy to ISO once the field is left with a date it understood.
                onEditingFinished: if (dateField.parsed.ok && !/^[+-]|today|now|tomorrow|yesterday/i.test(text.trim()))
                    text = dates.isoText(text)
            }
            SmallButton {
                text: qsTr("Today")
                onClicked: input.text = dates.todayText()
            }
        }
        Text {
            text: dateField.parsed.pretty || " "
            color: dateField.parsed.ok ? page.inkColor : Material.color(Material.Red)
            opacity: dateField.parsed.ok ? 0.8 : 1
            font.family: "iA Writer Mono S"
            font.pixelSize: page.px(13)
            Layout.fillWidth: true
            elide: Text.ElideRight
        }
    }

    component Field: TextField {
        id: field
        property string hint
        color: page.inkColor
        selectByMouse: true
        font.family: "iA Writer Mono S"
        font.pixelSize: page.px(16)
        leftPadding: page.px(10)
        rightPadding: page.px(10)
        topPadding: page.px(8)
        bottomPadding: page.px(8)
        Text {
            anchors.fill: parent
            anchors.leftMargin: field.leftPadding
            anchors.rightMargin: field.rightPadding
            verticalAlignment: Text.AlignVCenter
            horizontalAlignment: field.horizontalAlignment === TextInput.AlignRight ? Text.AlignRight : Text.AlignLeft
            visible: field.text.length === 0
            text: field.hint
            elide: Text.ElideRight
            color: page.mixColors(page.pageColor, page.inkColor, 0.35)
            font: field.font
        }
        background: Rectangle {
            radius: page.px(8)
            color: page.fieldColor
            border.width: 1
            border.color: parent.activeFocus ? Material.accent : page.lineColor
        }
    }

    component NumberField: ColumnLayout {
        property alias text: num.text
        property string caption
        readonly property int value: parseInt(num.text) || 0
        spacing: page.px(4)
        Layout.fillWidth: true
        Layout.preferredWidth: 1
        Label { text: parent.caption }
        Field {
            id: num
            Layout.fillWidth: true
            horizontalAlignment: TextInput.AlignRight
            hint: "0"
            inputMethodHints: Qt.ImhDigitsOnly
            validator: IntValidator { bottom: 0; top: 99999 }
        }
    }

    component SmallButton: Rectangle {
        id: small
        property string text
        property bool checked: false
        signal clicked()
        implicitWidth: caption.implicitWidth + page.px(20)
        implicitHeight: page.px(36)
        radius: page.px(8)
        color: checked ? page.inkColor
            : page.mixColors(page.pageColor, page.inkColor, hit.pressed ? 0.2 : (hit.containsMouse ? 0.12 : 0.08))
        Text {
            id: caption
            anchors.centerIn: parent
            text: small.text
            color: small.checked ? page.pageColor : page.inkColor
            font.family: "iA Writer Mono S"
            font.pixelSize: page.px(14)
        }
        MouseArea {
            id: hit
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: small.clicked()
        }
        Accessible.role: Accessible.Button
        Accessible.name: text
        Accessible.onPressAction: clicked()
    }

    component Check: CheckBox {
        id: check
        font.family: "iA Writer Mono S"
        font.pixelSize: page.px(14)
        padding: 0
        contentItem: Text {
            leftPadding: check.indicator.width + check.spacing
            text: check.text
            color: page.inkColor
            font: check.font
            verticalAlignment: Text.AlignVCenter
        }
    }

    // The answer: a large headline and a table of other ways to say it.
    component Result: ColumnLayout {
        id: result
        property var answer: ({})
        spacing: page.px(6)
        Layout.fillWidth: true
        Layout.topMargin: page.px(6)

        Rectangle { Layout.fillWidth: true; height: 1; color: page.lineColor }

        Text {
            Layout.fillWidth: true
            Layout.topMargin: page.px(6)
            text: result.answer.ok ? result.answer.headline : (result.answer.error || "")
            color: result.answer.ok ? page.inkColor : page.mutedColor
            wrapMode: Text.Wrap
            font.family: "iA Writer Mono S"
            font.pixelSize: page.px(result.answer.ok ? 26 : 16)
            font.bold: result.answer.ok === true
        }
        Repeater {
            model: result.answer.ok ? result.answer.rows : []
            RowLayout {
                Layout.fillWidth: true
                spacing: page.px(12)
                Label {
                    text: modelData.label
                    Layout.alignment: Qt.AlignTop
                    font.pixelSize: page.px(14)
                }
                Text {
                    Layout.fillWidth: true
                    text: modelData.value
                    color: page.inkColor
                    wrapMode: Text.Wrap
                    horizontalAlignment: Text.AlignRight
                    font.family: "iA Writer Mono S"
                    font.pixelSize: page.px(14)
                }
            }
        }
        SmallButton {
            visible: result.answer.ok === true
            Layout.alignment: Qt.AlignRight
            Layout.topMargin: page.px(4)
            text: qsTr("Copy")
            onClicked: {
                var lines = [result.answer.headline];
                for (var i = 0; i < result.answer.rows.length; ++i)
                    lines.push(result.answer.rows[i].label + ": " + result.answer.rows[i].value);
                backend.copyText(lines.join("\n"));
            }
        }
    }

    // ---- Layout --------------------------------------------------------

    ColumnLayout {
        id: column
        width: page.width - page.px(12)
        spacing: page.px(14)

        RowLayout {
            Layout.fillWidth: true
            spacing: page.px(8)
            SmallButton {
                Layout.fillWidth: true
                text: qsTr("Between")
                checked: page.mode === "between"
                onClicked: page.mode = "between"
            }
            SmallButton {
                Layout.fillWidth: true
                text: qsTr("Add / sub")
                checked: page.mode === "shift"
                onClicked: page.mode = "shift"
            }
            SmallButton {
                Layout.fillWidth: true
                text: qsTr("Date info")
                checked: page.mode === "info"
                onClicked: page.mode = "info"
            }
        }

        // Time between two dates ("days since" is just To = today).
        ColumnLayout {
            visible: page.mode === "between"
            Layout.fillWidth: true
            spacing: page.px(10)

            DateField { id: fromField; objectName: "fromInput"; caption: qsTr("From") }
            RowLayout {
                Layout.fillWidth: true
                Check { id: inclusive; text: qsTr("Include end day") }
                Item { Layout.fillWidth: true }
                SmallButton {
                    text: "⇅ " + qsTr("Swap")
                    onClicked: { var t = fromField.text; fromField.text = toField.text; toField.text = t; }
                }
            }
            DateField { id: toField; caption: qsTr("To"); text: dates.todayText() }
            Result { answer: dates.difference(fromField.text, toField.text, inclusive.checked) }
        }

        // Add or subtract years, months, weeks and days.
        ColumnLayout {
            visible: page.mode === "shift"
            Layout.fillWidth: true
            spacing: page.px(10)

            DateField { id: startField; caption: qsTr("Start"); text: dates.todayText() }
            RowLayout {
                Layout.fillWidth: true
                spacing: page.px(8)
                SmallButton {
                    Layout.fillWidth: true
                    text: "+ " + qsTr("Add")
                    checked: !subtract.on
                    onClicked: subtract.on = false
                }
                SmallButton {
                    id: subtract
                    property bool on: false
                    Layout.fillWidth: true
                    text: "− " + qsTr("Subtract")
                    checked: on
                    onClicked: on = true
                }
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: page.px(8)
                NumberField { id: yearsField; caption: qsTr("Years") }
                NumberField { id: monthsField; caption: qsTr("Months") }
                NumberField { id: weeksField; caption: qsTr("Weeks") }
                NumberField { id: daysField; objectName: "daysInput"; caption: qsTr("Days") }
            }
            Check { id: business; text: qsTr("Days are business days") }
            Result {
                answer: dates.shift(startField.text, subtract.on, yearsField.value, monthsField.value,
                                  weeksField.value, daysField.value, business.checked)
            }
        }

        // Everything about one date.
        ColumnLayout {
            visible: page.mode === "info"
            Layout.fillWidth: true
            spacing: page.px(10)

            DateField { id: infoField; objectName: "infoInput"; caption: qsTr("Date"); text: dates.todayText() }
            Result { answer: dates.info(infoField.text) }
        }
    }
}
