#include "compose.h"

#include <algorithm>
#include <cmath>

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

RenderResult RenderSignature(const Params& pin, const DocContext& ctx, Rasterizer& ras) {
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
        req.align = p.anchor % 3;
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
        FImage src;
        if (!ras.LoadImageFile(p.imagePath, src, err) || src.empty()) {
            res.status = Status::ImageLoadFailed;
            res.detail = err;
            return res;
        }
        if (p.recolor) RecolorImage(src, color);
        double factor = p.imgScaleUnit == kUnitPx ? p.imgScale / 100.0 * ctx.scale
                                                  : (p.imgScale / 100.0 * ctx.docW) / src.w;
        long long nw = std::max(1LL, std::llround(src.w * factor));
        long long nh = std::max(1LL, std::llround(src.h * factor));
        if (nw * nh > kMaxSignaturePixels) { res.status = Status::TooLarge; return res; }
        FImage scaled = Resize(src, (int)nw, (int)nh);
        src = FImage();
        if (p.flipH) FlipHorizontal(scaled);
        if (p.flipV) FlipVertical(scaled);
        FImage rot = Rotate(scaled, p.rotation);
        int cx, cy;
        if (!CropToContent(rot, 0.01f, base, cx, cy)) { res.status = Status::NothingToDraw; return res; }
    }

    // Efekty
    StrokeSpec stroke;
    EmbossSpec emboss;
    bool useStroke = p.strokeOn && p.strokeWidth > 0 && p.strokeOpacity > 0;
    bool useEmboss = p.embossOn;
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
    int pad = (useStroke || useEmboss) ? EffectMargin(useStroke ? &stroke : nullptr, useEmboss ? &emboss : nullptr) : 0;

    long long total = (long long)(base.w + 2 * pad) * (base.h + 2 * pad);
    if (total > kMaxSignaturePixels) { res.status = Status::TooLarge; return res; }

    int px, py;
    PlaceBox(p, ctx, base.w, base.h, px, py);

    FImage img = Pad(base, pad);
    base = FImage();
    ApplyEffects(img, useStroke ? &stroke : nullptr, useEmboss ? &emboss : nullptr);

    float v = (float)(p.visibility / 100.0);
    if (v < 1.0f) for (auto& f : img.d) f *= v;

    res.sig.x0 = px - pad;
    res.sig.y0 = py - pad;
    res.sig.img = std::move(img);
    return res;
}

}  // namespace wm
