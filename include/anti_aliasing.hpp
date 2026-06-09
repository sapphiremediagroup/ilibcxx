#pragma once

#include <stdint.h>
#include <emmintrin.h>

namespace std {

class PackedAntiAliasing {
public:
    static constexpr int SampleCount = 8;
    static constexpr int SampleTotal = SampleCount * SampleCount;

    static uint8_t rounded_rect_coverage(int rectX, int rectY, int width, int height, int radius,
                                         int pixelX, int pixelY) {
        return coverage(rectX, rectY, width, height, radius, pixelX, pixelY, false);
    }

    static uint8_t top_rounded_rect_coverage(int rectX, int rectY, int width, int height, int radius,
                                             int pixelX, int pixelY) {
        return coverage(rectX, rectY, width, height, radius, pixelX, pixelY, true);
    }

private:
    static int clamp_radius(int width, int height, int radius) {
        if (radius <= 0) {
            return 0;
        }

        const int maxRadius = (width < height ? width : height) / 2;
        return radius > maxRadius ? maxRadius : radius;
    }

    static uint8_t coverage(int rectX, int rectY, int width, int height, int radius,
                            int pixelX, int pixelY, bool topOnly) {
        if (width <= 0 || height <= 0) {
            return 0;
        }

        radius = clamp_radius(width, height, radius);
        if (radius <= 0) {
            return pixel_fully_inside(rectX, rectY, width, height, pixelX, pixelY) ? 255 : 0;
        }

        const __m128 sampleOffset = _mm_set_ps(0.4375f, 0.3125f, 0.1875f, 0.0625f);
        const __m128 rectLeft = _mm_set1_ps(static_cast<float>(rectX));
        const __m128 rectRight = _mm_set1_ps(static_cast<float>(rectX + width));
        const __m128 leftCenter = _mm_set1_ps(static_cast<float>(rectX + radius));
        const __m128 rightCenter = _mm_set1_ps(static_cast<float>(rectX + width - radius - 1));
        const __m128 radiusSquared = _mm_set1_ps(static_cast<float>(radius * radius));

        const float topCenterY = static_cast<float>(rectY + radius);
        const float bottomCenterY = static_cast<float>(rectY + height - radius - 1);
        const float rectTop = static_cast<float>(rectY);
        const float rectBottom = static_cast<float>(rectY + height);

        int inside = 0;
        for (int sy = 0; sy < SampleCount; ++sy) {
            const float sampleY = static_cast<float>(pixelY) + ((static_cast<float>(sy) + 0.5f) / SampleCount);
            if (sampleY < rectTop || sampleY >= rectBottom) {
                continue;
            }

            for (int sx = 0; sx < SampleCount; sx += 4) {
                const __m128 sampleXBase = _mm_set1_ps(static_cast<float>(pixelX) + (static_cast<float>(sx) / SampleCount));
                const __m128 sampleX = _mm_add_ps(sampleXBase, sampleOffset);

                const __m128 inHorizontalBounds = _mm_and_ps(
                    _mm_cmpge_ps(sampleX, rectLeft),
                    _mm_cmplt_ps(sampleX, rectRight)
                );

                const int boundMask = _mm_movemask_ps(inHorizontalBounds);
                if (boundMask == 0) {
                    continue;
                }

                int insideMask = 0;
                if (topOnly) {
                    if (sampleY >= topCenterY) {
                        insideMask = 0x0F;
                    }
                } else if (sampleY >= topCenterY && sampleY <= bottomCenterY) {
                    insideMask = 0x0F;
                }

                const __m128 centerBand = _mm_and_ps(
                    _mm_cmpge_ps(sampleX, leftCenter),
                    _mm_cmple_ps(sampleX, rightCenter)
                );
                insideMask |= _mm_movemask_ps(centerBand);

                if ((insideMask & boundMask) != boundMask) {
                    const __m128 leftDistance = _mm_sub_ps(sampleX, leftCenter);
                    const __m128 rightDistance = _mm_sub_ps(sampleX, rightCenter);
                    const __m128 useLeft = _mm_cmplt_ps(sampleX, leftCenter);
                    const __m128 dx = select_ps(useLeft, leftDistance, rightDistance);

                    const float centerY = topOnly
                        ? topCenterY
                        : (sampleY < topCenterY ? topCenterY : bottomCenterY);
                    const __m128 dy = _mm_set1_ps(sampleY - centerY);
                    const __m128 distanceSquared = _mm_add_ps(_mm_mul_ps(dx, dx), _mm_mul_ps(dy, dy));
                    insideMask |= _mm_movemask_ps(_mm_cmple_ps(distanceSquared, radiusSquared));
                }

                inside += popcount4(insideMask & boundMask);
            }
        }

        return static_cast<uint8_t>((inside * 255 + (SampleTotal / 2)) / SampleTotal);
    }

    static bool pixel_fully_inside(int rectX, int rectY, int width, int height, int pixelX, int pixelY) {
        return pixelX >= rectX && pixelY >= rectY && pixelX + 1 <= rectX + width && pixelY + 1 <= rectY + height;
    }

    static __m128 select_ps(__m128 mask, __m128 onTrue, __m128 onFalse) {
        return _mm_or_ps(_mm_and_ps(mask, onTrue), _mm_andnot_ps(mask, onFalse));
    }

    static int popcount4(int mask) {
        mask &= 0x0F;
        mask = (mask & 0x05) + ((mask >> 1) & 0x05);
        return (mask & 0x03) + ((mask >> 2) & 0x03);
    }
};

}
