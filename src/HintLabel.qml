import QtQuick

// A hint label drawn on an element. The letters already typed fade back, so
// what's left to type stands out.
Rectangle {
    id: label

    property string text
    property int typed: 0
    property bool selected: false
    property bool prominent: false

    signal picked()
    signal pickedSecondary()

    width: letters.implicitWidth + 8
    height: letters.implicitHeight + 3
    radius: Math.min(4, theme.radius)
    color: selected || prominent ? theme.accent : Qt.alpha(theme.accent, 0.82)
    border.color: Qt.rgba(0, 0, 0, 0.35)
    border.width: 1
    scale: selected ? 1.15 : 1

    Behavior on scale { NumberAnimation { duration: 80 } }

    Text {
        id: letters
        anchors.centerIn: parent
        textFormat: Text.StyledText
        font.family: "monospace"
        font.pixelSize: 12
        font.bold: true
        color: theme.accentForeground
        text: {
            const shown = label.text.toUpperCase()
            if (label.typed <= 0)
                return shown
            return "<font color=\"" + Qt.alpha(theme.accentForeground, 0.4) + "\">" + shown.substring(0, label.typed)
                 + "</font>" + shown.substring(label.typed)
        }
    }

    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        cursorShape: Qt.PointingHandCursor
        onClicked: (mouse) => mouse.button === Qt.RightButton ? label.pickedSecondary() : label.picked()
    }
}
