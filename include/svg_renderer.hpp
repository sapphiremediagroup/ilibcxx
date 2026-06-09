#pragma once

#include <cstddef.hpp>
#include <cstdint.hpp>

namespace std {

class SvgRenderer {
public:
    struct PixelBuffer {
        std::uint32_t* pixels;
        int width;
        int height;
        int pitch;
    };

    struct RenderOptions {
        int x;
        int y;
        int width;
        int height;
        bool clear;
        std::uint32_t clearColor;
    };

    static RenderOptions default_options() noexcept;

    static bool render(const char* svg, const PixelBuffer& target);
    static bool render(const char* svg, const PixelBuffer& target, const RenderOptions& options);
    static bool render(const char* svg, std::size_t length, const PixelBuffer& target);
    static bool render(const char* svg, std::size_t length, const PixelBuffer& target, const RenderOptions& options);
};

}
