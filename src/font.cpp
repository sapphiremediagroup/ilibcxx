#include <instant/font.hpp>

#include <cstring.hpp>
#include <new.hpp>
#include <service_protocol.hpp>

namespace {

constexpr std::uint64_t fail = static_cast<std::uint64_t>(-1);

std::Handle connect_service_with_retry(const char* name) noexcept {
    for (int attempt = 0; attempt < 500; ++attempt) {
        const std::Handle handle = std::service_connect(name);
        if (!std::syscall_failed(handle)) {
            return handle;
        }
        std::yield();
    }
    return fail;
}

std::uint8_t color_r(std::uint32_t color) noexcept {
    return static_cast<std::uint8_t>((color >> 16) & 0xFFU);
}

std::uint8_t color_g(std::uint32_t color) noexcept {
    return static_cast<std::uint8_t>((color >> 8) & 0xFFU);
}

std::uint8_t color_b(std::uint32_t color) noexcept {
    return static_cast<std::uint8_t>(color & 0xFFU);
}

std::uint32_t pack_rgb(std::uint8_t r, std::uint8_t g, std::uint8_t b) noexcept {
    return (static_cast<std::uint32_t>(r) << 16) |
           (static_cast<std::uint32_t>(g) << 8) |
           static_cast<std::uint32_t>(b);
}

std::uint32_t blend_rgb(std::uint32_t dst, std::uint32_t src, std::uint8_t alpha) noexcept {
    const std::uint32_t inv = 255U - alpha;
    const std::uint32_t r = (color_r(dst) * inv + color_r(src) * alpha) / 255U;
    const std::uint32_t g = (color_g(dst) * inv + color_g(src) * alpha) / 255U;
    const std::uint32_t b = (color_b(dst) * inv + color_b(src) * alpha) / 255U;
    return pack_rgb(static_cast<std::uint8_t>(r), static_cast<std::uint8_t>(g), static_cast<std::uint8_t>(b));
}

}

