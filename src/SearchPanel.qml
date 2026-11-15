import QtQuick
import QtQuick.Layouts

// The search field, the ranked results, and a line saying what's going on.
Rectangle {
    id: panel

    function focusField() { field.forceActiveFocus() }

    readonly property int rowHeight: 38
    readonly property int visibleRows: Math.min(8, list.count)
    readonly property bool helpVisible: backend.query === "?"

    implicitHeight: column.implicitHeight + 2
    height: implicitHeight
    radius: theme.radius
    color: theme.background
    border.color: Qt.alpha(theme.foreground, 0.12)
    border.width: 1

    // Clicks on the panel stay on the panel.
    MouseArea { anchors.fill: parent; onPressed: (mouse) => mouse.accepted = true }

    ColumnLayout {
        id: column
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 1
        spacing: 0

        // The field.
        Item {
            Layout.fillWidth: true
            implicitHeight: 56

            Text {
                id: searchGlyph
                anchors.left: parent.left
                anchors.leftMargin: 18
                anchors.verticalCenter: parent.verticalCenter
                text: backend.hintMode ? "" : ""
                font.pixelSize: 17
                color: backend.hintMode ? theme.accent : theme.muted
            }

            TextInput {
                id: field
                anchors.left: searchGlyph.right
                anchors.leftMargin: 14
                anchors.right: scopeChip.left
                anchors.rightMargin: 12
                anchors.verticalCenter: parent.verticalCenter
                font.pixelSize: 19
                color: theme.foreground
                selectionColor: theme.selection
                selectedTextColor: theme.foreground
                clip: true
                text: backend.query
                onTextEdited: backend.query = text
                focus: true

                Text {
                    anchors.fill: parent
                    visible: field.text.length === 0
                    text: qsTr("Search buttons, links, and fields…")
                    color: theme.muted
                    font: field.font
                    elide: Text.ElideRight
                }

                Keys.onPressed: (event) => {
                    const ctrl = event.modifiers & Qt.ControlModifier
                    const shift = event.modifiers & Qt.ShiftModifier
                    const alt = event.modifiers & Qt.AltModifier
                    if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                        backend.activateSelected(ctrl && shift ? "middle" : shift ? "right" : ctrl ? "double"
                                                 : alt ? "hover" : "click")
                    } else if (event.key === Qt.Key_Escape) {
                        backend.close()
                    } else if (event.key === Qt.Key_Down || event.key === Qt.Key_Tab || (ctrl && event.key === Qt.Key_N)
                               || (ctrl && event.key === Qt.Key_J)) {
                        backend.move(1)
                    } else if (event.key === Qt.Key_Up || event.key === Qt.Key_Backtab || (ctrl && event.key === Qt.Key_P)
                               || (ctrl && event.key === Qt.Key_K)) {
                        backend.move(-1)
                    } else if (event.key === Qt.Key_PageDown) {
                        backend.move(8)
                    } else if (event.key === Qt.Key_PageUp) {
                        backend.move(-8)
                    } else if (alt && event.key >= Qt.Key_1 && event.key <= Qt.Key_9) {
                        backend.activate(event.key - Qt.Key_1, shift ? "right" : "click")
                    } else if (ctrl && event.key === Qt.Key_S) {
                        backend.toggleScope()
                    } else if (ctrl && event.key === Qt.Key_R) {
                        backend.rescan()
                    } else if (ctrl && event.key === Qt.Key_U) {
                        backend.query = ""
                    } else if (event.key === Qt.Key_F1) {
                        backend.query = backend.query === "?" ? "" : "?"
                    } else {
                        return
                    }
                    event.accepted = true
                }
            }

            // Window or Screen; Ctrl+S or a click switches.
            Rectangle {
                id: scopeChip
                anchors.right: parent.right
                anchors.rightMargin: 14
                anchors.verticalCenter: parent.verticalCenter
                height: 26
                width: scopeText.implicitWidth + 20
                radius: Math.min(4, theme.radius)
                color: theme.surface

                Text {
                    id: scopeText
                    anchors.centerIn: parent
                    text: backend.scope === "screen" ? qsTr("Screen") : qsTr("Window")
                    font.pixelSize: 12
                    color: theme.foreground
                }
                MouseArea { anchors.fill: parent; onClicked: backend.toggleScope() }
            }

            // A thin line that runs while the scan is still reading.
            Rectangle {
                anchors.bottom: parent.bottom
                height: 2
                color: theme.accent
                visible: backend.scanning
                width: parent.width * 0.25
                SequentialAnimation on x {
                    running: backend.scanning
                    loops: Animation.Infinite
                    NumberAnimation { from: 0; to: panel.width * 0.75; duration: 520; easing.type: Easing.InOutQuad }
                    NumberAnimation { from: panel.width * 0.75; to: 0; duration: 520; easing.type: Easing.InOutQuad }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 1
            color: Qt.alpha(theme.foreground, 0.08)
            visible: list.count > 0 || panel.helpVisible
        }

        ListView {
            id: list
            Layout.fillWidth: true
            implicitHeight: panel.visibleRows * panel.rowHeight
            visible: !panel.helpVisible && count > 0
            clip: true
            interactive: false
            model: backend.results
            currentIndex: backend.selected
            highlightMoveDuration: 0
            highlightFollowsCurrentItem: true
            boundsBehavior: Flickable.StopAtBounds
            onCurrentIndexChanged: positionViewAtIndex(currentIndex, ListView.Contain)

            delegate: ResultRow {
                width: list.width
                height: panel.rowHeight
                onClicked: backend.activate(index, "click")
                onRightClicked: backend.activate(index, "right")
                onHovered: backend.selected = index
            }
        }

        HelpSheet {
            Layout.fillWidth: true
            visible: panel.helpVisible
        }

        // What's going on: how many, what's missing, and how to get it.
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: statusColumn.implicitHeight + 16
            color: theme.surface
            radius: theme.radius
            Rectangle {
                // Square off the top corners against the list.
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                height: parent.radius
                color: parent.color
            }

            ColumnLayout {
                id: statusColumn
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                anchors.leftMargin: 16
                anchors.rightMargin: 16
                spacing: 4

                Text {
                    Layout.fillWidth: true
                    visible: backend.error.length > 0
                    text: backend.error
                    wrapMode: Text.WordWrap
                    color: "#f7768e"
                    font.pixelSize: 12
                }

                Text {
                    Layout.fillWidth: true
                    visible: backend.inaccessible.length > 0 && backend.error.length === 0
                    text: qsTr("%1 isn't sharing its interface. Run %2, then restart it.")
                              .arg(backend.inaccessible.join(", ")).arg("<b>pounce setup</b>")
                    textFormat: Text.StyledText
                    wrapMode: Text.WordWrap
                    color: theme.muted
                    font.pixelSize: 12
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 14

                    Text {
                        text: backend.scanning
                              ? qsTr("Reading the screen…")
                              : backend.total === 0
                                ? qsTr("Nothing to click here")
                                : backend.count === 0
                                  ? qsTr("No matches")
                                  : qsTr("%1 of %n", "", backend.total).arg(backend.count)
                        color: theme.muted
                        font.pixelSize: 12
                    }
                    Item { Layout.fillWidth: true }
                    Repeater {
                        model: [
                            ["↵", qsTr("click")],
                            ["⇧↵", qsTr("right")],
                            ["⌃↵", qsTr("double")],
                            [";", qsTr("labels")],
                            ["?", qsTr("help")],
                        ]
                        delegate: Row {
                            required property var modelData
                            spacing: 5
                            Text { text: modelData[0]; color: theme.foreground; font.pixelSize: 12; font.family: "monospace" }
                            Text { text: modelData[1]; color: theme.muted; font.pixelSize: 12 }
                        }
                    }
                }
            }
        }
    }
}
