import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

// A small calendar that drops down from a date field. Days of one month, or
// with the title clicked, the months of a year, then a span of years, so a
// date decades away is a few clicks off. Dates go in and out as ISO text
// (2026-11-15), the form DateCalc reads.
Popup {
    id: picker

    property color pageColor
    property color inkColor
    property real uiScale: 1
    property string iso
    signal picked(string iso)

    property int view: 0 // 0 days, 1 months, 2 years
    property int shownYear: 2026
    property int shownMonth: 0 // 0–11
    property var cursor: new Date()

    function px(pixels) { return Math.max(1, Math.round(pixels * uiScale)); }
    function mix(base, tint, amount) {
        return Qt.rgba(base.r + (tint.r - base.r) * amount,
                       base.g + (tint.g - base.g) * amount,
                       base.b + (tint.b - base.b) * amount, 1);
    }
    function parseIso(text) {
        const match = /^(\d{4})-(\d{2})-(\d{2})$/.exec(text || "");
        return match ? new Date(+match[1], +match[2] - 1, +match[3]) : null;
    }
    function toIso(date) {
        const pad = (n, width) => String(n).padStart(width, "0");
        return pad(date.getFullYear(), 4) + "-" + pad(date.getMonth() + 1, 2) + "-" + pad(date.getDate(), 2);
    }
    function sameDay(a, b) {
        return a && b && a.getFullYear() === b.getFullYear() && a.getMonth() === b.getMonth()
            && a.getDate() === b.getDate();
    }
    function moveCursor(date) {
        cursor = date;
        shownYear = date.getFullYear();
        shownMonth = date.getMonth();
    }
    function choose(date) {
        picked(toIso(date));
        close();
    }
    // Months keep the day where they can: Jan 31 + 1 month is Feb 28.
    function addMonths(date, count) {
        const target = new Date(date.getFullYear(), date.getMonth() + count, 1);
        const last = new Date(target.getFullYear(), target.getMonth() + 1, 0).getDate();
        return new Date(target.getFullYear(), target.getMonth(), Math.min(date.getDate(), last));
    }
    function step(direction) {
        if (view === 0) {
            const shown = new Date(shownYear, shownMonth + direction, 1);
            shownYear = shown.getFullYear();
            shownMonth = shown.getMonth();
        } else if (view === 1) {
            shownYear += direction;
        } else {
            shownYear += direction * 12;
        }
    }

    readonly property var selected: parseIso(iso)
    readonly property var today: new Date()
    readonly property int firstDay: Qt.locale().firstDayOfWeek // 0 Sunday … 6 Saturday
    readonly property int decadeStart: Math.floor(shownYear / 12) * 12

    onAboutToShow: {
        view = 0;
        moveCursor(selected || new Date());
    }
    onOpened: keys.forceActiveFocus()

    modal: false
    focus: true
    margins: px(8)
    padding: px(10)
    width: px(300)
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutsideParent

    background: Rectangle {
        color: picker.mix(picker.pageColor, picker.inkColor, 0.05)
        border.width: 1
        border.color: picker.mix(picker.pageColor, picker.inkColor, 0.18)
        radius: picker.px(8)
    }

    component HeaderButton: Rectangle {
        id: headerButton
        property string text
        property string name
        signal clicked()
        implicitWidth: Math.max(picker.px(32), headerText.implicitWidth + picker.px(16))
        implicitHeight: picker.px(32)
        radius: picker.px(6)
        color: hit.containsMouse ? picker.mix(picker.pageColor, picker.inkColor, 0.1) : "transparent"
        Text {
            id: headerText
            anchors.centerIn: parent
            text: headerButton.text
            color: picker.inkColor
            font.family: "iA Writer Mono S"
            font.bold: true
            font.pixelSize: picker.px(15)
        }
        MouseArea { id: hit; anchors.fill: parent; hoverEnabled: true; onClicked: headerButton.clicked() }
        Accessible.role: Accessible.Button
        Accessible.name: name || text
        Accessible.onPressAction: clicked()
    }

    // One cell of any view: a day, a month, or a year.
    component Cell: Rectangle {
        id: cell
        property string text
        property bool chosen: false
        property bool current: false   // today, this month, or this year
        property bool focused: false   // the keyboard cursor
        property bool outside: false   // a day of the neighbouring month
        property string name
        signal clicked()
        Layout.fillWidth: true
        Layout.fillHeight: true
        radius: picker.px(6)
        color: chosen ? picker.inkColor
            : (cellHit.containsMouse ? picker.mix(picker.pageColor, picker.inkColor, 0.12) : "transparent")
        border.width: current && !chosen ? 1.5 : (focused ? 1 : 0)
        border.color: current ? Material.accent : picker.mix(picker.pageColor, picker.inkColor, 0.5)
        Text {
            anchors.centerIn: parent
            text: cell.text
            color: cell.chosen ? picker.pageColor
                : (cell.outside ? picker.mix(picker.pageColor, picker.inkColor, 0.35) : picker.inkColor)
            font.family: "iA Writer Mono S"
            font.bold: cell.chosen || cell.current
            font.pixelSize: picker.px(14)
        }
        MouseArea { id: cellHit; anchors.fill: parent; hoverEnabled: true; onClicked: cell.clicked() }
        Accessible.role: Accessible.Button
        Accessible.name: name || text
        Accessible.onPressAction: clicked()
    }

    contentItem: Item {
        id: keys
        implicitHeight: column.implicitHeight
        focus: true

        Keys.onPressed: function(event) {
            const shift = event.modifiers & Qt.ShiftModifier;
            if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter || event.key === Qt.Key_Space) {
                if (picker.view === 0) picker.choose(picker.cursor);
                else picker.view -= 1;
            } else if (event.text === "t" || event.text === "T") {
                picker.choose(new Date());
            } else if (picker.view !== 0) {
                if (event.key === Qt.Key_Left || event.key === Qt.Key_PageUp) picker.step(-1);
                else if (event.key === Qt.Key_Right || event.key === Qt.Key_PageDown) picker.step(1);
                else return;
            } else if (event.key === Qt.Key_Left) {
                picker.moveCursor(new Date(picker.cursor.getFullYear(), picker.cursor.getMonth(), picker.cursor.getDate() - 1));
            } else if (event.key === Qt.Key_Right) {
                picker.moveCursor(new Date(picker.cursor.getFullYear(), picker.cursor.getMonth(), picker.cursor.getDate() + 1));
            } else if (event.key === Qt.Key_Up) {
                picker.moveCursor(new Date(picker.cursor.getFullYear(), picker.cursor.getMonth(), picker.cursor.getDate() - 7));
            } else if (event.key === Qt.Key_Down) {
                picker.moveCursor(new Date(picker.cursor.getFullYear(), picker.cursor.getMonth(), picker.cursor.getDate() + 7));
            } else if (event.key === Qt.Key_PageUp) {
                picker.moveCursor(picker.addMonths(picker.cursor, shift ? -12 : -1));
            } else if (event.key === Qt.Key_PageDown) {
                picker.moveCursor(picker.addMonths(picker.cursor, shift ? 12 : 1));
            } else {
                return;
            }
            event.accepted = true;
        }

        ColumnLayout {
            id: column
            anchors.left: parent.left
            anchors.right: parent.right
            spacing: picker.px(6)

            RowLayout {
                Layout.fillWidth: true
                HeaderButton {
                    text: picker.view === 0
                        ? Qt.locale().standaloneMonthName(picker.shownMonth, Locale.LongFormat) + " " + picker.shownYear
                        : (picker.view === 1 ? String(picker.shownYear)
                                             : picker.decadeStart + " – " + (picker.decadeStart + 11))
                    name: qsTr("Zoom out to months or years")
                    onClicked: picker.view = Math.min(2, picker.view + 1)
                }
                Item { Layout.fillWidth: true }
                HeaderButton { text: "‹"; name: qsTr("Previous"); onClicked: picker.step(-1) }
                HeaderButton { text: "›"; name: qsTr("Next"); onClicked: picker.step(1) }
            }

            // Days: weekday initials, then six weeks so the height never jumps.
            GridLayout {
                visible: picker.view === 0
                Layout.fillWidth: true
                Layout.preferredHeight: picker.px(236)
                columns: 7
                rowSpacing: picker.px(2)
                columnSpacing: picker.px(2)
                Repeater {
                    model: 7
                    Text {
                        Layout.fillWidth: true
                        horizontalAlignment: Text.AlignHCenter
                        text: Qt.locale().dayName((picker.firstDay + index) % 7, Locale.ShortFormat).slice(0, 2)
                        color: picker.mix(picker.pageColor, picker.inkColor, 0.5)
                        font.family: "iA Writer Mono S"
                        font.pixelSize: picker.px(12)
                    }
                }
                Repeater {
                    model: 42
                    Cell {
                        readonly property var date: {
                            const first = new Date(picker.shownYear, picker.shownMonth, 1);
                            const offset = (first.getDay() - picker.firstDay + 7) % 7;
                            return new Date(picker.shownYear, picker.shownMonth, 1 - offset + index);
                        }
                        text: date.getDate()
                        name: date.toLocaleDateString(Qt.locale(), Locale.LongFormat)
                        outside: date.getMonth() !== picker.shownMonth
                        chosen: picker.sameDay(date, picker.selected)
                        current: picker.sameDay(date, picker.today)
                        focused: picker.sameDay(date, picker.cursor) && keys.activeFocus
                        onClicked: picker.choose(date)
                    }
                }
            }

            GridLayout {
                visible: picker.view === 1
                Layout.fillWidth: true
                Layout.preferredHeight: picker.px(236)
                columns: 3
                rowSpacing: picker.px(4)
                columnSpacing: picker.px(4)
                Repeater {
                    model: 12
                    Cell {
                        text: Qt.locale().standaloneMonthName(index, Locale.ShortFormat)
                        name: Qt.locale().standaloneMonthName(index, Locale.LongFormat)
                        chosen: picker.selected !== null && picker.selected.getFullYear() === picker.shownYear
                            && picker.selected.getMonth() === index
                        current: picker.today.getFullYear() === picker.shownYear && picker.today.getMonth() === index
                        onClicked: { picker.shownMonth = index; picker.view = 0; }
                    }
                }
            }

            GridLayout {
                visible: picker.view === 2
                Layout.fillWidth: true
                Layout.preferredHeight: picker.px(236)
                columns: 3
                rowSpacing: picker.px(4)
                columnSpacing: picker.px(4)
                Repeater {
                    model: 12
                    Cell {
                        readonly property int year: picker.decadeStart + index
                        text: year
                        chosen: picker.selected !== null && picker.selected.getFullYear() === year
                        current: picker.today.getFullYear() === year
                        onClicked: { picker.shownYear = year; picker.view = 1; }
                    }
                }
            }

            HeaderButton {
                Layout.alignment: Qt.AlignRight
                text: qsTr("Today")
                onClicked: picker.choose(new Date())
            }
        }
    }
}
