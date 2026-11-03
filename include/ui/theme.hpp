#pragma once

// The look every window and control shares (GetSysColor/GetSystemMetrics):
// read ui::theme() instead of writing colours and sizes into an app. The
// values are CLAUDE.md's "Base program style" (dark only).

#include <ui/graphics.hpp>

namespace ui {

struct Theme {
    // Surfaces.
    Color window;           // the whole window, title bar included
    Color panel;            // side panels (a sidebar)
    Color field;            // text boxes, search and path fields
    Color tooltip;

    // Lines (translucent white).
    Color divider;          // between areas
    Color border;           // around fields and buttons

    // Text.
    Color text;
    Color textDim;          // secondary text, placeholders, column headers
    Color textDisabled;
    Color textUnfocused;    // title and icons of a window without the focus

    // Controls.
    Color face;             // buttons, chips (translucent white)
    Color faceHover;
    Color facePressed;
    Color accent;           // selection, focus, default button, start button
    Color accentHover;
    Color accentLight;      // focus rings, selection marker, accent icons
    Color accentText;
    Color selection;        // selected row (accent, translucent)
    Color selectionSide;    // selected sidebar item
    Color selectionText;
    Color focus;            // focus ring
    Color thumb;            // scroll bar thumb (translucent white)
    Color thumbHover;
    Color danger;           // close button hover, destructive actions, errors
    Color success;          // a done action ("Background updated")

    // File kinds, by icon colour.
    Color kindFolder;
    Color kindProgram;
    Color kindFile;

    // Sizes, in pixels.
    int padding;            // inside controls
    int spacing;            // between related controls
    int radius;             // rows, chips, buttons
    int fieldRadius;        // fields, tooltips
    int controlHeight;      // buttons, text boxes
    int rowHeight;          // list rows (the selection is 2 px shorter)
    int scrollBarWidth;
    int checkSize;          // the check box's square
    int iconSize;           // icons in content
    int toolbarHeight;
    int statusHeight;
    int sidebarWidth;

    // Type: Stack Sans Text, sizes as em pixels.
    const Font& font() const;           // Regular 13: body text, rows
    const Font& fontMedium() const;     // Medium 13: titles, selected items
    const Font& fontSmall() const;      // Regular 12: column headers, status lines
    const Font& fontLabel() const;      // SemiBold 10.5: section labels, upper case
    const Font& fontMono() const;       // JetBrains Mono 13: terminals, code, logs
};

const Theme& theme();

}
