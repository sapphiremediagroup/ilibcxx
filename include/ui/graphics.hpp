#pragma once

// The drawing half of the UI library (GDI's role): colours, rectangles, a
// canvas that draws into a window's (or any) pixel buffer, fonts and images.
// Pixels are 32-bit B,G,R,A words, the compositor's surface format.

#include <cstddef.hpp>
#include <cstdint.hpp>

namespace ui {

// ---- colours ---------------------------------------------------------------

// Laid out like a pixel word (B,G,R,A from the low byte).
struct alignas(std::uint32_t) Color {
    std::uint8_t b;
    std::uint8_t g;
    std::uint8_t r;
    std::uint8_t a;

    constexpr std::uint32_t word() const {
        return static_cast<std::uint32_t>(b) | (static_cast<std::uint32_t>(g) << 8) |
               (static_cast<std::uint32_t>(r) << 16) | (static_cast<std::uint32_t>(a) << 24);
    }
    static constexpr Color from_word(std::uint32_t word) {
        return { static_cast<std::uint8_t>(word & 0xFF), static_cast<std::uint8_t>((word >> 8) & 0xFF),
                 static_cast<std::uint8_t>((word >> 16) & 0xFF), static_cast<std::uint8_t>((word >> 24) & 0xFF) };
    }
    constexpr bool operator==(const Color& other) const { return word() == other.word(); }
};

// 0xRRGGBB, opaque or with an alpha.
constexpr Color rgb(std::uint32_t rrggbb) {
    return { static_cast<std::uint8_t>(rrggbb & 0xFF), static_cast<std::uint8_t>((rrggbb >> 8) & 0xFF),
             static_cast<std::uint8_t>((rrggbb >> 16) & 0xFF), 0xFF };
}
constexpr Color rgba(std::uint32_t rrggbb, std::uint8_t alpha) {
    Color color = rgb(rrggbb);
    color.a = alpha;
    return color;
}
constexpr Color kTransparent = { 0, 0, 0, 0 };

// src over dst with `alpha` (0-255); the result is opaque.
Color blend(Color dst, Color src, std::uint8_t alpha);

// ---- geometry --------------------------------------------------------------

struct Point {
    int x;
    int y;
};

struct Size {
    int width;
    int height;
};

struct Rect {
    int x;
    int y;
    int width;
    int height;

    constexpr int right() const { return x + width; }
    constexpr int bottom() const { return y + height; }
    constexpr bool empty() const { return width <= 0 || height <= 0; }
    constexpr bool contains(int px, int py) const {
        return px >= x && py >= y && px < x + width && py < y + height;
    }
    // The overlap, or an empty rectangle at (0,0).
    constexpr Rect intersect(const Rect& o) const {
        const int l = x > o.x ? x : o.x;
        const int t = y > o.y ? y : o.y;
        const int r = right() < o.right() ? right() : o.right();
        const int b = bottom() < o.bottom() ? bottom() : o.bottom();
        return (r <= l || b <= t) ? Rect { 0, 0, 0, 0 } : Rect { l, t, r - l, b - t };
    }
    // The smallest rectangle holding both (an empty one does not count).
    constexpr Rect unite(const Rect& o) const {
        if (empty()) return o;
        if (o.empty()) return *this;
        const int l = x < o.x ? x : o.x;
        const int t = y < o.y ? y : o.y;
        const int r = right() > o.right() ? right() : o.right();
        const int b = bottom() > o.bottom() ? bottom() : o.bottom();
        return { l, t, r - l, b - t };
    }
    constexpr Rect offset(int dx, int dy) const { return { x + dx, y + dy, width, height }; }
    constexpr Rect inset(int d) const { return { x + d, y + d, width - 2 * d, height - 2 * d }; }
};

// Whether (px,py) is inside the rectangle with corners of `radius` (the
// test the rounded fills use for their solid part).
bool in_rounded_rect(const Rect& rect, int radius, int px, int py);

// ---- fonts -----------------------------------------------------------------

// A TrueType font at one pixel size, its printable ASCII glyphs rendered
// once (2x supersampled) and kept. Not copyable; Font::ui() is shared.
class Font {
public:
    struct Glyph {
        const std::uint8_t* alpha;   // width x height coverage, or null
        int width;
        int height;
        int xOffset;                 // from the pen, to the bitmap's top left
        int yOffset;                 // from the baseline
        int advance;
    };

    Font() = default;
    Font(const Font&) = delete;
    Font& operator=(const Font&) = delete;
    Font(Font&& other) noexcept;
    Font& operator=(Font&& other) noexcept;
    ~Font();

    bool load(const char* path, std::uint32_t pixelHeight);
    void reset();

    bool valid() const { return data_ != nullptr; }
    int line_height() const;
    int ascent() const;              // baseline below the top of a line
    int width(const char* text) const;
    int width(const char* text, std::size_t length) const;
    // Characters outside 32-126 have no glyph (an empty one, 6 px wide).
    const Glyph& glyph(unsigned char ch) const;

