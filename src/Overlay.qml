import QtQuick
import QtQuick.Window

// One monitor's overlay: a light dim, a label on every element, a ring around
// the selected one, and, on the focused monitor, the search panel.
Window {
    id: root

    property string monitorName
    property bool primary: false

    // Where this monitor sits in Hyprland's layout, which is the space the
    // element rectangles are in.
    readonly property real originX: screen ? screen.virtualX : 0
    readonly property real originY: screen ? screen.virtualY : 0

    flags: Qt.FramelessWindowHint
    color: "transparent"
    title: "pounce"

    function onThisMonitor(x, y, w, h) {
        return x + w > originX && x < originX + width && y + h > originY && y < originY + height
    }

    Item {
        id: content
        anchors.fill: parent
        opacity: 0
        LayoutMirroring.enabled: Qt.application.layoutDirection === Qt.RightToLeft
        LayoutMirroring.childrenInherit: true

        states: State {
            name: "shown"
            when: root.visible
            PropertyChanges { target: content; opacity: 1 }
        }
        transitions: Transition {
            to: "shown"
            NumberAnimation { property: "opacity"; duration: 90; easing.type: Easing.OutCubic }
        }

        Rectangle {
            anchors.fill: parent
            color: Qt.rgba(0, 0, 0, backend.dim)
        }

        // A click anywhere that isn't a label or the panel closes, like
        // clicking away from a menu.
        MouseArea {
            anchors.fill: parent
            onClicked: backend.close()
        }

        // The selected element, ringed in the accent.
        Rectangle {
            id: ring
            readonly property rect target: backend.selectedRect
            visible: backend.count > 0 && target.width > 0
                     && root.onThisMonitor(target.x, target.y, target.width, target.height)
            x: target.x - root.originX - 3
            y: target.y - root.originY - 3
            width: target.width + 6
            height: target.height + 6
            radius: Math.min(6, theme.radius + 2)
            color: Qt.alpha(theme.accent, 0.14)
            border.color: theme.accent
            border.width: 2

            Behavior on x { NumberAnimation { duration: 70; easing.type: Easing.OutCubic } }
            Behavior on y { NumberAnimation { duration: 70; easing.type: Easing.OutCubic } }
            Behavior on width { NumberAnimation { duration: 70; easing.type: Easing.OutCubic } }
            Behavior on height { NumberAnimation { duration: 70; easing.type: Easing.OutCubic } }
        }

        Repeater {
            model: backend.results

            delegate: HintLabel {
                required property int index
                required property string hint
                required property real elementX
                required property real elementY
                required property real elementWidth
                required property real elementHeight

                text: hint
                typed: hint.startsWith(backend.hintTyped) ? backend.hintTyped.length : 0
                selected: index === backend.selected
                prominent: backend.hintMode
                visible: backend.hintsReady && hint.length > 0
                         && root.onThisMonitor(elementX, elementY, elementWidth, elementHeight)
                // Tucked into the element's top-left corner, kept on screen.
                x: Math.max(2, Math.min(root.width - width - 2, elementX - root.originX - 4))
                y: Math.max(2, Math.min(root.height - height - 2, elementY - root.originY - 4))
                z: selected ? 2 : 1

                onPicked: backend.activate(index, "click")
                onPickedSecondary: backend.activate(index, "right")
            }
        }

        SearchPanel {
            id: panel
            visible: root.primary
            anchors.horizontalCenter: parent.horizontalCenter
            y: Math.round(backend.panelSlot === 0 ? parent.height * 0.12
                          : backend.panelSlot === 1 ? (parent.height - height) / 2
                          : parent.height * 0.92 - height)
            Behavior on y { NumberAnimation { duration: 140; easing.type: Easing.OutCubic } }
            width: Math.min(700, parent.width - 32)
            z: 10

            // Step out of the way of what's selected, so the ring is never
            // hidden behind the panel.
            readonly property rect target: backend.selectedRect
            readonly property bool covering: target.width > 0
                && target.x - root.originX < x + width && target.x - root.originX + target.width > x
                && target.y - root.originY < y + height && target.y - root.originY + target.height > y
            opacity: covering ? 0.28 : 1
            Behavior on opacity { NumberAnimation { duration: 120 } }
        }
    }

    onVisibleChanged: if (visible && primary) panel.focusField()
}
