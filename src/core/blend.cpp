#include "blend.h"

#include <algorithm>
#include <cmath>

namespace wm {

namespace {

float HardLight(float cb, float cs) {
    return cs <= 0.5f ? cb * (2.0f * cs) : (cb + (2.0f * cs - 1.0f) - cb * (2.0f * cs - 1.0f));  // screen(cb, 2cs-1)
}

float SoftLight(float cb, float cs) {
    if (cs <= 0.5f) return cb - (1.0f - 2.0f * cs) * cb * (1.0f - cb);
    float d = cb <= 0.25f ? ((16.0f * cb - 12.0f) * cb + 4.0f) * cb : std::sqrt(cb);
    return cb + (2.0f * cs - 1.0f) * (d - cb);
}

}  // namespace

float BlendChannel(int mode, float cb, float cs) {
    switch (mode) {
        case kBlendMultiply: return cb * cs;
        case kBlendScreen: return cb + cs - cb * cs;
        case kBlendOverlay: return HardLight(cs, cb);  // overlay = hard light z zamienionymi warstwami
        case kBlendSoftLight: return SoftLight(cb, cs);
        case kBlendHardLight: return HardLight(cb, cs);
        case kBlendLighten: return std::max(cb, cs);
        case kBlendDarken: return std::min(cb, cs);
        case kBlendDifference: return std::fabs(cb - cs);
        default: return cs;
    }
}

void CompositePixel(int mode, const float cb[3], float ab, const float cs[3], float as,
                    float outColor[3], float& outAlpha) {
    float ao = as + ab * (1.0f - as);
    outAlpha = ao;
    if (ao <= 0.0f) {
        outColor[0] = outColor[1] = outColor[2] = 0.0f;
        return;
    }
    for (int i = 0; i < 3; ++i) {
        float b = mode == kBlendNormal ? cs[i] : BlendChannel(mode, cb[i], cs[i]);
        float co = cs[i] * as * (1.0f - ab) + cb[i] * ab * (1.0f - as) + as * ab * b;
        outColor[i] = std::min(std::max(co / ao, 0.0f), 1.0f);
    }
}

}  // namespace wm
