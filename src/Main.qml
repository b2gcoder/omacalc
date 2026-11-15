import QtCore
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

ApplicationWindow {
    id: win
    width: 400
    height: 568
    minimumWidth: 300
    minimumHeight: 430
    visible: false
    title: qsTr("Calculator")

    readonly property bool darkMode: backend.darkMode
    readonly property color pageColor: backend.themeBackground
    readonly property color inkColor: backend.themeForeground
    // Every hardcoded size in the interface is expressed at the 400 × 568
    // design size; resizing the window scales the whole face with it. The
    // launch size below already includes the desktop text scale. A scale
    // change while the window is open waits until the next launch.
    readonly property real uiScale: Math.min(width / 400, height / 568)

    function mixColors(base, tint, amount) {
        return Qt.rgba(
            base.r + (tint.r - base.r) * amount,
            base.g + (tint.g - base.g) * amount,
            base.b + (tint.b - base.b) * amount, 1);
    }
    readonly property color mutedColor: mixColors(pageColor, inkColor, 0.5)

    function scaledSize(pixels) {
        return Math.max(1, Math.round(pixels * uiScale));
    }

    // Colors and sizes every page draws with, handed down as `ui`.
    QtObject {
        id: ui
        readonly property color pageColor: win.pageColor
        readonly property color inkColor: win.inkColor
        readonly property color mutedColor: win.mutedColor
        readonly property color lineColor: win.mixColors(win.pageColor, win.inkColor, 0.16)
        readonly property color fieldColor: win.mixColors(win.pageColor, win.inkColor, 0.05)
        readonly property color accent: backend.themeAccent
        readonly property int radius: win.scaledSize(8)
        readonly property real scale: win.uiScale
        function px(pixels) { return win.scaledSize(pixels); }
        function mix(base, tint, amount) { return win.mixColors(base, tint, amount); }
    }

    Material.theme: darkMode ? Material.Dark : Material.Light
    Material.accent: backend.themeAccent
    color: pageColor

    // ---- Modes ---------------------------------------------------------

    readonly property var calculatorModes: [
        { id: "standard", name: qsTr("Standard") },
        { id: "scientific", name: qsTr("Scientific") },
        { id: "graphing", name: qsTr("Graphing") },
        { id: "programmer", name: qsTr("Programmer") },
        { id: "dates", name: qsTr("Date calculation") }
    ]

    Settings {
        id: prefs
        category: "mode"
        property string mode: "standard"
        property string lastCalculator: "standard"
        property int scientificAngle: 0
        property int programmerBase: 10
        property int programmerWord: 64
    }

    property string mode: "standard"
    readonly property bool converterMode: mode.indexOf("conv:") === 0
    readonly property string converterCategory: converterMode ? mode.slice(5) : ""
    readonly property bool hasTape: mode === "standard" || mode === "scientific"
    readonly property var currentPage: ({
        standard: standardPage, scientific: scientificPage, graphing: graphPage,
        programmer: programmerPage, dates: datesPage
    })[mode] || converterPage

    readonly property string modeName: {
        if (converterMode)
            return converter.categoryName(converterCategory);
        for (let i = 0; i < calculatorModes.length; ++i)
            if (calculatorModes[i].id === mode)
                return calculatorModes[i].name;
        return "";
    }

    function showMode(name) {
        if (name.indexOf("conv:") !== 0 && !calculatorModes.some(m => m.id === name))
            name = "standard";
        mode = name;
        prefs.mode = name;
        // Ctrl+D returns from Dates to the calculator used before it.
        if (!converterMode && name !== "dates")
            prefs.lastCalculator = name;
        tapePanel.visible = false;
        navigation.close();
        Qt.callLater(focusPage);
    }
    function focusPage() {
        if (mode === "dates")
            datesPage.focusFirstField();
        else
            currentPage.activate();
    }
    function showTape(tab) {
        if (!hasTape)
            return;
        tapePanel.tab = tab;
        tapePanel.visible = true;
    }

    // ---- Shortcuts -----------------------------------------------------

    Shortcut {
        sequences: ["Ctrl+C", "Meta+C"]
        context: Qt.ApplicationShortcut
        enabled: win.mode !== "dates" && win.mode !== "graphing"
        onActivated: win.currentPage.copy()
    }

    Shortcut {
        sequences: ["Ctrl+V", "Meta+V"]
        context: Qt.ApplicationShortcut
        enabled: win.mode !== "dates" && win.mode !== "graphing"
        onActivated: win.currentPage.paste()
    }

    Shortcut {
        sequence: "Ctrl+Q"
        context: Qt.ApplicationShortcut
        onActivated: win.close()
    }

    Shortcut {
        sequences: ["Ctrl+D", "Ctrl+Tab"]
        context: Qt.ApplicationShortcut
        onActivated: win.showMode(win.mode === "dates" ? prefs.lastCalculator : "dates")
    }

    Repeater {
        model: win.calculatorModes
        Item {
            Shortcut {
                sequence: "Alt+" + (index + 1)
                context: Qt.ApplicationShortcut
                onActivated: win.showMode(modelData.id)
            }
        }
    }

    Shortcut {
        sequence: "Ctrl+H"
        context: Qt.ApplicationShortcut
        enabled: win.hasTape
        onActivated: tapePanel.visible && tapePanel.tab === "history" ? tapePanel.visible = false : win.showTape("history")
    }
    Shortcut {
        sequence: "Ctrl+M"
        context: Qt.ApplicationShortcut
        enabled: win.hasTape
        onActivated: tape.memoryStore(win.currentPage.currentNumber())
    }
    Shortcut {
        sequence: "Ctrl+R"
        context: Qt.ApplicationShortcut
        enabled: win.hasTape && tape.hasMemory
        onActivated: win.currentPage.recall(tape.memoryRecall())
    }
    Shortcut {
        sequence: "Ctrl+P"
        context: Qt.ApplicationShortcut
        enabled: win.hasTape
        onActivated: tape.memoryAdd(win.currentPage.currentNumber())
    }
    Shortcut {
        sequence: "Ctrl+Shift+P"
        context: Qt.ApplicationShortcut
        enabled: win.hasTape
        onActivated: tape.memorySubtract(win.currentPage.currentNumber())
    }
    Shortcut {
        sequence: "Ctrl+L"
        context: Qt.ApplicationShortcut
        enabled: win.hasTape
        onActivated: tape.memoryClear()
    }
    Shortcut {
        sequences: ["F1", "?"]
        context: Qt.ApplicationShortcut
        onActivated: help.open()
    }
    Shortcut {
        sequence: "Escape"
        context: Qt.ApplicationShortcut
        enabled: tapePanel.visible
        onActivated: { tapePanel.visible = false; win.focusPage(); }
    }

    // ---- Header --------------------------------------------------------

    Item {
        id: header
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: win.scaledSize(48)

        Rectangle {
            id: menuButton
            anchors.left: parent.left
            anchors.leftMargin: win.scaledSize(10)
            anchors.verticalCenter: parent.verticalCenter
            width: win.scaledSize(36)
            height: width
            radius: Math.min(6, ui.radius)
            color: menuHit.containsMouse || navigation.opened ? win.mixColors(win.pageColor, win.inkColor, 0.08) : "transparent"
            Column {
                anchors.centerIn: parent
                spacing: win.scaledSize(4)
                Repeater {
                    model: 3
                    Rectangle {
                        width: win.scaledSize(16)
                        height: Math.max(1, win.scaledSize(2))
                        radius: height / 2
                        color: win.inkColor
                    }
                }
            }
            MouseArea {
                id: menuHit
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: navigation.open()
            }
            Accessible.role: Accessible.ButtonMenu
            Accessible.name: qsTr("Open navigation")
            Accessible.onPressAction: navigation.open()
        }

        Text {
            anchors.left: menuButton.right
            anchors.leftMargin: win.scaledSize(10)
            anchors.right: historyButton.left
            anchors.verticalCenter: parent.verticalCenter
            text: win.modeName
            elide: Text.ElideRight
            color: win.inkColor
            font.family: "iA Writer Mono S"
            font.bold: true
            font.pixelSize: win.scaledSize(18)
        }

        Rectangle {
            id: historyButton
            visible: win.hasTape
            anchors.right: parent.right
            anchors.rightMargin: win.scaledSize(10)
            anchors.verticalCenter: parent.verticalCenter
            width: visible ? historyText.implicitWidth + win.scaledSize(18) : 0
            height: win.scaledSize(32)
            radius: Math.min(6, ui.radius)
            color: historyHit.containsMouse || tapePanel.visible ? win.mixColors(win.pageColor, win.inkColor, 0.08) : "transparent"
            Text {
                id: historyText
                anchors.centerIn: parent
                text: qsTr("History")
                color: win.inkColor
                font.family: "iA Writer Mono S"
                font.pixelSize: win.scaledSize(13)
            }
            MouseArea {
                id: historyHit
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: tapePanel.visible ? tapePanel.visible = false : win.showTape("history")
            }
            Accessible.role: Accessible.Button
            Accessible.name: qsTr("History")
        }
    }

    // ---- Pages ---------------------------------------------------------

    Item {
        id: content
        anchors.top: header.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: win.scaledSize(14)
        anchors.topMargin: win.scaledSize(4)

        StandardPage {
            id: standardPage
            objectName: "standardPage"
            anchors.fill: parent
            ui: ui
            visible: win.mode === "standard"
            onShowMemory: win.showTape("memory")
        }
        ScientificPage {
            id: scientificPage
            objectName: "scientificPage"
            anchors.fill: parent
            ui: ui
            visible: win.mode === "scientific"
            onShowMemory: win.showTape("memory")
        }
        GraphPage {
            id: graphPage
            objectName: "graphPage"
            anchors.fill: parent
            ui: ui
            visible: win.mode === "graphing"
        }
        ProgrammerPage {
            id: programmerPage
            objectName: "programmerPage"
            anchors.fill: parent
            ui: ui
            visible: win.mode === "programmer"
        }
        DatesPage {
            id: datesPage
            objectName: "datesPage"
            anchors.fill: parent
            visible: win.mode === "dates"
            pageColor: win.pageColor
            inkColor: win.inkColor
            uiScale: win.uiScale
        }
        ConverterPage {
            id: converterPage
            objectName: "converterPage"
            anchors.fill: parent
            ui: ui
            visible: win.converterMode
            category: win.converterMode ? win.converterCategory : "length"
        }

        TapePanel {
            id: tapePanel
            objectName: "tapePanel"
            visible: false
            ui: ui
            target: win.currentPage
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: Math.round(parent.height * 0.72)
            onCloseRequested: { visible = false; win.focusPage(); }
        }
    }

    // ---- Navigation ----------------------------------------------------

    Drawer {
        id: navigation
        objectName: "navigation"
        edge: Qt.LeftEdge
        width: Math.min(win.width * 0.8, win.scaledSize(290))
        height: win.height
        onClosed: win.focusPage()
        background: Rectangle {
            color: win.mixColors(win.pageColor, win.inkColor, 0.04)
        }

        ListView {
            id: navList
            anchors.fill: parent
            anchors.topMargin: win.scaledSize(8)
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar {}
            model: {
                const items = [];
                for (const m of win.calculatorModes)
                    items.push({ id: m.id, name: m.name, group: qsTr("Calculator") });
                for (const c of converter.categories)
                    items.push({ id: "conv:" + c.id, name: c.name, group: qsTr("Converter") });
                return items;
            }
            delegate: Column {
                id: navRow
                required property var modelData
                required property int index
                width: ListView.view.width

                // A heading above the first entry of each group.
                Text {
                    visible: navRow.index === 0 || navList.model[navRow.index - 1].group !== navRow.modelData.group
                    width: parent.width
                    leftPadding: win.scaledSize(20)
                    topPadding: win.scaledSize(14)
                    bottomPadding: win.scaledSize(6)
                    text: navRow.modelData.group
                    color: win.mutedColor
                    font.family: "iA Writer Mono S"
                    font.pixelSize: win.scaledSize(12)
                    Accessible.role: Accessible.Heading
                }

            Rectangle {
                id: navItem
                readonly property var modelData: navRow.modelData
                readonly property bool current: win.mode === modelData.id
                width: parent.width
                height: win.scaledSize(38)
                color: current ? win.mixColors(win.pageColor, win.inkColor, 0.1)
                    : (navHit.containsMouse ? win.mixColors(win.pageColor, win.inkColor, 0.06) : "transparent")
                Rectangle {
                    visible: navItem.current
                    width: win.scaledSize(3)
                    height: parent.height - win.scaledSize(14)
                    anchors.verticalCenter: parent.verticalCenter
                    radius: width / 2
                    color: ui.accent
                }
                Text {
                    anchors.left: parent.left
                    anchors.leftMargin: win.scaledSize(20)
                    anchors.verticalCenter: parent.verticalCenter
                    text: navItem.modelData.name
                    color: win.inkColor
                    font.family: "iA Writer Mono S"
                    font.bold: navItem.current
                    font.pixelSize: win.scaledSize(15)
                }
                MouseArea {
                    id: navHit
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: win.showMode(navItem.modelData.id)
                }
                Accessible.role: Accessible.MenuItem
                Accessible.name: modelData.name
            }
            }
        }
    }

    HelpOverlay {
        id: help
        ui: ui
        mode: win.converterMode ? "converter" : win.mode
        onClosed: win.focusPage()
    }

    // Per-mode choices survive a restart.
    Connections {
        target: scientific
        function onChanged() { prefs.scientificAngle = scientific.angleUnit; }
    }
    Connections {
        target: programmer
        function onChanged() {
            prefs.programmerBase = programmer.base;
            prefs.programmerWord = programmer.wordBits;
        }
    }

    Component.onCompleted: {
        // Read every saved value before applying any: each change writes the
        // current state back through the Connections above.
        const angle = prefs.scientificAngle;
        const base = prefs.programmerBase;
        const word = prefs.programmerWord;
        scientific.angleUnit = angle;
        programmer.base = base;
        programmer.wordBits = word;
        mode = prefs.mode;
        if (mode.indexOf("conv:") !== 0 && !calculatorModes.some(m => m.id === mode))
            mode = "standard";

        // Chosen once. Hyprland reports a normal float as maximized, so a
        // saved rectangle was the tiled size and the next launch filled the
        // screen. Position stays with the compositor.
        var size = backend.defaultWindowSize();
        width = size.width;
        height = size.height;
        visible = true;
        Qt.callLater(focusPage);
    }
}
