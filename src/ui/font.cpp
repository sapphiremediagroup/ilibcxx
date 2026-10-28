#include <ui/graphics.hpp>

#include <cmath.hpp>
#include <cstring.hpp>
#include <fcntl.h>
#include <math.h>
#include <new.hpp>
#include <syscall.hpp>

// stb_truetype, private to this file, with the few maths functions it needs.
namespace ui_stb {
static double cuberoot(double value) {
    if (value == 0.0) {
        return 0.0;
    }
    const double sign = value < 0.0 ? -1.0 : 1.0;
    double guess = std::fabs(value);
    if (guess < 1.0) {
        guess = 1.0;
    }
    for (int iteration = 0; iteration < 24; ++iteration) {
        guess = (2.0 * guess + std::fabs(value) / (guess * guess)) / 3.0;
    }
    return sign * guess;
}

static double fmod(double value, double modulus) {
    return std::fmod(value, modulus);
}

static double pow(double value, double exponent) {
    const double diff = exponent - 1.0 / 3.0;
    if (diff > -0.000001 && diff < 0.000001) {
        return cuberoot(value);
    }
    if (exponent == 0.0) {
        return 1.0;
    }
    if (exponent == 1.0) {
        return value;
    }
    const double integer = std::floor(exponent);
    if (exponent == integer) {
        const bool negative = integer < 0.0;
        const std::uint64_t count = static_cast<std::uint64_t>(negative ? -integer : integer);
        double result = 1.0;
        for (std::uint64_t index = 0; index < count; ++index) {
            result *= value;
        }
        return negative ? (result == 0.0 ? 0.0 : 1.0 / result) : result;
    }
    return 1.0;
}

static double acos(double value) {
    if (value <= -1.0) {
        return 3.14159265358979323846;
    }
    if (value >= 1.0) {
        return 0.0;
    }
    return static_cast<double>(acosf(static_cast<float>(value)));
}
}  // namespace ui_stb

// stb wants a NULL that converts to any pointer type.
#ifdef NULL
#undef NULL
#endif
#define NULL 0
#define STBTT_ifloor(x) static_cast<int>(std::floor(x))
#define STBTT_iceil(x) static_cast<int>(std::ceil(x))
#define STBTT_sqrt(x) std::sqrt(x)
#define STBTT_fmod(x, y) ui_stb::fmod((x), (y))
#define STBTT_pow(x, y) ui_stb::pow((x), (y))
#define STBTT_cos(x) std::cos(x)
#define STBTT_acos(x) ui_stb::acos((x))
#define STBTT_fabs(x) std::fabs(x)
#define STBTT_STATIC
#define STB_TRUETYPE_IMPLEMENTATION
#include <stb_truetype.h>