namespace instant {

UIFont gUIFont = {};

bool initialize_ui_font(UIFont* font, std::uint32_t pixelHeight) {
    if (!font) {
        return false;
    }

    destroy_ui_font(font);
    std::memset(font, 0, sizeof(*font));
    font->service = connect_service_with_retry(std::services::font_manager::NAME);
    if (font->service == fail) {
        return false;
    }

    std::services::font_manager::FontInfoRequest infoRequest = {};
    infoRequest.header.version = std::services::font_manager::VERSION;
    infoRequest.header.opcode = static_cast<std::uint16_t>(std::services::font_manager::Opcode::GetFontInfo);
    infoRequest.pixelHeight = pixelHeight;

    std::IPCMessage message = {};
    if (!std::services::encode_request(&message, infoRequest)) {
        destroy_ui_font(font);
        return false;
    }

    std::services::font_manager::FontInfoReply infoReply = {};
    std::uint64_t replySize = 0;
    if (std::queue_request(font->service, &message, &infoReply, sizeof(infoReply), &replySize) == fail ||
        replySize < sizeof(infoReply) ||
        infoReply.status != std::services::STATUS_OK) {
        destroy_ui_font(font);
        return false;
    }

    font->pixelHeight = pixelHeight;
    font->baseline = infoReply.baseline;
    font->lineHeight = infoReply.lineHeight;
    font->cellWidth = infoReply.cellWidth;

    for (std::size_t index = 0; index < kCachedGlyphCount; ++index) {
        const int codepoint = static_cast<int>(kFirstCachedGlyph + index);
        UIGlyphBitmap& glyph = font->glyphs[index];

        std::services::font_manager::GlyphMetricsRequest metricsRequest = {};
        metricsRequest.header.version = std::services::font_manager::VERSION;
        metricsRequest.header.opcode = static_cast<std::uint16_t>(std::services::font_manager::Opcode::GetGlyphMetrics);
        metricsRequest.pixelHeight = pixelHeight;
        metricsRequest.codepoint = static_cast<std::uint32_t>(codepoint);
        if (!std::services::encode_request(&message, metricsRequest)) {
            destroy_ui_font(font);
            return false;
        }

        std::services::font_manager::GlyphMetricsReply metricsReply = {};
        replySize = 0;
        if (std::queue_request(font->service, &message, &metricsReply, sizeof(metricsReply), &replySize) == fail ||
            replySize < sizeof(metricsReply) ||
            metricsReply.status != std::services::STATUS_OK) {
            destroy_ui_font(font);
            return false;
        }

        glyph.width = metricsReply.width;
        glyph.height = metricsReply.height;
        glyph.xOffset = metricsReply.xOffset;
        glyph.yOffset = metricsReply.yOffset;
        glyph.advance = metricsReply.advance;
        if (glyph.advance <= 0) {
            glyph.advance = font->cellWidth;
        }

        const std::uint64_t pixelCount = static_cast<std::uint64_t>(glyph.width) * static_cast<std::uint64_t>(glyph.height);
        if (glyph.width <= 0 || glyph.height <= 0 ||
            glyph.width > static_cast<int>(std::services::font_manager::MAX_GLYPH_ROW_PIXELS) ||
            pixelCount == 0) {
            glyph.width = 0;
            glyph.height = 0;
            glyph.xOffset = 0;
            glyph.yOffset = 0;
            continue;
        }

        glyph.pixels = new (std::nothrow) unsigned char[static_cast<std::size_t>(pixelCount)];
        if (!glyph.pixels) {
            destroy_ui_font(font);
            return false;
        }

        for (int row = 0; row < glyph.height; ++row) {
            std::services::font_manager::GlyphRowRequest rowRequest = {};
            rowRequest.header.version = std::services::font_manager::VERSION;
            rowRequest.header.opcode = static_cast<std::uint16_t>(std::services::font_manager::Opcode::GetGlyphRow);
            rowRequest.pixelHeight = pixelHeight;
            rowRequest.codepoint = static_cast<std::uint32_t>(codepoint);
            rowRequest.row = static_cast<std::uint32_t>(row);
            if (!std::services::encode_request(&message, rowRequest)) {
                destroy_ui_font(font);
                return false;
            }

            std::services::font_manager::GlyphRowReply rowReply = {};
            replySize = 0;
            if (std::queue_request(font->service, &message, &rowReply, sizeof(rowReply), &replySize) == fail ||
                replySize < sizeof(rowReply) ||
                rowReply.status != std::services::STATUS_OK ||
                rowReply.width != static_cast<std::uint32_t>(glyph.width)) {
                destroy_ui_font(font);
                return false;
            }

            std::memcpy(
                glyph.pixels + (static_cast<std::size_t>(row) * static_cast<std::size_t>(glyph.width)),
                rowReply.pixels,
                static_cast<std::size_t>(glyph.width)
            );
        }
    }

    font->valid = true;
    return true;
}

bool initialize_ui_font(std::uint32_t pixelHeight) {
    return initialize_ui_font(&gUIFont, pixelHeight);
}

void destroy_ui_font(UIFont* font) {
    if (!font) {
        return;
    }

    for (std::size_t index = 0; index < kCachedGlyphCount; ++index) {
        delete[] font->glyphs[index].pixels;
        font->glyphs[index].pixels = nullptr;
    }
    if (font->service != fail && font->service != 0) {
        std::close(font->service);
    }
    std::memset(font, 0, sizeof(*font));
}

void destroy_ui_font() {
    destroy_ui_font(&gUIFont);
}

void draw_text(
    std::uint32_t* pixels,
    std::uint32_t surfaceWidth,
    std::uint32_t surfaceHeight,
    UIFont& font,
    int x,
    int baselineY,
    const char* text,
    std::uint32_t color
) {
    if (!pixels || !font.valid || !text) {
        return;
    }

    int penX = x;
    for (std::size_t index = 0; text[index] != '\0'; ++index) {
        const unsigned char ch = static_cast<unsigned char>(text[index]);
        if (ch < kFirstCachedGlyph || ch > kLastCachedGlyph) {
            penX += font.cellWidth;
            continue;
        }

        const UIGlyphBitmap& glyph = font.glyphs[ch - kFirstCachedGlyph];
        if (!glyph.pixels || glyph.width <= 0 || glyph.height <= 0) {
            penX += glyph.advance > 0 ? glyph.advance : font.cellWidth;
            continue;
        }

        const int startX = penX + glyph.xOffset;
        const int startY = baselineY + glyph.yOffset;
        for (int drawY = 0; drawY < glyph.height; ++drawY) {
            const int dstY = startY + drawY;
            if (dstY < 0 || dstY >= static_cast<int>(surfaceHeight)) {
                continue;
            }

            for (int drawX = 0; drawX < glyph.width; ++drawX) {
                const int dstX = startX + drawX;
                if (dstX < 0 || dstX >= static_cast<int>(surfaceWidth)) {
                    continue;
                }

                const std::uint8_t alpha = glyph.pixels[drawY * glyph.width + drawX];
                if (alpha == 0) {
                    continue;
                }

                std::uint32_t& dst = pixels[dstY * surfaceWidth + dstX];
                dst = blend_rgb(dst, color, alpha);
            }
        }

        penX += glyph.advance > 0 ? glyph.advance : font.cellWidth;
    }
}

void draw_text(
    std::uint32_t* pixels,
    std::uint32_t surfaceWidth,
    std::uint32_t surfaceHeight,
    int x,
    int baselineY,
    const char* text,
    std::uint32_t color
) {
    draw_text(pixels, surfaceWidth, surfaceHeight, gUIFont, x, baselineY, text, color);
}

}
