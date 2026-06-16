#pragma once

#include <cstddef.hpp>
#include <cstdint.hpp>
#include <syscall.hpp>

namespace instant {

inline constexpr std::uint32_t kDefaultUIFontPixelHeight = 15;
inline constexpr unsigned char kFirstCachedGlyph = 32;
inline constexpr unsigned char kLastCachedGlyph = 126;
inline constexpr std::size_t kCachedGlyphCount = static_cast<std::size_t>(kLastCachedGlyph - kFirstCachedGlyph + 1);

struct UIGlyphBitmap {
    unsigned char* pixels;
    int width;
    int height;
    int xOffset;
    int yOffset;
    int advance;
};

struct UIFont {
    bool valid;
    std::Handle service;
    std::uint32_t pixelHeight;
    int cellWidth;
    int lineHeight;
    int baseline;
    UIGlyphBitmap glyphs[kCachedGlyphCount];
};

extern UIFont gUIFont;

bool initialize_ui_font(UIFont* font, std::uint32_t pixelHeight = kDefaultUIFontPixelHeight);
bool initialize_ui_font(std::uint32_t pixelHeight = kDefaultUIFontPixelHeight);
void destroy_ui_font(UIFont* font);
void destroy_ui_font();

void draw_text(
    std::uint32_t* pixels,
    std::uint32_t surfaceWidth,
    std::uint32_t surfaceHeight,
    UIFont& font,
    int x,
    int baselineY,
    const char* text,
    std::uint32_t color
);

void draw_text(
    std::uint32_t* pixels,
    std::uint32_t surfaceWidth,
    std::uint32_t surfaceHeight,
    int x,
    int baselineY,
    const char* text,
    std::uint32_t color
);

}
