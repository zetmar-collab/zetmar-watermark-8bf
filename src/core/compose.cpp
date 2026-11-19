#include "compose.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <new>

#include "effects.h"

namespace wm {

namespace {

void UnpackColor(uint32_t c, float out[3]) {
    out[0] = ((c >> 16) & 0xFF) / 255.0f;
    out[1] = ((c >> 8) & 0xFF) / 255.0f;
    out[2] = (c & 0xFF) / 255.0f;
}

bool IsBlank(const std::wstring& s) {
    for (wchar_t c : s)
        if (c != L' ' && c != L'\t' && c != L'\r' && c != L'\n' && c != 0x00A0) return false;
    return true;
}

void Colorize(FImage& img, const float rgb[3]) {
    for (size_t i = 0; i < img.d.size(); i += 4) {
        float a = img.d[i + 3];
        img.d[i] = rgb[0] * a;
        img.d[i + 1] = rgb[1] * a;
        img.d[i + 2] = rgb[2] * a;
    }
}

// Obraz jako maska: z alfa, jesli obraz ma przezroczystosc; inaczej "tusz" = 1 - luminancja.
void RecolorImage(FImage& img, const float rgb[3]) {
    bool hasAlpha = false;
    for (size_t i = 3; i < img.d.size(); i += 4)
        if (img.d[i] < 0.999f) { hasAlpha = true; break; }
    for (size_t i = 0; i < img.d.size(); i += 4) {
        float m;
        if (hasAlpha) {
            m = img.d[i + 3];
        } else {
            float lum = 0.2126f * img.d[i] + 0.7152f * img.d[i + 1] + 0.0722f * img.d[i + 2];
            m = 1.0f - lum;
        }
        img.d[i] = rgb[0] * m;
        img.d[i + 1] = rgb[1] * m;
        img.d[i + 2] = rgb[2] * m;
        img.d[i + 3] = m;
    }
}

struct ScaledCache {
    std::shared_ptr<const FImage> srcHold;  // trzyma zrodlo przy zyciu, zeby wskaznik nie zostal ponownie uzyty
    long long nw = 0, nh = 0;
    bool recolor = false;
    uint32_t color = 0;
    std::shared_ptr<FImage> result;
    bool Match(const FImage* s, long long w, long long h, bool rc, uint32_t col) const {
        return result && srcHold.get() == s && nw == w && nh == h && recolor == rc && (!rc || color == col);
    }
    void Store(const std::shared_ptr<const FImage>& s, long long w, long long h, bool rc, uint32_t col,
               const FImage& img) {
        srcHold = s; nw = w; nh = h; recolor = rc; color = col;
        result = std::make_shared<FImage>(img);
    }
};

}  // namespace

void PlaceBox(const Params& p, const DocContext& ctx, int bw, int bh, int& x, int& y) {
    const IRect& r = ctx.ref;
    int rw = r.width(), rh = r.height();
    double mx = p.marginX, my = p.marginY;
    if (p.marginUnit == kUnitPercentWidth) {
        double shorter = (double)std::min(rw, rh);
        mx = mx / 100.0 * shorter;
        my = my / 100.0 * shorter;
    } else {
        mx *= ctx.scale;
        my *= ctx.scale;
    }
    int col = p.anchor % 3, row = p.anchor / 3;
    double fx, fy;
    if (col == 0) fx = r.left + mx;
    else if (col == 1) fx = r.left + (rw - bw) / 2.0;
    else fx = r.right - mx - bw;
    if (row == 0) fy = r.top + my;
    else if (row == 1) fy = r.top + (rh - bh) / 2.0;
    else fy = r.bottom - my - bh;
    fx += p.offsetX * ctx.scale;
    fy += p.offsetY * ctx.scale;
    x = (int)std::lround(fx);
    y = (int)std::lround(fy);
}

static RenderResult RenderSignatureImpl(const Params& pin, const DocContext& ctx, Rasterizer& ras) {
    RenderResult res;
    Params p = pin;
    Sanitize(p);
    float color[3];
    UnpackColor(p.color, color);

    FImage base;
    std::wstring err;

    if (p.mode == kModeText) {
        if (IsBlank(p.text)) { res.status = Status::EmptyText; return res; }
        TextRequest req;
        req.text = p.text;
        req.font = p.fontName;
        req.bold = p.bold != 0;
        req.italic = p.italic != 0;
        double sizePx = p.sizeUnit == kUnitPx ? p.sizeValue * ctx.scale : p.sizeValue / 100.0 * ctx.docW;
        sizePx = std::max(sizePx, 1.0);
        if (sizePx > 20000.0) { res.status = Status::TooLarge; return res; }
        req.sizePx = sizePx;
        req.tracking = p.tracking;
        req.lineSpacing = p.lineSpacing;
        req.align = p.textAlign > 0 ? p.textAlign - 1 : p.anchor % 3;
        FImage raw;
        if (!ras.RenderText(req, raw, err)) {
            res.status = err == L"too large" ? Status::TooLarge : Status::RenderFailed;
            res.detail = err;
            return res;
        }
        int cx, cy;
        if (!CropToContent(raw, 0.002f, base, cx, cy)) { res.status = Status::NothingToDraw; return res; }
        Colorize(base, color);
    } else {
        if (p.imagePath[0] == 0) { res.status = Status::ImageNotSet; return res; }
        std::shared_ptr<const FImage> srcPtr = ras.LoadImageShared(p.imagePath, err);
        if (!srcPtr || srcPtr->empty()) {
            res.status = Status::ImageLoadFailed;
            res.detail = err;
            return res;
        }
        const FImage& src = *srcPtr;
        double factor = p.imgScaleUnit == kUnitPx ? p.imgScale / 100.0 * ctx.scale
                                                  : (p.imgScale / 100.0 * ctx.docW) / src.w;
        long long nw = std::max(1LL, std::llround(src.w * factor));
        long long nh = std::max(1LL, std::llround(src.h * factor));
        if (nw * nh > kMaxSignaturePixels) { res.status = Status::TooLarge; return res; }
        // Przekolorowanie i skalowanie to najdrozsze kroki: wynik zostaje w pamieci miedzy odswiezeniami podgladu.
        static ScaledCache cache;
        FImage scaled;
        if (cache.Match(srcPtr.get(), nw, nh, p.recolor != 0, p.color)) {
            scaled = *cache.result;
        } else {
            if (p.recolor) {
                FImage tmp = src;
                RecolorImage(tmp, color);
                scaled = Resize(tmp, (int)nw, (int)nh);
            } else {
                scaled = Resize(src, (int)nw, (int)nh);
            }
            cache.Store(srcPtr, nw, nh, p.recolor != 0, p.color, scaled);
        }
        if (p.flipH) FlipHorizontal(scaled);
        if (p.flipV) FlipVertical(scaled);
        FImage rot = Rotate(scaled, p.rotation);
        int cx, cy;
        if (!CropToContent(rot, 0.01f, base, cx, cy)) { res.status = Status::NothingToDraw; return res; }
    }

    // Wzor kafelkowy: kafelek (z obrotem wzoru) powtarzany na siatce obroconej wokol srodka obszaru.
    const bool tiled = p.tileOn != 0;
    if (tiled && p.tileAngle != 0.0) {
        FImage rot = Rotate(base, p.tileAngle);
        FImage cropped;
        int cx, cy;
        if (!CropToContent(rot, 0.002f, cropped, cx, cy)) { res.status = Status::NothingToDraw; return res; }
        base = std::move(cropped);
    }

    // Efekty
    StrokeSpec stroke;
    EmbossSpec emboss;
    ShadowSpec shadow;
    bool useStroke = p.strokeOn && p.strokeWidth > 0 && p.strokeOpacity > 0;
    bool useEmboss = p.embossOn;
    bool useShadow = p.shadowOn && p.shadowOpacity > 0;
    if (useStroke) {
        UnpackColor(p.strokeColor, stroke.color);
        stroke.width = (float)(p.strokeWidth * ctx.scale);
        stroke.position = p.strokePos;
        stroke.opacity = (float)(p.strokeOpacity / 100.0);
    }
    if (useEmboss) {
        emboss.style = p.embossStyle;
        emboss.down = p.embossDown != 0;
        emboss.depth = (float)(p.depth / 100.0);
        emboss.size = (float)(p.size * ctx.scale);
        emboss.soften = (float)(p.soften * ctx.scale);
        emboss.angleDeg = (float)p.angle;
        emboss.altDeg = (float)p.altitude;
        UnpackColor(p.hiColor, emboss.hiColor);
        UnpackColor(p.shColor, emboss.shColor);
        emboss.hiOpacity = (float)(p.hiOpacity / 100.0);
        emboss.shOpacity = (float)(p.shOpacity / 100.0);
    }
    if (useShadow) {
        UnpackColor(p.shadowColor, shadow.color);
        shadow.opacity = (float)(p.shadowOpacity / 100.0);
        shadow.angleDeg = (float)p.shadowAngle;
        shadow.distance = (float)(p.shadowDistance * ctx.scale);
        shadow.blur = (float)(p.shadowBlur * ctx.scale);
    }
    const StrokeSpec* sp = useStroke ? &stroke : nullptr;
    const EmbossSpec* ep = useEmboss ? &emboss : nullptr;
    const ShadowSpec* hp = useShadow ? &shadow : nullptr;
    int pad = (sp || ep || hp) ? EffectMargin(sp, ep, hp) : 0;

    long long total = (long long)(base.w + 2 * pad) * (base.h + 2 * pad);
    if (total > kMaxSignaturePixels) { res.status = Status::TooLarge; return res; }

    const int bw = base.w, bh = base.h;
    FImage img = Pad(base, pad);
    base = FImage();
    ApplyEffects(img, sp, ep, hp);

    float v = (float)(p.visibility / 100.0);
    if (v < 1.0f) for (auto& f : img.d) f *= v;

    if (!tiled) {
        int px, py;
        PlaceBox(p, ctx, bw, bh, px, py);
        res.sig.origins.push_back({px - pad, py - pad});
    } else {
        const IRect& r = ctx.ref;
        const double sx = bw + p.tileSpacingX * ctx.scale, sy = bh + p.tileSpacingY * ctx.scale;
        if (sx < 4.0 || sy < 4.0) { res.status = Status::TooLarge; return res; }
        const double ang = p.tileAngle * 3.14159265358979323846 / 180.0;
        const double ca = std::cos(ang), sa = std::sin(ang);
        const double cxr = (r.left + r.right) / 2.0, cyr = (r.top + r.bottom) / 2.0;
        const double half = std::hypot((double)r.width(), (double)r.height()) / 2.0 + std::max(bw, bh);
        const int ni = (int)std::ceil(half / sx) + 1, nj = (int)std::ceil(half / sy) + 1;
        if ((long long)(2 * ni + 1) * (2 * nj + 1) > 200000) { res.status = Status::TooLarge; return res; }
        for (int j = -nj; j <= nj; ++j) {
            for (int i = -ni; i <= ni; ++i) {
                double u = i * sx + ((j & 1) && p.tileStagger ? sx / 2.0 : 0.0), w = j * sy;
                double x = cxr + u * ca - w * sa, y = cyr + u * sa + w * ca;
                int ox = (int)std::lround(x - bw / 2.0) - pad, oy = (int)std::lround(y - bh / 2.0) - pad;
                if (ox + img.w <= r.left || oy + img.h <= r.top || ox >= r.right || oy >= r.bottom) continue;
                res.sig.origins.push_back({ox, oy});
            }
        }
        if (res.sig.origins.empty()) { res.status = Status::NothingToDraw; return res; }
        if (res.sig.origins.size() > 20000) { res.status = Status::TooLarge; return res; }
    }
    res.sig.x0 = res.sig.origins[0].first;
    res.sig.y0 = res.sig.origins[0].second;
    res.sig.img = std::move(img);
    return res;
}

RenderResult RenderSignature(const Params& p, const DocContext& ctx, Rasterizer& ras) {
    try {
        return RenderSignatureImpl(p, ctx, ras);
    } catch (const std::bad_alloc&) {
        RenderResult r;
        r.status = Status::TooLarge;
        return r;
    }
}

}  // namespace wm
