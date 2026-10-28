#include <ui/graphics.hpp>

#include <cstring.hpp>
#include <fcntl.h>
#include <new.hpp>
#include <resizer.hpp>
#include <svg_renderer.hpp>
#include <syscall.hpp>

// stb wants a NULL that converts to any pointer type.
#ifdef NULL
#undef NULL
#endif
#define NULL 0
// stb_image, private to this file.
#define STBI_ASSERT(x) ((void)0)
#define STBI_NO_STDIO
#define STBI_NO_THREAD_LOCALS
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#define STBI_NO_LINEAR
#define STBI_NO_HDR
#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

namespace ui {

namespace {

// The whole file, or null; `size` gets its length.
unsigned char* read_file(const char* path, std::size_t* size) {
    std::Stat stat = {};
    if (!path || std::stat(path, &stat) == static_cast<std::uint64_t>(-1) || stat.st_size == 0) {
        return nullptr;
    }
    const std::Handle file = std::open(path, O_RDONLY);
    if (file == static_cast<std::Handle>(-1)) {
        return nullptr;
    }
    const std::size_t length = static_cast<std::size_t>(stat.st_size);
    unsigned char* bytes = new (std::nothrow) unsigned char[length + 1];
    std::size_t total = 0;
    while (bytes && total < length) {
        const std::uint64_t got = std::read(file, bytes + total, length - total);
        if (got == static_cast<std::uint64_t>(-1) || got == 0) {
            break;
        }
        total += static_cast<std::size_t>(got);
    }
    std::close(file);
    if (!bytes || total != length) {
        delete[] bytes;
        return nullptr;
    }
    bytes[length] = '\0';
    *size = length;
    return bytes;
}

}  // namespace

Image::Image(Image&& other) noexcept : pixels_(other.pixels_), width_(other.width_), height_(other.height_) {
    other.pixels_ = nullptr;
    other.width_ = 0;
    other.height_ = 0;
}

Image& Image::operator=(Image&& other) noexcept {
    if (this != &other) {
        reset();
        pixels_ = other.pixels_;
        width_ = other.width_;
        height_ = other.height_;
        other.pixels_ = nullptr;
        other.width_ = 0;
        other.height_ = 0;
    }
    return *this;
}

Image::~Image() {
    reset();
}

void Image::reset() {
    delete[] pixels_;
    pixels_ = nullptr;
    width_ = 0;
    height_ = 0;
}

bool Image::create(int width, int height, Color fill) {
    reset();
    if (width <= 0 || height <= 0) {
        return false;
    }
    pixels_ = new (std::nothrow) Color[static_cast<std::size_t>(width) * static_cast<std::size_t>(height)];
    if (!pixels_) {
        return false;
    }
    width_ = width;
    height_ = height;
    for (std::size_t i = 0; i < static_cast<std::size_t>(width) * static_cast<std::size_t>(height); ++i) {
        pixels_[i] = fill;
    }
    return true;
}

bool Image::load(const char* path) {
    reset();
    std::size_t size = 0;
    unsigned char* encoded = read_file(path, &size);
    if (!encoded) {
        return false;
    }
    int width = 0;
    int height = 0;
    int components = 0;
    unsigned char* rgba = stbi_load_from_memory(encoded, static_cast<int>(size), &width, &height, &components, 4);
    delete[] encoded;
    if (!rgba || width <= 0 || height <= 0) {
        if (rgba) {
            stbi_image_free(rgba);
        }
        return false;
    }
    if (!create(width, height)) {
        stbi_image_free(rgba);
        return false;
    }
    for (std::size_t i = 0; i < static_cast<std::size_t>(width) * static_cast<std::size_t>(height); ++i) {
        pixels_[i] = { rgba[i * 4 + 2], rgba[i * 4 + 1], rgba[i * 4], rgba[i * 4 + 3] };
    }
    stbi_image_free(rgba);
    return true;
}

bool Image::load_svg(const char* path, int width, int height) {
    std::size_t size = 0;
    unsigned char* text = read_file(path, &size);
    if (!text) {
        reset();
        return false;
    }
    bool ok = create(width, height);
    if (ok) {
        const std::SvgRenderer::PixelBuffer target = {
            reinterpret_cast<std::uint32_t*>(pixels_), width, height, width };
        std::SvgRenderer::RenderOptions options = std::SvgRenderer::default_options();
        options.x = 0;
        options.y = 0;
        options.width = width;
        options.height = height;
        options.clear = true;
        options.clearColor = 0;
        ok = std::SvgRenderer::render(reinterpret_cast<const char*>(text), size, target, options);
    }
    delete[] text;
    if (!ok) {
        reset();
    }
    return ok;
}

bool Image::resize(int width, int height) {
    if (!pixels_ || width <= 0 || height <= 0) {
        return false;
    }
    if (width == width_ && height == height_) {
        return true;
    }
    Color* resized = new (std::nothrow) Color[static_cast<std::size_t>(width) * static_cast<std::size_t>(height)];
    if (!resized) {
        return false;
    }
    if (!std::Resizer::resize(reinterpret_cast<const std::Resizer::RGBA*>(pixels_), width_, height_,
                              reinterpret_cast<std::Resizer::RGBA*>(resized), width, height, width)) {
        delete[] resized;
        return false;
    }
    delete[] pixels_;
    pixels_ = resized;
    width_ = width;
    height_ = height;
    return true;
}

bool Image::scaled_from(const Image& source, int width, int height) {
    if (!source.valid() || !create(width, height)) {
        return false;
    }
    if (!std::Resizer::resize(reinterpret_cast<const std::Resizer::RGBA*>(source.pixels()), source.width(),
                              source.height(), reinterpret_cast<std::Resizer::RGBA*>(pixels_), width, height, width)) {
        reset();
        return false;
    }
    return true;
}

Canvas Image::canvas() {
    return Canvas(reinterpret_cast<std::uint32_t*>(pixels_), width_, height_, width_);
}

}  // namespace ui
