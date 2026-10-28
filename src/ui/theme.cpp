#include <ui/theme.hpp>

namespace ui {

// Dark, to match the desktop's apps; the accent is the wallpaper picker's
// selection blue.
static constexpr Theme kTheme = {
    .window = rgb(0x151c27),
    .text = rgb(0xdfe7ef),
    .textDim = rgb(0x8796a8),
    .textDisabled = rgb(0x56606d),
    .face = rgb(0x253042),
    .faceHover = rgb(0x2e3c52),
    .facePressed = rgb(0x1c2533),
    .border = rgb(0x34435a),
    .field = rgb(0x0f141c),
    .accent = rgb(0x398dd0),
    .accentHover = rgb(0x4a9cde),
    .accentText = rgb(0xffffff),
    .selection = rgb(0x398dd0),
    .selectionText = rgb(0xffffff),
    .focus = rgb(0x6cb6ee),
    .track = rgb(0x1a2230),
    .padding = 8,
    .spacing = 8,
    .radius = 6,
    .controlHeight = 30,
    .rowHeight = 26,
    .scrollBarWidth = 12,
    .checkSize = 16,
};

const Theme& theme() {
    return kTheme;
}

}
