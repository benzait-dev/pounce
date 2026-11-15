---
name: pounce
description: Click, focus, or hover any button, link, field, tab, or menu item in a desktop app on Hyprland by its visible name, and list what is clickable on screen. Use when asked to press something in a GUI app, operate a window that has no CLI, or check what a window shows. Triggers: click the button, press, open the menu, GUI automation, what's on screen.
---

# pounce

`pounce` reads apps through the accessibility bus (AT-SPI) and acts on their
widgets. It works headless from a terminal; nothing needs to be on top.

## See what's there

```sh
pounce list                 # the focused window, best match first
pounce list --screen        # every visible window
pounce list save            # only what matches "save"
pounce list --json          # label, role, app, window, x/y/width/height, actions
pounce list --address 0x55d4c3a0  # one window by Hyprland address, even off screen
```

Find window addresses with `hyprctl clients -j`.

## Act

```sh
pounce click "Save"                  # best match for "Save" in the focused window
pounce click --screen "Reload"       # look in every visible window
pounce click --index 2 "Close"       # the second match
pounce click --right "file.txt"      # right-click
pounce click --double "Projects"     # double-click
pounce click --middle "Docs"         # middle-click (new tab for links)
pounce click --focus "Search"        # put the caret in a field, then type with wtype
pounce click --hover "Help"          # move the pointer there
pounce click --dry-run "Delete"      # report what would be clicked, do nothing
```

Add `--json` to get `{"ok", "method", "element"}`. Exit status is 0 on success
and 1 when nothing matched or the click failed (the reason is on stderr).

A plain click uses the widget's own accessibility action when it has one, so
the pointer doesn't move and covered windows still work. Right, middle, double,
hover, and widgets without an action use a virtual pointer at the widget's
centre, which needs the widget to be visible.

## When nothing is found

Run `pounce doctor`. Common causes:

- Accessibility is off: `pounce setup` switches it on.
- Chromium and Electron apps build their tree only when started with
  `--force-renderer-accessibility`; `pounce setup` adds it to
  `~/.config/chromium-flags.conf`, and the app must be restarted.
- Terminals, games, and canvas-drawn apps have no accessibility tree. Use the
  keyboard (`wtype`) instead.

Be careful with destructive buttons: list first, confirm the match is the one
you mean, then click.
