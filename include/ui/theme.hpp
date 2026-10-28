#pragma once

// The look every window and control shares (GetSysColor/GetSystemMetrics):
// read ui::theme() instead of writing colours and sizes into an app.

#include <ui/graphics.hpp>

namespace ui {

struct Theme {
    // Colours.
    Color window;           // window background
    Color text;
    Color textDim;          // secondary text, placeholders
    Color textDisabled;
    Color face;             // buttons, check boxes, scroll thumbs
    Color faceHover;
    Color facePressed;
    Color border;
    Color field;            // text box and list background
    Color accent;           // default button, checked box
    Color accentHover;
    Color accentText;
    Color selection;        // selected row, selected text
    Color selectionText;
    Color focus;            // focus ring
    Color track;            // scroll bar track

    // Sizes, in pixels.
    int padding;            // inside controls
    int spacing;            // between controls
    int radius;             // corner radius of controls
    int controlHeight;      // buttons, text boxes
    int rowHeight;          // list rows
    int scrollBarWidth;
    int checkSize;          // the check box's square

    const Font& font() const { return Font::ui(); }
};

const Theme& theme();

}
