#include <windows.h>

#include "PIDefines.h"
#include "PIFilter.h"
#include "PITypes.h"

#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>

#include "core/blend.h"
#include "core/settings.h"
#include "plugin/host.h"

namespace wm {

namespace {

bool UsesBig(const FilterRecord* fr) {
    return fr->bigDocumentData != nullptr && fr->bigDocumentData->PluginUsing32BitCoordinates;
}

void SetRects(FilterRecord* fr, int left, int top, int right, int bottom) {
    if (UsesBig(fr)) {
        VRect r;
        r.left = left; r.top = top; r.right = right; r.bottom = bottom;
        fr->bigDocumentData->inRect32 = r;
        fr->bigDocumentData->outRect32 = r;
    } else {
        Rect r;
        r.left = (int16)left; r.top = (int16)top; r.right = (int16)right; r.bottom = (int16)bottom;
        fr->inRect = r;
        fr->outRect = r;
    }
}

void ZeroRects(FilterRecord* fr) {
    SetRects(fr, 0, 0, 0, 0);
    if (UsesBig(fr)) {
        VRect z; z.left = z.top = z.right = z.bottom = 0;
        fr->bigDocumentData->maskRect32 = z;
    } else {
        Rect z; z.left = z.top = z.right = z.bottom = 0;
        fr->maskRect = z;
    }
}

}  // namespace

void DebugLog(const wchar_t* fmt, ...) {
    std::wstring dir = DataDir();
    if (dir.empty()) return;
    // Dziennik wlacza sie, tworzac pusty plik debug.flag w katalogu danych.
    if (GetFileAttributesW((dir + L"\\debug.flag").c_str()) == INVALID_FILE_ATTRIBUTES) return;
    wchar_t buf[1024];
    va_list ap;
    va_start(ap, fmt);
    _vsnwprintf_s(buf, _TRUNCATE, fmt, ap);
    va_end(ap);
    HANDLE h = CreateFileW((dir + L"\\log.txt").c_str(), FILE_APPEND_DATA, FILE_SHARE_READ, nullptr, OPEN_ALWAYS,
                           FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return;
    std::string u = WideToUtf8(buf) + "\r\n";
    DWORD w;
    WriteFile(h, u.data(), (DWORD)u.size(), &w, nullptr);
    CloseHandle(h);
}

HostInfo QueryHost(FilterRecord* fr) {
    HostInfo hi;
    if (UsesBig(fr)) {
        hi.imageW = fr->bigDocumentData->imageSize32.h;
        hi.imageH = fr->bigDocumentData->imageSize32.v;
        const VRect& r = fr->bigDocumentData->filterRect32;
        hi.filterRect = {r.left, r.top, r.right, r.bottom};
    } else {
        hi.imageW = fr->imageSize.h;
        hi.imageH = fr->imageSize.v;
        hi.filterRect = {fr->filterRect.left, fr->filterRect.top, fr->filterRect.right, fr->filterRect.bottom};
    }
    hi.hasSelection = hi.filterRect.left > 0 || hi.filterRect.top > 0 || hi.filterRect.right < hi.imageW ||
                      hi.filterRect.bottom < hi.imageH || fr->haveMask;
    hi.depth = fr->depth > 0 ? fr->depth : 8;
    hi.planes = fr->planes;
    int mode = fr->imageMode;
    hi.rgbOk = (mode == plugInModeRGBColor || mode == plugInModeRGB48) && (hi.depth == 8 || hi.depth == 16);

    const int fc = fr->filterCase;
    const bool editableTrans = fc == filterCaseEditableTransparencyNoSelection ||
                               fc == filterCaseEditableTransparencyWithSelection;
    hi.lockedTransparency = fc == filterCaseProtectedTransparencyNoSelection ||
                            fc == filterCaseProtectedTransparencyWithSelection;
    // Host moze podac przezroczystosc, nie ustawiajac "filterCase" dla warstwy z edytowalna przezroczystoscia
    // (inne programy niz Photoshop): wtedy ufamy licznikom plaszczyzn.
    const bool hostHasTrans = fr->inTransparencyMask > 0 && fr->inLayerPlanes > 0 && !hi.lockedTransparency;
    if (editableTrans || hostHasTrans) {
        if (fr->inTransparencyMask > 0 && fr->inLayerPlanes > 0) hi.transPlane = fr->inLayerPlanes;
        else if (hi.planes >= 4) hi.transPlane = 3;
        if (hi.transPlane >= hi.planes) hi.transPlane = -1;
    }
    hi.flatLayer = hi.transPlane < 0;

    hi.foreColor = ((uint32_t)fr->foreColor[0] << 16) | ((uint32_t)fr->foreColor[1] << 8) | fr->foreColor[2];
    if (fr->platformData) hi.hwnd = (HWND)((PlatformData*)fr->platformData)->hwnd;
    if (!hi.hwnd || !IsWindow(hi.hwnd)) {
        // Niektore programy nie podaja okna-rodzica: bierzemy aktywne okno tego samego procesu.
        HWND fg = GetForegroundWindow();
        DWORD pid = 0;
        if (fg) GetWindowThreadProcessId(fg, &pid);
        hi.hwnd = (fg && pid == GetCurrentProcessId()) ? fg : nullptr;
    }

    if (fr->documentInfo && fr->documentInfo->targetCompositeChannels) {
        hi.layerOffsetX = fr->documentInfo->targetCompositeChannels->bounds.left;
        hi.layerOffsetY = fr->documentInfo->targetCompositeChannels->bounds.top;
    }
    DebugLog(L"QueryHost: hostSig %08X ver %d", (unsigned)fr->hostSig, (int)fr->serialNumber);
    DebugLog(L"QueryHost: image %dx%d filterRect %d,%d,%d,%d depth %d planes %d filterCase %d transPlane %d "
             L"inLayerPlanes %d inTrans %d outLayerPlanes %d outTrans %d mode %d fore %06X",
             hi.imageW, hi.imageH, hi.filterRect.left, hi.filterRect.top, hi.filterRect.right, hi.filterRect.bottom,
             hi.depth, hi.planes, fc, hi.transPlane, fr->inLayerPlanes, fr->inTransparencyMask, fr->outLayerPlanes,
             fr->outTransparencyMask, mode, hi.foreColor);
    return hi;
}

// ---------------------------------------------------------------------------------------------
// Podglad

namespace {

bool ReadMerged(FilterRecord* fr, const HostInfo& hi, int pw, int ph, PreviewBase& out) {
    ReadImageDocumentDesc* doc = fr->documentInfo;
    if (!doc || !fr->channelPortProcs || !fr->channelPortProcs->readPixelsProc) return false;
    ReadChannelDesc* ch[3] = {nullptr, nullptr, nullptr};
    ReadChannelDesc* c = doc->mergedCompositeChannels;
    for (int i = 0; i < 3; ++i) {
        if (!c) return false;
        ch[i] = c;
        c = c->next;
    }
    // Obszar warstwy w dokumencie musi miescic sie w zlozonym obrazie.
    VRect b = ch[0]->bounds;
    int x0 = hi.layerOffsetX, y0 = hi.layerOffsetY;
    if (x0 < b.left || y0 < b.top || x0 + hi.imageW > b.right || y0 + hi.imageH > b.bottom) return false;

    std::vector<uint8_t> planes[3];
    for (int i = 0; i < 3; ++i) {
        planes[i].assign((size_t)pw * ph, 0);
        PSScaling sc;
        sc.sourceRect.left = x0; sc.sourceRect.top = y0;
        sc.sourceRect.right = x0 + hi.imageW; sc.sourceRect.bottom = y0 + hi.imageH;
        sc.destinationRect.left = 0; sc.destinationRect.top = 0;
        sc.destinationRect.right = pw; sc.destinationRect.bottom = ph;
        VRect wr = sc.destinationRect, wrote;
        PixelMemoryDesc d;
        d.data = planes[i].data();
        d.rowBits = pw * 8;
        d.colBits = 8;
        d.bitOffset = 0;
        d.depth = 8;
        OSErr e = fr->channelPortProcs->readPixelsProc(ch[i]->port, &sc, &wr, &d, &wrote);
        if (e != noErr) { DebugLog(L"readPixelsProc[%d] err %d", i, (int)e); return false; }
    }
    out.w = pw; out.h = ph; out.merged = true;
    out.rgba.resize((size_t)pw * ph * 4);
    for (size_t i = 0; i < (size_t)pw * ph; ++i) {
        out.rgba[i * 4 + 0] = planes[0][i] / 255.0f;
        out.rgba[i * 4 + 1] = planes[1][i] / 255.0f;
        out.rgba[i * 4 + 2] = planes[2][i] / 255.0f;
        out.rgba[i * 4 + 3] = 1.0f;
    }
    return true;
}

bool ReadLayerOnly(FilterRecord* fr, const HostInfo& hi, int rate, PreviewBase& out) {
    if (hi.planes < 3) return false;
    if (fr->samplingSupport == 0) return false;
    int pw = (hi.imageW + rate - 1) / rate, ph = (hi.imageH + rate - 1) / rate;
    fr->inputRate = (Fixed)rate << 16;
    fr->maskRate = (Fixed)rate << 16;
    if (UsesBig(fr)) {
        VRect r; r.left = 0; r.top = 0; r.right = pw; r.bottom = ph;
        fr->bigDocumentData->inRect32 = r;
        fr->bigDocumentData->outRect32 = r;
        VRect z; z.left = z.top = z.right = z.bottom = 0;
        fr->bigDocumentData->outRect32 = z;
    } else {
        Rect r; r.left = 0; r.top = 0; r.right = (int16)pw; r.bottom = (int16)ph;
        fr->inRect = r;
        Rect z; z.left = z.top = z.right = z.bottom = 0;
        fr->outRect = z;
    }
    int np = hi.planes;
    int use = hi.transPlane >= 0 ? hi.transPlane + 1 : std::min(np, 3);
    fr->inLoPlane = 0;
    fr->inHiPlane = (int16)(use - 1);
    fr->outLoPlane = fr->outHiPlane = 0;
    OSErr e = fr->advanceState();
    if (e != noErr || !fr->inData) { DebugLog(L"advanceState(preview) err %d", (int)e); return false; }

    const int bps = hi.depth == 16 ? 2 : 1;
    const float maxv = hi.depth == 16 ? 32768.0f : 255.0f;
    out.w = pw; out.h = ph; out.merged = false;
    out.rgba.assign((size_t)pw * ph * 4, 0.0f);
    for (int y = 0; y < ph; ++y) {
        const uint8_t* row = (const uint8_t*)fr->inData + (ptrdiff_t)y * fr->inRowBytes;
        for (int x = 0; x < pw; ++x) {
            const uint8_t* px = row + (size_t)x * use * bps;
            float v[4] = {0, 0, 0, 1};
            for (int c = 0; c < use; ++c) {
                float f = bps == 2 ? ((const uint16_t*)px)[c] / maxv : px[c] / maxv;
                if (c < 3) v[c] = f;
                else if (c == hi.transPlane) v[3] = f;
            }
            float* o = &out.rgba[((size_t)y * pw + x) * 4];
            for (int c = 0; c < 4; ++c) o[c] = std::min(std::max(v[c], 0.0f), 1.0f);
        }
    }
    // Zadanie probkowania 1:1 przywracamy, zeby nie psuc pozniejszego zapisu.
    fr->inputRate = (Fixed)1 << 16;
    fr->maskRate = (Fixed)1 << 16;
    ZeroRects(fr);
    return true;
}

}  // namespace

bool ReadPreviewBase(FilterRecord* fr, const HostInfo& hi, int maxDim, PreviewBase& out) {
    if (hi.imageW <= 0 || hi.imageH <= 0) return false;
    int longest = std::max(hi.imageW, hi.imageH);
    double s = std::min(1.0, (double)maxDim / longest);
    int pw = std::max(1, (int)std::lround(hi.imageW * s)), ph = std::max(1, (int)std::lround(hi.imageH * s));
    bool ok = false;
    __try {
        ok = ReadMerged(fr, hi, pw, ph, out);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        ok = false;
    }
    if (!ok) {
        int rate = std::max(1, (longest + maxDim - 1) / maxDim);
        ok = ReadLayerOnly(fr, hi, rate, out);
    }
    if (ok) {
        out.scaleX = (double)out.w / hi.imageW;
        out.scaleY = (double)out.h / hi.imageH;
    }
    DebugLog(L"ReadPreviewBase: ok=%d merged=%d %dx%d", (int)ok, (int)out.merged, out.w, out.h);
    return ok;
}

// ---------------------------------------------------------------------------------------------
// Zapis podpisu

namespace {

template <typename T>
inline T RoundClamp(float f, float maxv) {
    float v = f * maxv + 0.5f;
    if (v < 0.0f) v = 0.0f;
    if (v > maxv) v = maxv;
    return (T)v;
}

}  // namespace

ApplyStatus ApplySignature(FilterRecord* fr, const HostInfo& hi, const Params& pin, Rasterizer& ras,
                           Status& renderStatus, std::wstring& detail, short& hostErr) {
    Params p = pin;
    Sanitize(p);
    renderStatus = Status::Ok;
    hostErr = 0;
    if (!fr->advanceState) { hostErr = filterBadParameters; return ApplyStatus::HostError; }

    DocContext ctx;
    ctx.docW = hi.imageW;
    ctx.docH = hi.imageH;
    ctx.ref = p.relToSelection && hi.hasSelection ? hi.filterRect : IRect{0, 0, hi.imageW, hi.imageH};
    ctx.scale = 1.0;

    RenderResult rr = RenderSignature(p, ctx, ras);
    if (rr.status != Status::Ok) {
        renderStatus = rr.status;
        detail = rr.detail;
        return ApplyStatus::Cancelled;
    }
    const Rendered& sig = rr.sig;

    // Obwiednia wszystkich kopii podpisu (jedna albo wiele w trybie kafelkowym) przycieta do warstwy.
    int rx0 = hi.imageW, ry0 = hi.imageH, rx1 = 0, ry1 = 0;
    for (const auto& o : sig.origins) {
        rx0 = std::min(rx0, std::max(o.first, 0));
        ry0 = std::min(ry0, std::max(o.second, 0));
        rx1 = std::max(rx1, std::min(o.first + sig.img.w, hi.imageW));
        ry1 = std::max(ry1, std::min(o.second + sig.img.h, hi.imageH));
    }
    if (rx1 <= rx0 || ry1 <= ry0) {
        renderStatus = Status::NothingToDraw;
        return ApplyStatus::Cancelled;
    }

    // Prefiks plaszczyzn: RGB (+ przezroczystosc). Maski warstw i kanaly alfa nas nie interesuja.
    const int planes = hi.transPlane >= 0 ? hi.transPlane + 1 : std::min(hi.planes, 3);
    const int bps = hi.depth == 16 ? 2 : 1;
    const float maxv = hi.depth == 16 ? 32768.0f : 255.0f;
    const int mode = p.direct ? p.blend : (int)kBlendNormal;

    fr->inputRate = (Fixed)1 << 16;
    fr->maskRate = (Fixed)1 << 16;
    fr->inLoPlane = fr->outLoPlane = 0;
    fr->inHiPlane = fr->outHiPlane = (int16)(planes - 1);

    // Paski: ok. 16 MB na bufor wejsciowy + wyjsciowy.
    const long long rowBytes = (long long)(rx1 - rx0) * planes * bps;
    int stripRows = (int)std::max<long long>(1, (16LL << 20) / std::max<long long>(rowBytes, 1));
    int totalRows = ry1 - ry0, doneRows = 0;
    const size_t pixStride = (size_t)planes * bps;

    for (int ys = ry0; ys < ry1; ys += stripRows) {
        int ye = std::min(ys + stripRows, ry1);
        SetRects(fr, rx0, ys, rx1, ye);
        OSErr e = fr->advanceState();
        if (e != noErr) { hostErr = (short)e; ZeroRects(fr); return ApplyStatus::HostError; }
        if (!fr->inData || !fr->outData) { hostErr = filterBadParameters; ZeroRects(fr); return ApplyStatus::HostError; }

        // Najpierw przepisujemy wejscie na wyjscie (reszta kanalow bez zmian), potem skladamy kolejne kopie.
        for (int y = ys; y < ye; ++y) {
            const uint8_t* inRow = (const uint8_t*)fr->inData + (ptrdiff_t)(y - ys) * fr->inRowBytes;
            uint8_t* outRow = (uint8_t*)fr->outData + (ptrdiff_t)(y - ys) * fr->outRowBytes;
            std::memcpy(outRow, inRow, (size_t)(rx1 - rx0) * pixStride);
        }
        for (const auto& o : sig.origins) {
            const int tx0 = std::max(o.first, rx0), tx1 = std::min(o.first + sig.img.w, rx1);
            const int ty0 = std::max(o.second, ys), ty1 = std::min(o.second + sig.img.h, ye);
            if (tx1 <= tx0 || ty1 <= ty0) continue;
            for (int y = ty0; y < ty1; ++y) {
                uint8_t* outRow = (uint8_t*)fr->outData + (ptrdiff_t)(y - ys) * fr->outRowBytes;
                for (int x = tx0; x < tx1; ++x) {
                    const float* s = sig.img.px(x - o.first, y - o.second);
                    float sa = s[3];
                    if (sa <= 0.0f) continue;
                    uint8_t* op = outRow + (size_t)(x - rx0) * pixStride;  // wejscie = biezacy stan wyjscia
                    float cb[3], ab = 1.0f;
                    if (bps == 1) {
                        for (int c = 0; c < 3; ++c) cb[c] = op[c] / maxv;
                        if (hi.transPlane >= 0) ab = op[hi.transPlane] / maxv;
                    } else {
                        const uint16_t* i16 = (const uint16_t*)op;
                        for (int c = 0; c < 3; ++c) cb[c] = std::min(i16[c] / maxv, 1.0f);
                        if (hi.transPlane >= 0) ab = std::min(i16[hi.transPlane] / maxv, 1.0f);
                    }
                    float cs[3] = {std::min(s[0] / sa, 1.0f), std::min(s[1] / sa, 1.0f), std::min(s[2] / sa, 1.0f)};
                    float oc[3], oa;
                    CompositePixel(mode, cb, ab, cs, sa, oc, oa);
                    if (bps == 1) {
                        for (int c = 0; c < 3; ++c) op[c] = RoundClamp<uint8_t>(oc[c], maxv);
                        if (hi.transPlane >= 0) op[hi.transPlane] = RoundClamp<uint8_t>(oa, maxv);
                    } else {
                        uint16_t* o16 = (uint16_t*)op;
                        for (int c = 0; c < 3; ++c) o16[c] = RoundClamp<uint16_t>(oc[c], maxv);
                        if (hi.transPlane >= 0) o16[hi.transPlane] = RoundClamp<uint16_t>(oa, maxv);
                    }
                }
            }
        }
        doneRows += ye - ys;
        if (fr->progressProc) fr->progressProc(doneRows, totalRows);
        if (fr->abortProc && fr->abortProc()) { ZeroRects(fr); return ApplyStatus::Cancelled; }
    }
    ZeroRects(fr);
    return ApplyStatus::Ok;
}

}  // namespace wm
