#include <ui/graphics.hpp>

#include <anti_aliasing.hpp>
#include <cstring.hpp>
#include <emmintrin.h>
#include <resizer.hpp>

namespace ui {

static_assert(sizeof(Color) == sizeof(std::uint32_t));
static_assert(sizeof(Color) == sizeof(std::Resizer::RGBA));

Color blend(Color dst, Color src, std::uint8_t alpha) {
    const std::uint32_t inv = 255U - alpha;
    Color out = {};
    out.r = static_cast<std::uint8_t>((dst.r * inv + src.r * alpha) / 255U);
    out.g = static_cast<std::uint8_t>((dst.g * inv + src.g * alpha) / 255U);
    out.b = static_cast<std::uint8_t>((dst.b * inv + src.b * alpha) / 255U);
    out.a = 0xFFU;
    return out;
}

namespace {

int clamp_radius(int width, int height, int radius) {
    if (radius <= 0) {
        return 0;
    }
    if (radius > width / 2) {
        radius = width / 2;
    }
    if (radius > height / 2) {
        radius = height / 2;
    }
    return radius;
}

void fill_row(std::uint32_t* words, int count, std::uint32_t word) {
    int index = 0;
    const __m128i wide = _mm_set1_epi32(static_cast<int>(word));
    for (; index + 4 <= count; index += 4) {
        _mm_storeu_si128(reinterpret_cast<__m128i*>(&words[index]), wide);
    }
    for (; index < count; ++index) {
        words[index] = word;
    }
}

void blend_row(std::uint32_t* words, int count, Color color, std::uint8_t alpha) {
    for (int index = 0; index < count; ++index) {
        words[index] = blend(Color::from_word(words[index]), color, alpha).word();
    }
}

void copy_row_opaque(std::uint32_t* dst, const std::uint32_t* src, int count) {
    int index = 0;
    const __m128i alpha = _mm_set1_epi32(static_cast<int>(0xFF000000U));
    for (; index + 4 <= count; index += 4) {
        const __m128i pixels = _mm_loadu_si128(reinterpret_cast<const __m128i*>(&src[index]));
        _mm_storeu_si128(reinterpret_cast<__m128i*>(&dst[index]), _mm_or_si128(pixels, alpha));
    }
    for (; index < count; ++index) {
        dst[index] = src[index] | 0xFF000000U;
    }
}

}  // namespace

bool in_rounded_rect(const Rect& rect, int radius, int px, int py) {
    if (rect.empty() || !rect.contains(px, py)) {
        return false;
    }
    if (radius <= 0) {
        return true;
    }
    radius = clamp_radius(rect.width, rect.height, radius);
    const int left = rect.x + radius;
    const int right = rect.x + rect.width - radius - 1;
    const int top = rect.y + radius;
    const int bottom = rect.y + rect.height - radius - 1;
    if ((px >= left && px <= right) || (py >= top && py <= bottom)) {
        return true;
    }
    const int dx = px - (px < left ? left : right);
    const int dy = py - (py < top ? top : bottom);
    return dx * dx + dy * dy <= radius * radius;
}

Canvas::Canvas(std::uint32_t* pixels, int width, int height, int pitch)
    : pixels_(pixels), pitch_(pitch), width_(width), height_(height),
      originX_(0), originY_(0), clip_ { 0, 0, width, height } {
}

Canvas Canvas::sub(const Rect& rect) const {
    Canvas out = *this;
    out.originX_ = originX_ + rect.x;
    out.originY_ = originY_ + rect.y;
    out.width_ = rect.width;
    out.height_ = rect.height;
    out.clip_ = clip_.intersect({ out.originX_, out.originY_, rect.width, rect.height });
    return out;
}

Canvas Canvas::clipped(const Rect& rect) const {
    Canvas out = *this;
    out.clip_ = clip_.intersect(rect.offset(originX_, originY_));
    return out;
}

// ---- pixels: everything below works in buffer coordinates -------------------

// Writes color at (x,y), a buffer position inside the clip, blended by alpha
// alone (color's own alpha is not used).
#define UI_PUT(x, y, color, alpha)                                                        \
    do {                                                                                  \
        std::uint32_t& dst_ = pixels_[(y) * pitch_ + (x)];                                \
        if ((alpha) == 255) {                                                             \
            Color solid_ = (color);                                                       \
            solid_.a = 0xFF;                                                              \
            dst_ = solid_.word();                                                         \
        } else if ((alpha) != 0) {                                                        \
            dst_ = blend(Color::from_word(dst_), (color), (alpha)).word();                \
        }                                                                                 \
    } while (0)

static std::uint8_t scale_alpha(std::uint8_t alpha, std::uint8_t by) {
    return by == 255 ? alpha : static_cast<std::uint8_t>((alpha * by + 127U) / 255U);
}

void Canvas::blend_pixel(int x, int y, Color color, std::uint8_t alpha) {
    x += originX_;
    y += originY_;
    if (!clip_.contains(x, y)) {
        return;
    }
    const std::uint8_t a = scale_alpha(alpha, color.a);
    UI_PUT(x, y, color, a);
}

void Canvas::clear(Color color) {
    fill({ 0, 0, width_, height_ }, color);
}

void Canvas::fill(const Rect& rect, Color color) {
    const Rect area = rect.offset(originX_, originY_).intersect(clip_);
    if (area.empty() || color.a == 0) {
        return;
    }
    if (color.a == 255) {
        for (int y = area.y; y < area.bottom(); ++y) {
            fill_row(&pixels_[y * pitch_ + area.x], area.width, color.word());
        }
        return;
    }
    for (int y = area.y; y < area.bottom(); ++y) {
        blend_row(&pixels_[y * pitch_ + area.x], area.width, color, color.a);
    }
}

// Rounded fills: rows clear of the corners are filled (or blended) whole; in
// the corner bands only the corner pixels need their coverage.
static void rounded_fill(std::uint32_t* pixels, int pitch, const Rect& clip, const Rect& shape, int radius,
                         Color color, bool topOnly) {
    if (shape.empty()) {
        return;
    }
    radius = clamp_radius(shape.width, shape.height, radius);
    const Rect area = shape.intersect(clip);
    if (area.empty() || color.a == 0) {
        return;
    }
    const std::uint32_t solid = Color { color.b, color.g, color.r, 0xFF }.word();
    for (int y = area.y; y < area.bottom(); ++y) {
        std::uint32_t* row = &pixels[y * pitch];
        const bool band = radius > 0 && (y < shape.y + radius || (!topOnly && y >= shape.bottom() - radius));
        int solidLeft = area.x;
        int solidRight = area.right();
        if (band) {
            solidLeft = shape.x + radius > area.x ? shape.x + radius : area.x;
            solidRight = shape.right() - radius < area.right() ? shape.right() - radius : area.right();
            for (int x = area.x; x < area.right(); ++x) {
                if (x >= solidLeft && x < solidRight) {
                    continue;
                }
                const std::uint8_t coverage = topOnly
                    ? std::PackedAntiAliasing::top_rounded_rect_coverage(shape.x, shape.y, shape.width, shape.height,
                                                                         radius, x, y)
                    : std::PackedAntiAliasing::rounded_rect_coverage(shape.x, shape.y, shape.width, shape.height,
                                                                     radius, x, y);
                const std::uint8_t alpha = scale_alpha(std::Resizer::alpha_from_coverage(coverage), color.a);
                if (alpha == 255) {
                    row[x] = solid;
                } else if (alpha != 0) {
                    row[x] = blend(Color::from_word(row[x]), color, alpha).word();
                }
            }
        }
        if (solidRight > solidLeft) {
            if (color.a == 255) {
                fill_row(row + solidLeft, solidRight - solidLeft, solid);
            } else {
                blend_row(row + solidLeft, solidRight - solidLeft, color, color.a);
            }
        }
    }
}

void Canvas::fill_rounded(const Rect& rect, int radius, Color color) {
    rounded_fill(pixels_, pitch_, clip_, rect.offset(originX_, originY_), radius, color, false);
}

void Canvas::fill_top_rounded(const Rect& rect, int radius, Color color) {
    rounded_fill(pixels_, pitch_, clip_, rect.offset(originX_, originY_), radius, color, true);
}

void Canvas::fill_circle(Point center, int radius, Color color) {
    if (radius <= 0) {
        return;
    }
    constexpr int kSamples = 8;
    constexpr int kSampleTotal = kSamples * kSamples;
    const int cx = center.x + originX_;
    const int cy = center.y + originY_;
    const Rect area = Rect { cx - radius - 1, cy - radius - 1, 2 * radius + 3, 2 * radius + 3 }.intersect(clip_);
    const double radiusSquared = static_cast<double>(radius * radius);
    for (int y = area.y; y < area.bottom(); ++y) {
        for (int x = area.x; x < area.right(); ++x) {
            int inside = 0;
            for (int sy = 0; sy < kSamples; ++sy) {
                for (int sx = 0; sx < kSamples; ++sx) {
                    const double px = (x + (sx + 0.5) / kSamples) - cx;
                    const double py = (y + (sy + 0.5) / kSamples) - cy;
                    if (px * px + py * py <= radiusSquared) {
                        ++inside;
                    }
                }
            }
            if (inside == 0) {
                continue;
            }
            const std::uint8_t alpha =
                scale_alpha(static_cast<std::uint8_t>((inside * 255 + kSampleTotal / 2) / kSampleTotal), color.a);
            UI_PUT(x, y, color, alpha);
        }
    }
}

void Canvas::frame(const Rect& rect, Color color, int thickness, int radius) {
    if (rect.empty() || thickness <= 0) {
        return;
    }
    if (radius <= 0) {
        fill({ rect.x, rect.y, rect.width, thickness }, color);
        fill({ rect.x, rect.bottom() - thickness, rect.width, thickness }, color);
        fill({ rect.x, rect.y + thickness, thickness, rect.height - 2 * thickness }, color);
        fill({ rect.right() - thickness, rect.y + thickness, thickness, rect.height - 2 * thickness }, color);
        return;
    }
    // The outer rounded shape less the inner one, pixel by pixel.
    const Rect outer = rect.offset(originX_, originY_);
    const Rect inner = outer.inset(thickness);
    const int outerRadius = clamp_radius(outer.width, outer.height, radius);
    const int innerRadius = outerRadius > thickness ? outerRadius - thickness : 0;
    const Rect area = outer.intersect(clip_);
    for (int y = area.y; y < area.bottom(); ++y) {
        for (int x = area.x; x < area.right(); ++x) {
            const int out = std::PackedAntiAliasing::rounded_rect_coverage(outer.x, outer.y, outer.width,
                                                                          outer.height, outerRadius, x, y);
            const int in = inner.empty() ? 0
                : std::PackedAntiAliasing::rounded_rect_coverage(inner.x, inner.y, inner.width, inner.height,
                                                                 innerRadius, x, y);
            const int coverage = out > in ? out - in : 0;
            if (coverage == 0) {
                continue;
            }
            const std::uint8_t alpha = scale_alpha(
                std::Resizer::alpha_from_coverage(static_cast<std::uint8_t>(coverage)), color.a);
            UI_PUT(x, y, color, alpha);
        }
    }
}

void Canvas::line(Point from, Point to, Color color, int thickness) {
    int x0 = from.x;
    int y0 = from.y;
    const int x1 = to.x;
    const int y1 = to.y;
    const int dx = x1 > x0 ? x1 - x0 : x0 - x1;
    const int sx = x0 < x1 ? 1 : -1;
    const int dy = y1 > y0 ? y0 - y1 : y1 - y0;
    const int sy = y0 < y1 ? 1 : -1;
    int error = dx + dy;
    for (;;) {
        fill({ x0 - thickness / 2, y0 - thickness / 2, thickness, thickness }, color);
        if (x0 == x1 && y0 == y1) {
            break;
        }
        const int twice = error * 2;
        if (twice >= dy) {
            error += dy;
            x0 += sx;
        }
        if (twice <= dx) {
            error += dx;
            y0 += sy;
        }
    }
}

void Canvas::gradient(const Rect& rect, Color first, Color last, bool vertical) {
    const Rect shape = rect.offset(originX_, originY_);
    const Rect area = shape.intersect(clip_);
    if (area.empty()) {
        return;
    }
    const int span = vertical ? shape.height : shape.width;
    const std::uint32_t steps = span > 1 ? static_cast<std::uint32_t>(span - 1) : 1U;
    auto at = [&](int offset) {
        const std::uint32_t mix = (static_cast<std::uint32_t>(offset) * 255U) / steps;
        Color c = {};
        c.r = static_cast<std::uint8_t>((first.r * (255U - mix) + last.r * mix) / 255U);
        c.g = static_cast<std::uint8_t>((first.g * (255U - mix) + last.g * mix) / 255U);
        c.b = static_cast<std::uint8_t>((first.b * (255U - mix) + last.b * mix) / 255U);
        c.a = 0xFF;
        return c.word();
    };
    for (int y = area.y; y < area.bottom(); ++y) {
        std::uint32_t* row = &pixels_[y * pitch_ + area.x];
        if (vertical) {
            fill_row(row, area.width, at(y - shape.y));
        } else {
            for (int x = 0; x < area.width; ++x) {
                row[x] = at(area.x + x - shape.x);
            }
        }
    }
}

void Canvas::text(int x, int baseline, const char* text, const Font& font, Color color) {
    if (!font.valid() || !text || color.a == 0) {
        return;
    }
    int penX = x + originX_;
    int baselineY = baseline + originY_;
    const int startPenX = penX;
    for (std::size_t i = 0; text[i] != '\0'; ++i) {
        const unsigned char ch = static_cast<unsigned char>(text[i]);
        if (ch == '\n') {
            penX = startPenX;
            baselineY += font.line_height();
            continue;
        }
        const Font::Glyph& glyph = font.glyph(ch);
        if (glyph.alpha && glyph.width > 0 && glyph.height > 0) {
            const Rect box = { penX + glyph.xOffset, baselineY + glyph.yOffset, glyph.width, glyph.height };
            const Rect area = box.intersect(clip_);
            for (int y = area.y; y < area.bottom(); ++y) {
                const std::uint8_t* coverage = &glyph.alpha[(y - box.y) * glyph.width - box.x];
                std::uint32_t* row = &pixels_[y * pitch_];
                for (int px = area.x; px < area.right(); ++px) {
                    const std::uint8_t alpha = scale_alpha(coverage[px], color.a);
                    if (alpha != 0) {
                        row[px] = blend(Color::from_word(row[px]), color, alpha).word();
                    }
                }
            }
        }
        penX += glyph.advance;
    }
}

void Canvas::text(const Rect& rect, const char* text, const Font& font, Color color, std::uint32_t align) {
    if (!font.valid() || !text) {
        return;
    }
    char fitted[256];
    const int width = font.fit(text, rect.width, fitted, sizeof(fitted));
    int x = rect.x;
    if ((align & 3U) == AlignCenter) {
        x += (rect.width - width) / 2;
    } else if ((align & 3U) == AlignRight) {
        x += rect.width - width;
    }
    int top = rect.y;
    if ((align & 12U) == AlignMiddle) {
        top += (rect.height - font.line_height()) / 2;
    } else if ((align & 12U) == AlignBottom) {
        top += rect.height - font.line_height();
    }
    clipped(rect).text(x, top + font.ascent(), fitted, font, color);
}

void Canvas::image(const Image& image, Point at) {
    if (!image.valid()) {
        return;
    }
    const Rect box = { at.x + originX_, at.y + originY_, image.width(), image.height() };
    const Rect area = box.intersect(clip_);
    for (int y = area.y; y < area.bottom(); ++y) {
        const Color* src = &image.pixels()[(y - box.y) * image.width() - box.x];
        for (int x = area.x; x < area.right(); ++x) {
            UI_PUT(x, y, src[x], src[x].a);
        }
    }
}

void Canvas::image(const Image& image, const Rect& dest) {
    if (!image.valid() || dest.empty()) {
        return;
    }
    if (dest.width == image.width() && dest.height == image.height()) {
        this->image(image, Point { dest.x, dest.y });
        return;
    }
    const Rect box = dest.offset(originX_, originY_);
    const Rect area = box.intersect(clip_);
    const auto* src = reinterpret_cast<const std::Resizer::RGBA*>(image.pixels());
    for (int y = area.y; y < area.bottom(); ++y) {
        for (int x = area.x; x < area.right(); ++x) {
            const std::Resizer::RGBA s = std::Resizer::sample(src, image.width(), image.height(), box.width,
                                                              box.height, x - box.x, y - box.y);
            const Color color = { s.b, s.g, s.r, s.a };
            UI_PUT(x, y, color, color.a);
        }
    }
}

void Canvas::copy(const Image& source, const Rect& from, Point to) {
    if (!source.valid()) {
        return;
    }
    const Rect within = from.intersect({ 0, 0, source.width(), source.height() });
    copy(reinterpret_cast<const std::uint32_t*>(source.pixels()), source.width(), within,
         { to.x + (within.x - from.x), to.y + (within.y - from.y) });
}

void Canvas::copy(const std::uint32_t* source, int sourcePitch, const Rect& from, Point to, bool opaque) {
    if (!source || from.empty()) {
        return;
    }
    const Rect box = { to.x + originX_, to.y + originY_, from.width, from.height };
    const Rect area = box.intersect(clip_);
    for (int y = area.y; y < area.bottom(); ++y) {
        const std::uint32_t* src = &source[(from.y + y - box.y) * sourcePitch + from.x + (area.x - box.x)];
        std::uint32_t* dst = &pixels_[y * pitch_ + area.x];
        if (opaque) {
            copy_row_opaque(dst, src, area.width);
        } else {
            std::memcpy(dst, src, static_cast<std::size_t>(area.width) * sizeof(std::uint32_t));
        }
    }
}

#undef UI_PUT

}  // namespace ui
