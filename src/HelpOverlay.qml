import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Every keyboard shortcut for the page that's showing, plus the global ones.
Popup {
    id: help

    property var ui
    property string mode

    modal: true
    focus: true
    anchors.centerIn: Overlay.overlay
    width: Math.min(parent ? parent.width - ui.px(24) : 360, ui.px(380))
    height: Math.min(parent ? parent.height - ui.px(24) : 500, content.implicitHeight + ui.px(32))
    padding: ui.px(16)
    background: Rectangle {
        color: help.ui.pageColor
        border.width: 1
        border.color: help.ui.lineColor
        radius: help.ui.radius
    }

    readonly property var globalKeys: [
        ["Alt+1 … 5", qsTr("Standard, Scientific, Graphing, Programmer, Date calculation")],
        ["Ctrl+H", qsTr("History")],
        ["Ctrl+M / R", qsTr("Memory store / recall")],
        ["Ctrl+P / Shift+P", qsTr("Memory add / subtract")],
        ["Ctrl+L", qsTr("Memory clear")],
        ["Ctrl+C / V", qsTr("Copy / paste")],
        ["F1  ?", qsTr("These shortcuts")],
        ["Ctrl+Q", qsTr("Quit")]
    ]
    readonly property var modeKeys: ({
        standard: [
            ["0–9 . + − * /", qsTr("Digits and operators")],
            ["Enter", qsTr("Equals")], ["Esc", qsTr("Clear")], ["Delete", qsTr("Clear entry")],
            ["Backspace", qsTr("Delete a digit")], ["%", qsTr("Percent")],
            ["S  F9", qsTr("Toggle sign")], ["R", "1/x"], ["Q", "x²"], ["@", "√x"], ["^", "xʸ"]
        ],
        scientific: [
            ["0–9 . + − * / ^", qsTr("Digits and operators")],
            ["( )", qsTr("Parentheses")], ["Enter", qsTr("Equals")], ["Esc", qsTr("Clear")],
            ["Delete", qsTr("Clear entry")], ["F9", qsTr("Toggle sign")],
            ["s o t", "sin cos tan"], ["S O T", "sin⁻¹ cos⁻¹ tan⁻¹"], ["l n", "log ln"],
            ["q #", "x² x³"], ["r", "1/x"], ["@", "√x"], ["y", "ʸ√x"], ["!", "n!"], ["%", "mod"],
            ["p e", "π e"], ["E", qsTr("Exponent")], ["|", "|x|"]
        ],
        programmer: [
            ["0–9 A–F", qsTr("Digits in the current base")],
            ["+ − * / %", qsTr("Arithmetic")], ["& | ^ ~", "AND OR XOR NOT"], ["< >", qsTr("Shift left / right")],
            ["F5 F6 F7 F8", "HEX DEC OCT BIN"], ["F12 F2 F3 F4", "QWORD DWORD WORD BYTE"],
            ["Enter", qsTr("Equals")], ["Esc", qsTr("Clear")], ["F9", qsTr("Toggle sign")]
        ],
        graphing: [
            [qsTr("Drag"), qsTr("Pan")], [qsTr("Scroll"), qsTr("Zoom at the pointer")],
            ["← → ↑ ↓", qsTr("Pan")], ["+ −", qsTr("Zoom")], ["0", qsTr("Reset the view")],
            [qsTr("Hover"), qsTr("Trace values")]
        ],
        dates: [
            ["Ctrl+D", qsTr("Back to the calculator")]
        ],
        converter: [
            ["0–9 .", qsTr("Type a value")], ["Tab ↑ ↓", qsTr("Switch fields")],
            ["- F9", qsTr("Toggle sign")], ["S", qsTr("Swap units")],
            ["Esc", qsTr("Clear")], ["Backspace", qsTr("Delete a digit")]
        ]
    })

    contentItem: Flickable {
        contentHeight: content.implicitHeight
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        ColumnLayout {
            id: content
            width: parent.width
            spacing: help.ui.px(4)

            Repeater {
                model: [
                    { title: qsTr("This page"), rows: help.modeKeys[help.mode] || [] },
                    { title: qsTr("Everywhere"), rows: help.globalKeys }
                ]
                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.topMargin: index > 0 ? help.ui.px(12) : 0
                    spacing: help.ui.px(4)
                    Text {
                        text: modelData.title
                        color: help.ui.inkColor
                        font.family: "iA Writer Mono S"
                        font.bold: true
                        font.pixelSize: help.ui.px(15)
                    }
                    Repeater {
                        model: modelData.rows
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: help.ui.px(12)
                            Text {
                                Layout.preferredWidth: help.ui.px(130)
                                text: modelData[0]
                                color: help.ui.inkColor
                                font.family: "iA Writer Mono S"
                                font.pixelSize: help.ui.px(13)
                            }
                            Text {
                                Layout.fillWidth: true
                                text: modelData[1]
                                color: help.ui.mutedColor
                                wrapMode: Text.Wrap
                                font.family: "iA Writer Mono S"
                                font.pixelSize: help.ui.px(13)
                            }
                        }
                    }
                }
            }
        }
    }
}
