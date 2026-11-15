import QtQuick
import QtQuick.Layouts

// The search field as a single line: what's typed, what Enter would click,
// and how many match. The labels on screen already show every match, so the
// full list only opens on request (Down or Ctrl+L) and folds again as soon
// as the search changes.
Rectangle {
    id: panel

    function focusField() {
        listOpen = false
        field.forceActiveFocus()
    }

    readonly property int rowHeight: 38
    property bool listOpen: false
    readonly property bool helpVisible: backend.query === "?"
    readonly property bool hasMessage: backend.error.length > 0 || backend.inaccessible.length > 0
    // Labelling everything is about the screen: with nothing searched for
    // and nothing to report, the panel steps aside for a small chip.
    readonly property bool labelsOnly: backend.hintMode && backend.searchText.length === 0 && !hasMessage
    readonly property bool listShown: listOpen && !helpVisible && list.count > 0
    readonly property int visibleRows: listShown ? Math.min(8, list.count) : 0

    implicitHeight: column.implicitHeight + 2
    height: implicitHeight
    radius: theme.radius
    color: theme.background
    border.color: Qt.alpha(theme.foreground, 0.12)
    border.width: 1

    // Clicks on the panel stay on the panel.
    MouseArea { anchors.fill: parent; onPressed: (mouse) => mouse.accepted = true }

    // Alt held on its own, briefly, peeks through. A short delay keeps
    // Alt+Enter and Alt+1…9 from flickering the screen.
    Timer {
        id: peekTimer
        interval: 160
        onTriggered: backend.peeking = true
    }

    ColumnLayout {
        id: column
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 1
        spacing: 0

        // The bar.
        Item {
            Layout.fillWidth: true
            implicitHeight: 44

            Text {
                id: searchGlyph
                anchors.left: parent.left
                anchors.leftMargin: 14
                anchors.verticalCenter: parent.verticalCenter
                text: backend.hintMode ? "" : ""
                font.pixelSize: 15
                color: backend.hintMode ? theme.accent : theme.muted
            }

            TextInput {
                id: field
                anchors.left: searchGlyph.right
                anchors.leftMargin: 12
                anchors.verticalCenter: parent.verticalCenter
                // As wide as the text, so what Enter would click sits right
                // after it; the placeholder needs room of its own.
                width: Math.min(parent.width * 0.55,
                                Math.max(contentWidth + 4, text.length === 0 ? placeholder.implicitWidth : 0))
                font.pixelSize: 17
                color: theme.foreground
                selectionColor: theme.selection
                selectedTextColor: theme.foreground
                clip: true
                text: backend.query
                onTextEdited: {
                    panel.listOpen = false
                    backend.query = text
                }
                focus: true

                Text {
                    id: placeholder
                    anchors.verticalCenter: parent.verticalCenter
                    visible: field.text.length === 0
                    text: qsTr("Search buttons, links, and fields…")
                    color: theme.muted
                    font: field.font
                }

                Keys.onReleased: (event) => {
                    if (event.key === Qt.Key_Alt) {
                        peekTimer.stop()
                        backend.peeking = false
                    }
                }

                Keys.onPressed: (event) => {
                    if (event.key === Qt.Key_Alt) {
                        if (!event.isAutoRepeat)
                            peekTimer.start()
                        return
                    }
                    peekTimer.stop()
                    backend.peeking = false

                    const ctrl = event.modifiers & Qt.ControlModifier
                    const shift = event.modifiers & Qt.ShiftModifier
                    const alt = event.modifiers & Qt.AltModifier
                    if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                        backend.activateSelected(ctrl && shift ? "middle" : shift ? "right" : ctrl ? "double"
                                                 : alt ? "hover" : "click")
                    } else if (event.key === Qt.Key_Escape) {
                        if (panel.listOpen)
                            panel.listOpen = false
                        else
                            backend.close()
                    } else if (ctrl && event.key === Qt.Key_L) {
                        panel.listOpen = !panel.listOpen
                    } else if (event.key === Qt.Key_Down && !panel.listOpen && !panel.labelsOnly) {
                        panel.listOpen = true
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

            // What Enter would click, so the list isn't needed to know.
            Text {
                id: target
                anchors.left: field.right
                anchors.leftMargin: 10
                anchors.right: count.left
                anchors.rightMargin: 12
                anchors.verticalCenter: parent.verticalCenter
                visible: backend.searchText.length > 0 && backend.count > 0 && !panel.helpVisible
                text: "→ " + backend.selectedLabel
                      + (backend.selectedRole.length > 0 ? "  <font color=\"" + theme.muted + "\">" + backend.selectedRole + "</font>" : "")
                textFormat: Text.StyledText
                elide: Text.ElideRight
                color: theme.foreground
                font.pixelSize: 13
            }

            Text {
                id: count
                anchors.right: scopeChip.left
                anchors.rightMargin: 10
                anchors.verticalCenter: parent.verticalCenter
                visible: !panel.helpVisible
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

            // Window or Screen; Ctrl+S or a click switches.
            Rectangle {
                id: scopeChip
                anchors.right: parent.right
                anchors.rightMargin: 9
                anchors.verticalCenter: parent.verticalCenter
                height: 24
                width: scopeText.implicitWidth + 16
                radius: Math.min(4, theme.radius)
                color: theme.surface

                Text {
                    id: scopeText
                    anchors.centerIn: parent
                    text: backend.scope === "screen" ? qsTr("Screen") : qsTr("Window")
                    font.pixelSize: 11
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
            visible: panel.listShown || panel.helpVisible || panel.hasMessage
        }

        ListView {
            id: list
            Layout.fillWidth: true
            implicitHeight: panel.visibleRows * panel.rowHeight
            visible: panel.listShown
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

        // Only when something's wrong: what's missing, and how to get it.
        ColumnLayout {
            Layout.fillWidth: true
            Layout.leftMargin: 14
            Layout.rightMargin: 14
            Layout.topMargin: 8
            Layout.bottomMargin: 9
            visible: panel.hasMessage
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
                text: backend.restart.length > 0
                      ? qsTr("%1 was opened before accessibility was switched on. Quit it completely and open it again.")
                            .arg(backend.restart.join(", "))
                      : qsTr("%1 isn't sharing its interface. Run %2, then restart it.")
                            .arg(backend.inaccessible.join(", ")).arg("<b>pounce setup</b>")
                textFormat: Text.StyledText
                wrapMode: Text.WordWrap
                color: theme.muted
                font.pixelSize: 12
            }
        }

        // Key reminders, with the list or the help open.
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 30
            visible: panel.listShown || panel.helpVisible
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

            Row {
                anchors.right: parent.right
                anchors.rightMargin: 14
                anchors.verticalCenter: parent.verticalCenter
                spacing: 14
                Text {
                    visible: panel.helpVisible
                    text: qsTr("Esc closes, Backspace returns to the search")
                    color: theme.muted
                    font.pixelSize: 12
                }
                Repeater {
                    model: panel.helpVisible ? [] : [
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
