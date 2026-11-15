#include "roles.h"

#include <QCoreApplication>

namespace Roles {

bool isInteractive(int role) {
    switch (role) {
    case Role::PushButton:
    case Role::PushButtonMenu:
    case Role::ToggleButton:
    case Role::Switch:
    case Role::CheckBox:
    case Role::RadioButton:
    case Role::Link:
    case Role::MenuItem:
    case Role::CheckMenuItem:
    case Role::RadioMenuItem:
    case Role::Menu:
    case Role::Entry:
    case Role::PasswordText:
    case Role::ComboBox:
    case Role::PageTab:
    case Role::ListItem:
    case Role::TableCell:
    case Role::TreeItem:
    case Role::Slider:
    case Role::SpinButton:
    case Role::Icon:
        return true;
    default:
        return false;
    }
}

bool isContainer(int role) {
    switch (role) {
    case Role::Application:
    case Role::Frame:
    case Role::Window:
    case Role::Dialog:
    case Role::DocumentWeb:
    case Role::DocumentFrame:
    case Role::Panel:
    case Role::Filler:
    case Role::ScrollPane:
    case Role::ScrollBar:
    case Role::Terminal:
    case Role::ToolBar:
    case Role::MenuBar:
    case Role::ListBox:
        return true;
    default:
        return false;
    }
}

bool isTextInput(int role) {
    return role == Role::Entry || role == Role::PasswordText || role == Role::Text || role == Role::SpinButton
        || role == Role::ComboBox;
}

QString displayName(int role) {
    const char *name = nullptr;
    switch (role) {
    case Role::PushButton:
    case Role::PushButtonMenu: name = QT_TR_NOOP("Button"); break;
    case Role::ToggleButton:
    case Role::Switch: name = QT_TR_NOOP("Toggle"); break;
    case Role::CheckBox: name = QT_TR_NOOP("Checkbox"); break;
    case Role::RadioButton: name = QT_TR_NOOP("Option"); break;
    case Role::Link: name = QT_TR_NOOP("Link"); break;
    case Role::MenuItem:
    case Role::CheckMenuItem:
    case Role::RadioMenuItem: name = QT_TR_NOOP("Menu item"); break;
    case Role::Menu: name = QT_TR_NOOP("Menu"); break;
    case Role::Entry:
    case Role::Text: name = QT_TR_NOOP("Text field"); break;
    case Role::PasswordText: name = QT_TR_NOOP("Password"); break;
    case Role::ComboBox: name = QT_TR_NOOP("Dropdown"); break;
    case Role::PageTab: name = QT_TR_NOOP("Tab"); break;
    case Role::ListItem:
    case Role::TableCell:
    case Role::TreeItem: name = QT_TR_NOOP("Item"); break;
    case Role::Slider: name = QT_TR_NOOP("Slider"); break;
    case Role::SpinButton: name = QT_TR_NOOP("Number"); break;
    case Role::Icon: name = QT_TR_NOOP("Icon"); break;
    case Role::Heading: name = QT_TR_NOOP("Heading"); break;
    default: name = QT_TR_NOOP("Element"); break;
    }
    return QCoreApplication::translate("Roles", name);
}

// Nerd Font glyphs; Omarchy's default fonts carry them.
QString glyph(int role) {
    switch (role) {
    case Role::Link: return QString::fromUtf8("");
    case Role::Entry:
    case Role::Text:
    case Role::SpinButton: return QString::fromUtf8("");
    case Role::PasswordText: return QString::fromUtf8("");
    case Role::CheckBox:
    case Role::CheckMenuItem: return QString::fromUtf8("");
    case Role::RadioButton:
    case Role::RadioMenuItem: return QString::fromUtf8("");
    case Role::ToggleButton:
    case Role::Switch: return QString::fromUtf8("");
    case Role::PageTab: return QString::fromUtf8("");
    case Role::Menu:
    case Role::MenuItem: return QString::fromUtf8("");
    case Role::ComboBox: return QString::fromUtf8("");
    case Role::ListItem:
    case Role::TableCell:
    case Role::TreeItem: return QString::fromUtf8("");
    case Role::Slider: return QString::fromUtf8("");
    case Role::Icon: return QString::fromUtf8("");
    default: return QString::fromUtf8("");
    }
}

}
