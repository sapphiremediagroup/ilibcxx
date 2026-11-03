#include <ui/theme.hpp>

namespace ui {

namespace {
constexpr char kTextRegular[] = "/bin/StackSansText-Regular.ttf";
constexpr char kTextMedium[] = "/bin/StackSansText-Medium.ttf";
constexpr char kTextSemiBold[] = "/bin/StackSansText-SemiBold.ttf";
constexpr char kMono[] = "/bin/JetBrainsMono-Regular.ttf";

constexpr Color white(std::uint8_t percent) {
    return rgba(0xffffff, static_cast<std::uint8_t>(percent * 255 / 100));
}

// A font loaded on first use; falls back to Font::ui() when the file is
// missing so text still shows.
const Font& lazy(Font& font, bool& tried, const char* path, float em) {
    if (!tried) {
        tried = true;
        font.load_em(path, em);
    }
    return font.valid() ? font : Font::ui();
}
}  // namespace

static constexpr Theme kTheme = {
    .window = rgb(0x161d28),
    .panel = rgb(0x121821),
    .field = rgb(0x0f141c),
    .tooltip = rgba(0x0e131b, 242),
    .divider = white(6),
    .border = white(8),
    .text = rgb(0xe6edf3),
    .textDim = rgb(0x8796a8),
    .textDisabled = rgb(0x56606d),
    .textUnfocused = rgb(0x7d8794),
    .face = white(7),
    .faceHover = white(10),
    .facePressed = white(4),
    .accent = rgb(0x398dd0),
    .accentHover = rgb(0x4a9cde),
    .accentLight = rgb(0x6cb6ee),
    .accentText = rgb(0xffffff),
    .selection = rgba(0x398dd0, 56),
    .selectionSide = rgba(0x398dd0, 46),
    .selectionText = rgb(0xffffff),
    .focus = rgb(0x6cb6ee),
    .thumb = white(12),
    .thumbHover = white(20),
    .danger = rgb(0xe5484d),
    .success = rgb(0x7ee0a0),
    .kindFolder = rgb(0x6cb6ee),
    .kindProgram = rgb(0x7ee0a0),
    .kindFile = rgb(0xaab4c0),
    .padding = 8,
    .spacing = 8,
    .radius = 6,
    .fieldRadius = 7,
    .controlHeight = 28,
    .rowHeight = 32,
    .scrollBarWidth = 10,
    .checkSize = 16,
    .iconSize = 16,
    .toolbarHeight = 40,
    .statusHeight = 28,
    .sidebarWidth = 176,
};

const Theme& theme() {
    return kTheme;
}

const Font& Theme::font() const {
    return Font::ui();
}

const Font& Theme::fontMedium() const {
    static Font font;
    static bool tried = false;
    return lazy(font, tried, kTextMedium, 13.0f);
}

const Font& Theme::fontSmall() const {
    static Font font;
    static bool tried = false;
    return lazy(font, tried, kTextRegular, 12.0f);
}

const Font& Theme::fontLabel() const {
    static Font font;
    static bool tried = false;
    return lazy(font, tried, kTextSemiBold, 10.5f);
}

const Font& Theme::fontMono() const {
    static Font font;
    static bool tried = false;
    if (!tried) {
        tried = true;
        font.load_em(kMono, 13.0f);
    }
    return font.valid() ? font : Font::ui();
}

}
