#include "effects.h"

#include <algorithm>
#include <cmath>

namespace wm {

namespace {

const double kPi = 3.14159265358979323846;

float Clamp01(float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }

// Zrodlo `top` (premult) nakladane na `dst` (premult), mnoznik krycia `k`.
inline void OverPremult(float* dst, const float* srcRGB, float a) {
    float inv = 1.0f - a;
    dst[0] = srcRGB[0] * a + dst[0] * inv;
    dst[1] = srcRGB[1] * a + dst[1] * inv;
    dst[2] = srcRGB[2] * a + dst[2] * inv;
    dst[3] = a + dst[3] * inv;
}

}  // namespace

int EffectMargin(const StrokeSpec* stroke, const EmbossSpec* emboss) {
    float m = 0;
    if (stroke) m = std::max(m, stroke->width);
    if (emboss) m = std::max(m, emboss->size + emboss->soften * 3.0f);
    return (int)std::ceil(m) + 2;
}

static void ApplyEmboss(FImage& img, const Mask& alpha, const Mask& sd, const EmbossSpec& e) {
    const int w = img.w, h = img.h;
    const float r = std::max(e.size, 0.5f);
    const float dir = e.down ? -1.0f : 1.0f;

    Mask H(w, h);
    Mask region(w, h);  // gdzie nakladamy swiatlo/cien (0..1)
    for (size_t i = 0; i < H.d.size(); ++i) {
        float s = sd.d[i];
        float a = alpha.d[i];
        float height = 0, reg = 0;
        switch (e.style) {
            case kEmbossInnerBevel:
                height = r * Clamp01(-s / r);
                reg = a;
                break;
            case kEmbossOuterBevel:
                height = r * Clamp01(1.0f - s / r);
                reg = Clamp01(1.0f - a) * Clamp01(r + 0.5f - s);
                break;
            case kEmbossPillow:
                height = -r * Clamp01(1.0f - std::fabs(s) / r);
                reg = Clamp01(r + 0.5f - std::fabs(s));
                break;
            case kEmbossEmboss:
            default:
                height = r * Clamp01(0.5f - s / r);
                reg = Clamp01(r * 0.5f + 0.5f - std::fabs(s));
                break;
        }
        H.d[i] = dir * height * e.depth;
        region.d[i] = reg;
    }
    GaussianBlur(H, e.soften);

    const double ang = e.angleDeg * kPi / 180.0, alt = e.altDeg * kPi / 180.0;
    const float lx = (float)(std::cos(ang) * std::cos(alt));
    const float ly = (float)(-std::sin(ang) * std::cos(alt));
    const float lz = (float)std::sin(alt);

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            float reg = region.at(x, y);
            if (reg <= 0.0f) continue;
            int x0 = std::max(x - 1, 0), x1 = std::min(x + 1, w - 1);
            int y0 = std::max(y - 1, 0), y1 = std::min(y + 1, h - 1);
            float gx = (H.at(x1, y) - H.at(x0, y)) / (float)(x1 - x0);
            float gy = (H.at(x, y1) - H.at(x, y0)) / (float)(y1 - y0);
            float inv = 1.0f / std::sqrt(gx * gx + gy * gy + 1.0f);
            float shade = (-gx * lx - gy * ly + lz) * inv - lz;  // plasko => 0
            shade *= 2.0f;
            float* p = img.px(x, y);
            if (shade < 0.0f) {
                float a = Clamp01(-shade) * e.shOpacity * reg;
                if (a > 0.0f) OverPremult(p, e.shColor, a);
            } else if (shade > 0.0f) {
                float a = Clamp01(shade) * e.hiOpacity * reg;
                if (a > 0.0f) OverPremult(p, e.hiColor, a);
            }
        }
    }
}

static void ApplyStroke(FImage& img, const Mask& sd, const StrokeSpec& s) {
    if (s.width <= 0.0f || s.opacity <= 0.0f) return;
    float lo, hi;
    switch (s.position) {
        case kStrokeInside: lo = -s.width; hi = 0.0f; break;
        case kStrokeCenter: lo = -s.width * 0.5f; hi = s.width * 0.5f; break;
        case kStrokeOutside:
        default: lo = 0.0f; hi = s.width; break;
    }
    for (int y = 0; y < img.h; ++y) {
        for (int x = 0; x < img.w; ++x) {
            float d = sd.at(x, y);
            float cov = Clamp01(std::min(hi - d, d - lo) + 0.5f);
            if (cov <= 0.0f) continue;
            OverPremult(img.px(x, y), s.color, cov * s.opacity);
        }
    }
}

void ApplyEffects(FImage& img, const StrokeSpec* stroke, const EmbossSpec* emboss) {
    if (img.empty() || (!stroke && !emboss)) return;
    Mask alpha = AlphaOf(img);  // ksztalt wejsciowy, przed jakimkolwiek efektem
    Mask sd = SignedDistance(alpha);
    if (emboss) ApplyEmboss(img, alpha, sd, *emboss);
    if (stroke) ApplyStroke(img, sd, *stroke);
}

}  // namespace wm
