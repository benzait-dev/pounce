import QtQuick
import QtQuick.Layouts

// Every key, shown when the query is "?" or F1 is pressed.
Item {
    implicitHeight: grid.implicitHeight + 28

    GridLayout {
        id: grid
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 16
        columns: 2
        columnSpacing: 18
        rowSpacing: 7

        Repeater {
            model: [
                ["Enter", qsTr("Click the selected element")],
                ["Shift+Enter", qsTr("Right-click")],
                ["Ctrl+Enter", qsTr("Double-click")],
                ["Ctrl+Shift+Enter", qsTr("Middle-click (open links in a new tab)")],
                ["Alt+Enter", qsTr("Move the pointer there")],
                ["↓  Ctrl+L", qsTr("Show the full list of matches")],
                ["↑ ↓  Tab  Ctrl+N/P", qsTr("Move the selection")],
                [qsTr("Hold Alt"), qsTr("See the screen without pounce")],
                ["Alt+1…9", qsTr("Click result 1 to 9")],
                [qsTr("label, Enter"), qsTr("Select a labelled element, then click it")],
                [";", qsTr("Label everything; type a label to click it")],
                [qsTr("text;label"), qsTr("Label only what matches the text")],
                [qsTr("UPPERCASE label"), qsTr("Right-click by label")],
                ["Ctrl+S", qsTr("Switch between this window and the whole screen")],
                ["Ctrl+R", qsTr("Read the screen again")],
                ["Ctrl+U", qsTr("Clear the search")],
                ["Esc", qsTr("Close")],
            ]
            delegate: Item {
                required property var modelData
                required property int index
                Layout.columnSpan: 2
                Layout.fillWidth: true
                implicitHeight: Math.max(keys.implicitHeight, meaning.implicitHeight)
                Text {
                    id: keys
                    width: 220
                    text: modelData[0]
                    font.family: "monospace"
                    font.pixelSize: 12
                    color: theme.accent
                }
                Text {
                    id: meaning
                    anchors.left: keys.right
                    anchors.leftMargin: 12
                    anchors.right: parent.right
                    text: modelData[1]
                    wrapMode: Text.WordWrap
                    font.pixelSize: 13
                    color: theme.foreground
                }
            }
        }
    }
}