namespace ui {

namespace {
constexpr int kFirstGlyph = 32;
constexpr int kLastGlyph = 126;
constexpr int kSupersample = 2;
constexpr int kNoGlyphAdvance = 6;
constexpr char kUIFontPath[] = "/bin/JetBrainsMono-Regular.ttf";
constexpr std::uint32_t kUIFontPixelHeight = 13;

int floor_div(int value, int divisor) {
    if (value >= 0) {
        return value / divisor;
    }
    return -(((-value) + divisor - 1) / divisor);
}

std::uint8_t downsample(const unsigned char* bitmap, int width, int height, int x, int y) {
    int sum = 0;
    int count = 0;
    for (int sy = 0; sy < kSupersample; ++sy) {
        const int sourceY = y * kSupersample + sy;
        if (sourceY >= height) {
            continue;
        }
        for (int sx = 0; sx < kSupersample; ++sx) {
            const int sourceX = x * kSupersample + sx;
            if (sourceX >= width) {
                continue;
            }
            sum += bitmap[sourceY * width + sourceX];
            ++count;
        }
    }
    return count == 0 ? 0 : static_cast<std::uint8_t>((sum + count / 2) / count);
}

const Font::Glyph kNoGlyph = { nullptr, 0, 0, 0, 0, kNoGlyphAdvance };
}  // namespace

struct Font::Data {
    unsigned char* file;
    stbtt_fontinfo info;
    float scale;
    int lineHeight;
    int ascent;
    Glyph glyphs[kLastGlyph - kFirstGlyph + 1];
};

static void free_data(Font::Glyph* glyphs, std::size_t count) {
    for (std::size_t i = 0; i < count; ++i) {
        delete[] glyphs[i].alpha;
        glyphs[i].alpha = nullptr;
    }
}

Font::Font(Font&& other) noexcept : data_(other.data_) {
    other.data_ = nullptr;
}

Font& Font::operator=(Font&& other) noexcept {
    if (this != &other) {
        reset();
        data_ = other.data_;
        other.data_ = nullptr;
    }
    return *this;
}

Font::~Font() {
    reset();
}

void Font::reset() {
    if (!data_) {
        return;
    }
    free_data(data_->glyphs, sizeof(data_->glyphs) / sizeof(data_->glyphs[0]));
    delete[] data_->file;
    delete data_;
    data_ = nullptr;
}

bool Font::load(const char* path, std::uint32_t pixelHeight) {
    reset();
    std::Stat stat = {};
    if (!path || std::stat(path, &stat) == static_cast<std::uint64_t>(-1) || stat.st_size == 0) {
        return false;
    }
    const std::Handle file = std::open(path, O_RDONLY);
    if (file == static_cast<std::Handle>(-1)) {
        return false;
    }
    Data* data = new (std::nothrow) Data {};
    const std::size_t size = static_cast<std::size_t>(stat.st_size);
    unsigned char* bytes = data ? new (std::nothrow) unsigned char[size] : nullptr;
    std::size_t total = 0;
    while (bytes && total < size) {
        const std::uint64_t got = std::read(file, bytes + total, size - total);
        if (got == static_cast<std::uint64_t>(-1) || got == 0) {
            break;
        }
        total += static_cast<std::size_t>(got);
    }
    std::close(file);
    if (!bytes || total != size) {
        delete[] bytes;
        delete data;
        return false;
    }
    data->file = bytes;

    const int offset = stbtt_GetFontOffsetForIndex(bytes, 0);
    if (offset < 0 || stbtt_InitFont(&data->info, bytes, offset) == 0) {
        delete[] bytes;
        delete data;
        return false;
    }
    data->scale = stbtt_ScaleForPixelHeight(&data->info, static_cast<float>(pixelHeight));
    int ascent = 0;
    int descent = 0;
    int lineGap = 0;
    stbtt_GetFontVMetrics(&data->info, &ascent, &descent, &lineGap);
    data->ascent = static_cast<int>(ascent * data->scale + 0.999f);
    data->lineHeight = static_cast<int>((ascent - descent + lineGap) * data->scale + 0.999f);
    if (data->lineHeight <= 0) {
        data->lineHeight = static_cast<int>(pixelHeight) + 4;
    }

    const float glyphScale = data->scale * static_cast<float>(kSupersample);
    for (int ch = kFirstGlyph; ch <= kLastGlyph; ++ch) {
        Glyph& glyph = data->glyphs[ch - kFirstGlyph];
        int advance = 0;
        int bearing = 0;
        stbtt_GetCodepointHMetrics(&data->info, ch, &advance, &bearing);
        glyph.advance = static_cast<int>(advance * data->scale + 0.999f);
        if (glyph.advance <= 0) {
            glyph.advance = kNoGlyphAdvance;
        }

        int width = 0;
        int height = 0;
        int xOffset = 0;
        int yOffset = 0;
        unsigned char* bitmap = stbtt_GetCodepointBitmap(&data->info, glyphScale, glyphScale, ch,
                                                         &width, &height, &xOffset, &yOffset);
        if (!bitmap || width <= 0 || height <= 0) {
            if (bitmap) {
                stbtt_FreeBitmap(bitmap, nullptr);
            }
            continue;
        }
        glyph.width = (width + kSupersample - 1) / kSupersample;
        glyph.height = (height + kSupersample - 1) / kSupersample;
        glyph.xOffset = floor_div(xOffset, kSupersample);
        glyph.yOffset = floor_div(yOffset, kSupersample);
        auto* alpha = new (std::nothrow) std::uint8_t[static_cast<std::size_t>(glyph.width) * glyph.height];
        if (alpha) {
            for (int y = 0; y < glyph.height; ++y) {
                for (int x = 0; x < glyph.width; ++x) {
                    alpha[y * glyph.width + x] = downsample(bitmap, width, height, x, y);
                }
            }
        } else {
            glyph.width = 0;
            glyph.height = 0;
        }
        glyph.alpha = alpha;
        stbtt_FreeBitmap(bitmap, nullptr);
    }

    data_ = data;
    return true;
}

int Font::line_height() const {
    return data_ ? data_->lineHeight : 0;
}

int Font::ascent() const {
    return data_ ? data_->ascent : 0;
}

const Font::Glyph& Font::glyph(unsigned char ch) const {
    if (!data_ || ch < kFirstGlyph || ch > kLastGlyph) {
        return kNoGlyph;
    }
    return data_->glyphs[ch - kFirstGlyph];
}

int Font::width(const char* text, std::size_t length) const {
    if (!data_ || !text) {
        return 0;
    }
    int total = 0;
    for (std::size_t i = 0; i < length && text[i] != '\0'; ++i) {
        total += glyph(static_cast<unsigned char>(text[i])).advance;
    }
    return total;
}

int Font::width(const char* text) const {
    return width(text, static_cast<std::size_t>(-1));
}

int Font::fit(const char* text, int maxWidth, char* out, std::size_t outSize) const {
    if (!out || outSize == 0) {
        return 0;
    }
    out[0] = '\0';
    if (!text) {
        return 0;
    }
    std::size_t length = 0;
    while (text[length] != '\0' && length + 1 < outSize) {
        ++length;
    }
    if (width(text, length) <= maxWidth && text[length] == '\0') {
        std::memcpy(out, text, length);
        out[length] = '\0';
        return width(out);
    }
    // Cut: as much as fits in front of "..".
    const int dots = width("..");
    int used = 0;
    std::size_t keep = 0;
    while (keep < length && keep + 3 <= outSize) {
        const int next = glyph(static_cast<unsigned char>(text[keep])).advance;
        if (used + next + dots > maxWidth) {
            break;
        }
        used += next;
        ++keep;
    }
    if (used + dots > maxWidth || keep + 3 > outSize) {
        return 0;
    }
    std::memcpy(out, text, keep);
    out[keep] = '.';
    out[keep + 1] = '.';
    out[keep + 2] = '\0';
    return used + dots;
}

Font& Font::ui() {
    static Font font;
    static bool tried = false;
    if (!tried) {
        tried = true;
        font.load(kUIFontPath, kUIFontPixelHeight);
    }
    return font;
}

}  // namespace ui
