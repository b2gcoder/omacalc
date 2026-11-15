import QtQuick

// The expression line above and the number below, shared by the keypad
// calculators. The number shrinks to fit rather than eliding digits.
Item {
    id: area

    property var ui
    property string expression
    property string display
    property bool errored: false
    // A short note under the expression, such as the angle unit.
    property string badge

    Text {
        id: expressionText
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: badgeText.visible ? badgeText.left : parent.right
        anchors.rightMargin: badgeText.visible ? area.ui.px(8) : 0
        text: area.expression
        color: area.ui.mutedColor
        elide: Text.ElideLeft
        horizontalAlignment: Text.AlignRight
        font.family: "iA Writer Mono S"
        font.pixelSize: area.ui.px(18)
    }

    Text {
        id: badgeText
        visible: area.badge !== ""
        anchors.top: parent.top
        anchors.right: parent.right
        text: area.badge
        color: area.ui.mutedColor
        font.family: "iA Writer Mono S"
        font.pixelSize: area.ui.px(13)
    }

    Text {
        anchors.top: expressionText.bottom
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        horizontalAlignment: Text.AlignRight
        verticalAlignment: Text.AlignBottom
        text: area.display
        color: area.errored ? area.ui.mutedColor : area.ui.inkColor
        fontSizeMode: Text.Fit
        minimumPixelSize: area.ui.px(14)
        font.family: "iA Writer Mono S"
        font.pixelSize: area.ui.px(area.errored ? 34 : 64)
    }
}
