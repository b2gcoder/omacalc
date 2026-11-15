import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import Omacalc

// Graphing: up to six equations in x over a grid. Drag to pan, scroll to
// zoom around the pointer, hover to trace values. Equations use the same
// syntax as the Scientific keypad (sin(x), x^2, 2x, √x, |x|, …).
FocusScope {
    id: page

    property var ui
    property var equations: ["x^2", "", ""]
    readonly property var curveColors: ["#4f8cff", "#e5534b", "#3fb950", "#d29922", "#a371f7", "#39c5cf"]
    readonly property var angleNames: ["DEG", "RAD", "GRAD"]

    function activate() { graph.forceActiveFocus(); }
    function copy() {}
    function paste() {}

    function setEquation(index, text) {
        const next = equations.slice();
        next[index] = text;
        equations = next;
    }
    function removeEquation(index) {
        const next = equations.slice();
        next.splice(index, 1);
        if (next.length === 0)
            next.push("");
        equations = next;
    }

    component ToolKey: Rectangle {
        id: tool
        property string text
        property string name
        signal clicked()
        Layout.preferredWidth: Math.max(page.ui.px(32), label.implicitWidth + page.ui.px(14))
        Layout.fillHeight: true
        radius: Math.min(4, page.ui.radius)
        color: hit.pressed ? page.ui.mix(page.ui.pageColor, page.ui.inkColor, 0.16)
            : (hit.containsMouse ? page.ui.mix(page.ui.pageColor, page.ui.inkColor, 0.1)
                                 : page.ui.mix(page.ui.pageColor, page.ui.inkColor, 0.05))
        Text {
            id: label
            anchors.centerIn: parent
            text: tool.text
            color: page.ui.inkColor
            font.family: "iA Writer Mono S"
            font.pixelSize: page.ui.px(14)
        }
        MouseArea {
            id: hit
            anchors.fill: parent
            hoverEnabled: true
            onClicked: tool.clicked()
        }
        Accessible.role: Accessible.Button
        Accessible.name: name || text
        Accessible.onPressAction: clicked()
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: page.ui.px(8)

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: page.ui.px(160)
            radius: page.ui.radius
            clip: true
            color: page.ui.pageColor
            border.width: 1
            border.color: graph.activeFocus ? page.ui.accent : page.ui.lineColor

            GraphView {
                id: graph
                objectName: "graphView"
                anchors.fill: parent
                anchors.margins: 1
                focus: true
                equations: page.equations
                colors: page.curveColors
                background: page.ui.pageColor
                ink: page.ui.inkColor
                fontFamily: "iA Writer Mono S"
                fontPixelSize: page.ui.px(11)
                Component.onCompleted: resetView()
                property bool viewTouched: false
                onWidthChanged: if (!viewTouched) resetView()
                onHeightChanged: if (!viewTouched) resetView()

                Keys.onPressed: function(event) {
                    const step = page.ui.px(40);
                    if (event.key === Qt.Key_Left) graph.panBy(step, 0);
                    else if (event.key === Qt.Key_Right) graph.panBy(-step, 0);
                    else if (event.key === Qt.Key_Up) graph.panBy(0, step);
                    else if (event.key === Qt.Key_Down) graph.panBy(0, -step);
                    else if (event.text === "+" || event.text === "=") graph.zoom(1.25);
                    else if (event.text === "-") graph.zoom(0.8);
                    else if (event.text === "0") { graph.resetView(); graph.viewTouched = false; return; }
                    else return;
                    graph.viewTouched = true;
                    event.accepted = true;
                }

                MouseArea {
                    id: pointer
                    anchors.fill: parent
                    hoverEnabled: true
                    preventStealing: true
                    cursorShape: pressed ? Qt.ClosedHandCursor : Qt.CrossCursor
                    property real lastX
                    property real lastY
                    onPressed: function(mouse) {
                        graph.forceActiveFocus();
                        lastX = mouse.x;
                        lastY = mouse.y;
                    }
                    onPositionChanged: function(mouse) {
                        if (pressed) {
                            graph.panBy(mouse.x - lastX, mouse.y - lastY);
                            graph.viewTouched = true;
                            lastX = mouse.x;
                            lastY = mouse.y;
                        }
                        graph.traceX = mouse.x;
                    }
                    onContainsMouseChanged: graph.tracing = containsMouse
                    onWheel: function(wheel) {
                        graph.zoomAt(wheel.angleDelta.y > 0 ? 1.15 : 1 / 1.15, wheel.x, wheel.y);
                        graph.viewTouched = true;
                    }
                }
            }

            Text {
                anchors.left: parent.left
                anchors.bottom: parent.bottom
                anchors.margins: page.ui.px(6)
                text: graph.viewText
                color: page.ui.mutedColor
                font.family: "iA Writer Mono S"
                font.pixelSize: page.ui.px(10)
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: page.ui.px(30)
            Layout.fillHeight: false
            spacing: page.ui.px(6)
            ToolKey { text: "+"; name: qsTr("Zoom in"); onClicked: { graph.zoom(1.25); graph.viewTouched = true; } }
            ToolKey { text: "−"; name: qsTr("Zoom out"); onClicked: { graph.zoom(0.8); graph.viewTouched = true; } }
            ToolKey { text: qsTr("Reset"); onClicked: { graph.resetView(); graph.viewTouched = false; } }
            Item { Layout.fillWidth: true }
            ToolKey {
                text: page.angleNames[graph.angleUnit]
                name: qsTr("Angle unit")
                onClicked: graph.angleUnit = (graph.angleUnit + 1) % 3
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: page.ui.px(4)

            Repeater {
                model: page.equations.length
                ColumnLayout {
                    id: row
                    readonly property string error: graph.errors[index] || ""
                    Layout.fillWidth: true
                    spacing: page.ui.px(1)
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: page.ui.px(6)
                        Rectangle {
                            Layout.preferredWidth: page.ui.px(6)
                            Layout.preferredHeight: page.ui.px(26)
                            Layout.fillHeight: false
                            radius: width / 2
                            color: page.curveColors[index % page.curveColors.length]
                        }
                        Text {
                            text: "y ="
                            color: page.ui.mutedColor
                            font.family: "iA Writer Mono S"
                            font.pixelSize: page.ui.px(14)
                        }
                        TextField {
                            id: field
                            Layout.fillWidth: true
                            text: page.equations[index]
                            color: page.ui.inkColor
                            selectByMouse: true
                            font.family: "iA Writer Mono S"
                            font.pixelSize: page.ui.px(15)
                            topPadding: page.ui.px(4)
                            bottomPadding: page.ui.px(4)
                            leftPadding: page.ui.px(8)
                            rightPadding: page.ui.px(8)
                            placeholderText: index === 0 ? "" : qsTr("Enter an equation")
                            placeholderTextColor: page.ui.mix(page.ui.pageColor, page.ui.inkColor, 0.35)
                            onTextEdited: page.setEquation(index, text)
                            Keys.onReturnPressed: graph.forceActiveFocus()
                            Keys.onEscapePressed: graph.forceActiveFocus()
                            background: Rectangle {
                                radius: Math.min(4, page.ui.radius)
                                color: page.ui.fieldColor
                                border.width: 1
                                border.color: row.error !== "" ? Material.color(Material.Red)
                                    : (field.activeFocus ? page.ui.accent : page.ui.lineColor)
                            }
                            Accessible.name: qsTr("Equation %1").arg(index + 1)
                        }
                        ToolKey {
                            text: "×"
                            name: qsTr("Remove equation")
                            Layout.fillHeight: false
                            Layout.preferredHeight: page.ui.px(26)
                            onClicked: page.removeEquation(index)
                        }
                    }
                    Text {
                        visible: row.error !== ""
                        Layout.fillWidth: true
                        Layout.leftMargin: page.ui.px(50)
                        text: row.error
                        color: Material.color(Material.Red)
                        elide: Text.ElideRight
                        font.family: "iA Writer Mono S"
                        font.pixelSize: page.ui.px(11)
                    }
                }
            }

            ToolKey {
                visible: page.equations.length < 6
                Layout.fillHeight: false
                Layout.preferredHeight: page.ui.px(26)
                text: "+ " + qsTr("Add equation")
                onClicked: page.equations = page.equations.concat([""])
            }
        }
    }
}