    // `text` cut to fit `maxWidth` pixels, ending in ".." when cut, into
    // `out` (always terminated). Returns the width of what was written.
    int fit(const char* text, int maxWidth, char* out, std::size_t outSize) const;

    // The desktop's UI font (JetBrains Mono, 13 px), loaded on first use.
    static Font& ui();

private:
    struct Data;
    Data* data_ = nullptr;
};

// ---- images ----------------------------------------------------------------

class Canvas;

// An owned pixel buffer: loaded from a file, scaled, drawn from, and drawn
// into (canvas()). Not copyable.
class Image {
public:
    Image() = default;
    Image(const Image&) = delete;
    Image& operator=(const Image&) = delete;
    Image(Image&& other) noexcept;
    Image& operator=(Image&& other) noexcept;
    ~Image();

    // A width x height image filled with `fill`.
    bool create(int width, int height, Color fill = kTransparent);
    // PNG or JPEG (stb_image), with its alpha.
    bool load(const char* path);
    // An SVG file rendered at width x height.
    bool load_svg(const char* path, int width, int height);
    // Scaled to width x height (area average down, bilinear up).
    bool resize(int width, int height);
    // This image becomes a copy of `source` scaled to width x height.
    bool scaled_from(const Image& source, int width, int height);
    void reset();

    bool valid() const { return pixels_ != nullptr; }
    int width() const { return width_; }
    int height() const { return height_; }
    Color* pixels() { return pixels_; }
    const Color* pixels() const { return pixels_; }
    Canvas canvas();

private:
    Color* pixels_ = nullptr;
    int width_ = 0;
    int height_ = 0;
};

// ---- canvas ----------------------------------------------------------------

enum Align : std::uint32_t {
    AlignLeft = 0,
    AlignCenter = 1,
    AlignRight = 2,
    AlignTop = 0,
    AlignMiddle = 4,
    AlignBottom = 8,
};

// Draws into a pixel buffer it does not own. Coordinates are relative to
// the canvas's origin, and nothing lands outside its clip rectangle (which
// never leaves the buffer). sub() and clipped() make narrower canvases on
// the same pixels; copying a Canvas is cheap.
class Canvas {
public:
    Canvas() = default;
    // `pitch` in pixels.
    Canvas(std::uint32_t* pixels, int width, int height, int pitch);

    std::uint32_t* pixels() const { return pixels_; }
    int pitch() const { return pitch_; }
    // The canvas's own size (a sub canvas's rectangle).
    int width() const { return width_; }
    int height() const { return height_; }
    // The clip, in this canvas's coordinates.
    Rect clip() const { return clip_.offset(-originX_, -originY_); }

    // Origin moved to rect's corner, clip narrowed to it.
    Canvas sub(const Rect& rect) const;
    // Same origin, clip narrowed to rect.
    Canvas clipped(const Rect& rect) const;

    void clear(Color color);
    // Blended when color.a < 255.
    void fill(const Rect& rect, Color color);
    void fill_rounded(const Rect& rect, int radius, Color color);
    // Rounded at the top corners only (title bars).
    void fill_top_rounded(const Rect& rect, int radius, Color color);
    void fill_circle(Point center, int radius, Color color);
    // An outline `thickness` wide inside rect; rounded when radius > 0.
    void frame(const Rect& rect, Color color, int thickness = 1, int radius = 0);
    // A square pen `thickness` wide.
    void line(Point from, Point to, Color color, int thickness = 1);
    // From `first` to `last` across rect, top to bottom (or left to right).
    void gradient(const Rect& rect, Color first, Color last, bool vertical = true);
    // One pixel, blended with `alpha` (times color.a).
    void blend_pixel(int x, int y, Color color, std::uint8_t alpha);

    // Text from the pen at (x, baseline); '\n' starts a new line.
    void text(int x, int baseline, const char* text, const Font& font, Color color);
    // Text placed in rect by `align` (AlignLeft|AlignMiddle ...), cut with
    // ".." when wider than rect.
    void text(const Rect& rect, const char* text, const Font& font, Color color,
              std::uint32_t align = AlignLeft | AlignMiddle);

    // The image at `at`, 1:1, blended by its alpha.
    void image(const Image& image, Point at);
    // The image scaled into dest, blended by its alpha.
    void image(const Image& image, const Rect& dest);
    // Pixels copied as they are (no blending): `from` of the source to `to`.
    void copy(const Image& source, const Rect& from, Point to);
    // The same from a raw buffer (pitch in pixels); `opaque` sets alpha to 255.
    void copy(const std::uint32_t* source, int sourcePitch, const Rect& from, Point to, bool opaque = false);

private:
    std::uint32_t* pixels_ = nullptr;
    int pitch_ = 0;
    int width_ = 0;
    int height_ = 0;
    int originX_ = 0;     // buffer coordinates of the canvas's (0,0)
    int originY_ = 0;
    Rect clip_ = {};      // in buffer coordinates
};

}
