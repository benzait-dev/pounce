#pragma once

#include <QString>

// AT-SPI role and state numbers (atspi-constants.h). They are part of the
// D-Bus protocol, so they never change between toolkit versions.
namespace Role {
enum : int {
    Application = 75,
    CheckBox = 7,
    CheckMenuItem = 8,
    ComboBox = 11,
    Dialog = 16,
    DocumentFrame = 82,
    DocumentWeb = 95,
    Entry = 79,
    Filler = 20,
    Frame = 23,
    Heading = 83,
    Icon = 26,
    Label = 29,
    Link = 88,
    ListBox = 98,
    ListItem = 32,
    Menu = 33,
    MenuBar = 34,
    MenuItem = 35,
    PageTab = 37,
    Panel = 39,
    PasswordText = 40,
    PushButton = 43,
    PushButtonMenu = 129,
    RadioButton = 44,
    RadioMenuItem = 45,
    ScrollBar = 48,
    ScrollPane = 49,
    Section = 85,
    Slider = 51,
    SpinButton = 52,
    Static = 116,
    Switch = 130,
    TableCell = 56,
    Terminal = 60,
    Text = 61,
    ToggleButton = 62,
    ToolBar = 63,
    TreeItem = 91,
    Window = 69,
};
}

namespace State {
enum : int {
    Active = 1,
    Checked = 4,
    Defunct = 6,
    Editable = 7,
    Enabled = 8,
    Expanded = 10,
    Focusable = 11,
    Focused = 12,
    Pressed = 20,
    Selected = 23,
    Sensitive = 24,
    Showing = 25,
    Visible = 30,
};
}

namespace Roles {

// Roles people click, type into, or pick from. Anything else is only kept
// when the toolkit says it can take keyboard focus and it has a name, which
// catches the clickable divs and custom widgets web apps are made of.
bool isInteractive(int role);

// Big structural containers that are never targets themselves, even when a
// toolkit marks them focusable.
bool isContainer(int role);

bool isTextInput(int role);

// A short, translatable word for the role, shown beside each result.
QString displayName(int role);

// A single glyph from the Omarchy font's icon set for the result list.
QString glyph(int role);

}
