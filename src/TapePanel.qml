import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// History and memory, sliding over the keypad. Choosing an entry recalls
// its value into the calculator that is showing (`target`).
Rectangle {
    id: panel

    property var ui
    property var target
    property string tab: "history" // "history" or "memory"
    signal closeRequested()

    color: ui.pageColor
    border.width: 1
    border.color: ui.lineColor
    radius: ui.radius

    // Swallow clicks so they don't reach the keypad underneath.
    MouseArea { anchors.fill: parent }

    component Tab: Rectangle {
        id: tabItem
        property string name
        property string label
        readonly property bool current: panel.tab === name
        Layout.preferredWidth: tabText.implicitWidth + panel.ui.px(20)
        Layout.preferredHeight: panel.ui.px(30)
        color: "transparent"
        Text {
            id: tabText
            anchors.centerIn: parent
            text: tabItem.label
            color: tabItem.current ? panel.ui.inkColor : panel.ui.mutedColor
            font.family: "iA Writer Mono S"
            font.bold: tabItem.current
            font.pixelSize: panel.ui.px(14)
        }
        Rectangle {
            visible: tabItem.current
            anchors.bottom: parent.bottom
            anchors.horizontalCenter: parent.horizontalCenter
            width: tabText.implicitWidth
            height: panel.ui.px(2)
            color: panel.ui.accent
        }
        MouseArea { anchors.fill: parent; onClicked: panel.tab = tabItem.name }
        Accessible.role: Accessible.PageTab
        Accessible.name: label
    }

    component LinkButton: Rectangle {
        id: link
        property string text
        signal clicked()
        implicitWidth: linkText.implicitWidth + panel.ui.px(14)
        implicitHeight: panel.ui.px(24)
        radius: Math.min(4, panel.ui.radius)
        color: linkHit.containsMouse ? panel.ui.mix(panel.ui.pageColor, panel.ui.inkColor, 0.1) : "transparent"
        Text {
            id: linkText
            anchors.centerIn: parent
            text: link.text
            color: panel.ui.inkColor
            font.family: "iA Writer Mono S"
            font.pixelSize: panel.ui.px(12)
        }
        MouseArea { id: linkHit; anchors.fill: parent; hoverEnabled: true; onClicked: link.clicked() }
        Accessible.role: Accessible.Button
        Accessible.name: text
        Accessible.onPressAction: clicked()
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: panel.ui.px(10)
        spacing: panel.ui.px(6)

        RowLayout {
            Layout.fillWidth: true
            Tab { name: "history"; label: qsTr("History") }
            Tab { name: "memory"; label: qsTr("Memory") }
            Item { Layout.fillWidth: true }
            LinkButton {
                text: "✕"
                onClicked: panel.closeRequested()
                Accessible.name: qsTr("Close")
            }
        }

        ListView {
            id: list
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: panel.ui.px(2)
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar {}
            model: panel.tab === "history" ? tape.history : tape.memory

            delegate: Rectangle {
                id: entry
                width: ListView.view.width
                height: column.implicitHeight + panel.ui.px(12)
                radius: Math.min(4, panel.ui.radius)
                color: entryHit.containsMouse ? panel.ui.mix(panel.ui.pageColor, panel.ui.inkColor, 0.06) : "transparent"

                MouseArea {
                    id: entryHit
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: {
                        panel.target.recall(modelData.value);
                        panel.closeRequested();
                    }
                }

                ColumnLayout {
                    id: column
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.leftMargin: panel.ui.px(8)
                    anchors.rightMargin: panel.ui.px(8)
                    spacing: panel.ui.px(2)
                    Text {
                        visible: panel.tab === "history"
                        Layout.fillWidth: true
                        horizontalAlignment: Text.AlignRight
                        text: (modelData.expression || "") + " ="
                        color: panel.ui.mutedColor
                        elide: Text.ElideLeft
                        font.family: "iA Writer Mono S"
                        font.pixelSize: panel.ui.px(13)
                    }
                    Text {
                        Layout.fillWidth: true
                        horizontalAlignment: Text.AlignRight
                        text: panel.tab === "history" ? modelData.result : modelData.text
                        color: panel.ui.inkColor
                        elide: Text.ElideLeft
                        font.family: "iA Writer Mono S"
                        font.pixelSize: panel.ui.px(22)
                    }
                    RowLayout {
                        visible: panel.tab === "memory"
                        Layout.alignment: Qt.AlignRight
                        spacing: panel.ui.px(2)
                        LinkButton { text: "MC"; onClicked: tape.memoryItemClear(index) }
                        LinkButton { text: "M+"; onClicked: tape.memoryItemAdd(index, panel.target.currentNumber()) }
                        LinkButton { text: "M−"; onClicked: tape.memoryItemSubtract(index, panel.target.currentNumber()) }
                    }
                }
            }

            Text {
                anchors.centerIn: parent
                visible: list.count === 0
                text: panel.tab === "history" ? qsTr("There's no history yet") : qsTr("There's nothing saved in memory")
                color: panel.ui.mutedColor
                font.family: "iA Writer Mono S"
                font.pixelSize: panel.ui.px(13)
            }
        }

        LinkButton {
            Layout.alignment: Qt.AlignRight
            visible: list.count > 0
            text: panel.tab === "history" ? qsTr("Clear history") : qsTr("Clear memory")
            onClicked: panel.tab === "history" ? tape.clearHistory() : tape.memoryClear()
        }
    }
}
