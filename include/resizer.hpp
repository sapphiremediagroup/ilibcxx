#pragma once

#include <stdint.h>

namespace std {

class Resizer {
public:
    struct RGBA {
        uint8_t b;
        uint8_t g;
        uint8_t r;
        uint8_t a;
    };

    static RGBA sample(const RGBA* src, int srcWidth, int srcHeight,
                       int dstWidth, int dstHeight, int dstX, int dstY) {
        if (!src || srcWidth <= 0 || srcHeight <= 0 || dstWidth <= 0 || dstHeight <= 0) {
            return {};
        }

        if (srcWidth == dstWidth && srcHeight == dstHeight) {
            return src[dstY * srcWidth + dstX];
        }

        if (dstWidth < srcWidth || dstHeight < srcHeight) {
            return sample_area(src, srcWidth, srcHeight, dstWidth, dstHeight, dstX, dstY);
        }

        return sample_bilinear(src, srcWidth, srcHeight, dstWidth, dstHeight, dstX, dstY);
    }

    static bool resize(const RGBA* src, int srcWidth, int srcHeight,
                       RGBA* dst, int dstWidth, int dstHeight, int dstPitch) {
        if (!src || !dst || srcWidth <= 0 || srcHeight <= 0 ||
            dstWidth <= 0 || dstHeight <= 0 || dstPitch < dstWidth) {
            return false;
        }

        for (int y = 0; y < dstHeight; ++y) {
            for (int x = 0; x < dstWidth; ++x) {
                dst[y * dstPitch + x] = sample(src, srcWidth, srcHeight, dstWidth, dstHeight, x, y);
            }
        }
        return true;
    }

    static uint8_t alpha_from_coverage(uint8_t coverage, uint8_t opacity = 255) {
        return static_cast<uint8_t>((static_cast<uint32_t>(coverage) * opacity + 127U) / 255U);
    }

private:
    static double clamp(double value, double minValue, double maxValue) {
        if (value < minValue) {
            return minValue;
        }
        return value > maxValue ? maxValue : value;
    }

    static int floor_to_int(double value) {
        int result = static_cast<int>(value);
        if (static_cast<double>(result) > value) {
            --result;
        }
        return result;
    }

    static double min_double(double a, double b) {
        return a < b ? a : b;
    }

    static double max_double(double a, double b) {
        return a > b ? a : b;
    }

    static RGBA compose(double red, double green, double blue, double alpha) {
        RGBA out = {};
        if (alpha <= 0.0) {
            return out;
        }

        const double invAlpha = 255.0 / alpha;
        out.r = to_u8(red * invAlpha);
        out.g = to_u8(green * invAlpha);
        out.b = to_u8(blue * invAlpha);
        out.a = to_u8(alpha);
        return out;
    }

    static uint8_t to_u8(double value) {
        if (value <= 0.0) {
            return 0;
        }
        if (value >= 255.0) {
            return 255;
        }
        return static_cast<uint8_t>(value + 0.5);
    }

    static void accumulate(RGBA pixel, double weight,
                           double* red, double* green, double* blue, double* alpha) {
        const double weightedAlpha = (static_cast<double>(pixel.a) / 255.0) * weight;
        *alpha += weightedAlpha * 255.0;
        *red += static_cast<double>(pixel.r) * weightedAlpha;
        *green += static_cast<double>(pixel.g) * weightedAlpha;
        *blue += static_cast<double>(pixel.b) * weightedAlpha;
    }

    static RGBA sample_bilinear(const RGBA* src, int srcWidth, int srcHeight,
                                int dstWidth, int dstHeight, int dstX, int dstY) {
        const double scaleX = static_cast<double>(srcWidth) / static_cast<double>(dstWidth);
        const double scaleY = static_cast<double>(srcHeight) / static_cast<double>(dstHeight);
        const double srcX = clamp((static_cast<double>(dstX) + 0.5) * scaleX - 0.5, 0.0, static_cast<double>(srcWidth - 1));
        const double srcY = clamp((static_cast<double>(dstY) + 0.5) * scaleY - 0.5, 0.0, static_cast<double>(srcHeight - 1));

        const int x0 = floor_to_int(srcX);
        const int y0 = floor_to_int(srcY);
        const int x1 = x0 + 1 < srcWidth ? x0 + 1 : x0;
        const int y1 = y0 + 1 < srcHeight ? y0 + 1 : y0;
        const double fx = srcX - static_cast<double>(x0);
        const double fy = srcY - static_cast<double>(y0);

        double red = 0.0;
        double green = 0.0;
        double blue = 0.0;
        double alpha = 0.0;
        accumulate(src[y0 * srcWidth + x0], (1.0 - fx) * (1.0 - fy), &red, &green, &blue, &alpha);
        accumulate(src[y0 * srcWidth + x1], fx * (1.0 - fy), &red, &green, &blue, &alpha);
        accumulate(src[y1 * srcWidth + x0], (1.0 - fx) * fy, &red, &green, &blue, &alpha);
        accumulate(src[y1 * srcWidth + x1], fx * fy, &red, &green, &blue, &alpha);
        return compose(red, green, blue, alpha);
    }

    static RGBA sample_area(const RGBA* src, int srcWidth, int srcHeight,
                            int dstWidth, int dstHeight, int dstX, int dstY) {
        const double scaleX = static_cast<double>(srcWidth) / static_cast<double>(dstWidth);
        const double scaleY = static_cast<double>(srcHeight) / static_cast<double>(dstHeight);
        const double srcLeft = static_cast<double>(dstX) * scaleX;
        const double srcTop = static_cast<double>(dstY) * scaleY;
        const double srcRight = srcLeft + scaleX;
        const double srcBottom = srcTop + scaleY;

        const int startX = floor_to_int(srcLeft);
        const int startY = floor_to_int(srcTop);
        const int endX = floor_to_int(srcRight - 0.0000001);
        const int endY = floor_to_int(srcBottom - 0.0000001);

        double red = 0.0;
        double green = 0.0;
        double blue = 0.0;
        double alpha = 0.0;
        double totalWeight = 0.0;

        for (int y = startY; y <= endY; ++y) {
            if (y < 0 || y >= srcHeight) {
                continue;
            }
            const double overlapY = min_double(srcBottom, static_cast<double>(y + 1)) -
                                    max_double(srcTop, static_cast<double>(y));
            if (overlapY <= 0.0) {
                continue;
            }

            for (int x = startX; x <= endX; ++x) {
                if (x < 0 || x >= srcWidth) {
                    continue;
                }
                const double overlapX = min_double(srcRight, static_cast<double>(x + 1)) -
                                        max_double(srcLeft, static_cast<double>(x));
                if (overlapX <= 0.0) {
                    continue;
                }

                const double weight = overlapX * overlapY;
                totalWeight += weight;
                accumulate(src[y * srcWidth + x], weight, &red, &green, &blue, &alpha);
            }
        }

        if (totalWeight <= 0.0) {
            return {};
        }

        return compose(red / totalWeight, green / totalWeight, blue / totalWeight, alpha / totalWeight);
    }
};

}
