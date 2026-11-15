import QtQuick
import QtQuick.Layouts

// One result: its label, the matched letters picked out, what it is, and
// which app it's in.
Rectangle {
    id: row

    required property int index
    required property string labelHtml
    required property string roleName
    required property string glyph
    required property string app
    required property string hint
    required property bool inActiveWindow
    required property bool checked

    signal clicked()
    signal rightClicked()
    signal hovered()

    readonly property bool current: ListView.isCurrentItem

    color: current ? theme.selection : "transparent"

    Rectangle {
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: 3
        color: theme.accent
        visible: row.current
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 14
        anchors.rightMargin: 16
        spacing: 12

        Rectangle {
            Layout.preferredWidth: Math.max(26, hintText.implicitWidth + 10)
            Layout.preferredHeight: 20
            radius: Math.min(4, theme.radius)
            color: row.current ? theme.accent : theme.surface
            Text {
                id: hintText
                anchors.centerIn: parent
                text: row.hint.toUpperCase()
                font.family: "monospace"
                font.pixelSize: 11
                font.bold: true
                color: row.current ? theme.accentForeground : theme.muted
            }
        }

        Text {
            Layout.preferredWidth: 18
            horizontalAlignment: Text.AlignHCenter
            text: row.glyph
            font.pixelSize: 14
            color: row.current ? theme.accent : theme.muted
        }

        Text {
            Layout.fillWidth: true
            text: row.labelHtml
            textFormat: Text.StyledText
            elide: Text.ElideRight
            font.pixelSize: 15
            color: theme.foreground
        }

        Text {
            text: row.checked ? "✓ " + row.roleName : row.roleName
            font.pixelSize: 12
            color: theme.muted
        }

        Text {
            visible: !row.inActiveWindow && row.app.length > 0
            Layout.maximumWidth: 140
            text: row.app
            elide: Text.ElideRight
            font.pixelSize: 12
            color: theme.muted
            opacity: 0.8
        }
    }

    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        onEntered: row.hovered()
        onClicked: (mouse) => mouse.button === Qt.RightButton ? row.rightClicked() : row.clicked()
    }
}
