import QtQuick
import QtQuick.Layouts

// MC MR M+ M− MS, and a button that opens the memory list. `target` is the
// page whose number the keys read and recall into.
RowLayout {
    id: row

    property var ui
    property var target
    signal showMemory()

    spacing: ui.px(4)

    component MemoryKey: Rectangle {
        id: key
        property string label
        property string accessibleName
        signal activated()
        Layout.fillWidth: true
        Layout.fillHeight: true
        radius: Math.min(4, row.ui.radius)
        opacity: enabled ? 1 : 0.35
        color: hit.pressed ? row.ui.mix(row.ui.pageColor, row.ui.inkColor, 0.14)
            : (hit.containsMouse ? row.ui.mix(row.ui.pageColor, row.ui.inkColor, 0.07) : "transparent")
        Text {
            anchors.centerIn: parent
            text: key.label
            color: row.ui.inkColor
            font.family: "iA Writer Mono S"
            font.pixelSize: row.ui.px(13)
        }
        MouseArea {
            id: hit
            anchors.fill: parent
            hoverEnabled: true
            enabled: key.enabled
            onClicked: key.activated()
        }
        Accessible.role: Accessible.Button
        Accessible.name: accessibleName
        Accessible.onPressAction: activated()
    }

    MemoryKey {
        label: "MC"; accessibleName: qsTr("Memory clear")
        enabled: tape.hasMemory
        onActivated: tape.memoryClear()
    }
    MemoryKey {
        label: "MR"; accessibleName: qsTr("Memory recall")
        enabled: tape.hasMemory
        onActivated: row.target.recall(tape.memoryRecall())
    }
    MemoryKey {
        label: "M+"; accessibleName: qsTr("Memory add")
        onActivated: tape.memoryAdd(row.target.currentNumber())
    }
    MemoryKey {
        label: "M−"; accessibleName: qsTr("Memory subtract")
        onActivated: tape.memorySubtract(row.target.currentNumber())
    }
    MemoryKey {
        label: "MS"; accessibleName: qsTr("Memory store")
        onActivated: tape.memoryStore(row.target.currentNumber())
    }
    MemoryKey {
        label: "M▾"; accessibleName: qsTr("Show memory")
        enabled: tape.hasMemory
        onActivated: row.showMemory()
    }
}
