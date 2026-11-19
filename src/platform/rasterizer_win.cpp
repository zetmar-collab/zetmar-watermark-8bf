#include "rasterizer_win.h"

#include <windows.h>

#include <objidl.h>
#include <wincodec.h>
// GDI+ wymaga min/max z <algorithm>
#include <algorithm>
#include <gdiplus.h>

#include <cmath>
#include <map>
#include <set>

namespace wm {

namespace {

// Liczy uzycia, zeby GdiplusStartup/Shutdown byly sparowane takze przy wielu rasteryzatorach.
struct GdiplusSession {
    ULONG_PTR token = 0;
    bool ok = false;
    GdiplusSession() {
        Gdiplus::GdiplusStartupInput in;
        ok = Gdiplus::GdiplusStartup(&token, &in, nullptr) == Gdiplus::Ok;
    }
    ~GdiplusSession() {
        if (ok) Gdiplus::GdiplusShutdown(token);
    }
};

std::wstring HrMessage(const wchar_t* what, HRESULT hr) {
    wchar_t buf[128];
    swprintf_s(buf, L"%s (0x%08X)", what, (unsigned)hr);
    return buf;
}

int StyleFlags(bool bold, bool italic) {
    int s = Gdiplus::FontStyleRegular;
    if (bold) s |= Gdiplus::FontStyleBold;
    if (italic) s |= Gdiplus::FontStyleItalic;
    return s;
}

// Znajduje istniejaca rodzine: zadana, potem Segoe UI, Arial, wreszcie generyczna sans-serif.
std::unique_ptr<Gdiplus::FontFamily> ResolveFamily(const std::wstring& name) {
    const wchar_t* candidates[] = {name.c_str(), L"Segoe UI", L"Arial", L"Tahoma"};
    for (const wchar_t* c : candidates) {
        if (!c || !*c) continue;
        auto f = std::make_unique<Gdiplus::FontFamily>(c);
        if (f->IsAvailable()) return f;
    }
    return std::unique_ptr<Gdiplus::FontFamily>(Gdiplus::FontFamily::GenericSansSerif()->Clone());
}

std::vector<std::wstring> SplitLines(const std::wstring& t) {
    std::vector<std::wstring> lines;
    std::wstring cur;
    for (size_t i = 0; i < t.size(); ++i) {
        wchar_t c = t[i];
        if (c == L'\r') { if (i + 1 < t.size() && t[i + 1] == L'\n') ++i; lines.push_back(cur); cur.clear(); }
        else if (c == L'\n') { lines.push_back(cur); cur.clear(); }
        else cur += c;
    }
    lines.push_back(cur);
    return lines;
}

}  // namespace

struct WinRasterizer::Impl {
    GdiplusSession gdip;
    // Pamiec podreczna ostatnio wczytanego obrazu.
    std::wstring cachePath;
    FILETIME cacheTime{};
    std::shared_ptr<const FImage> cacheImg;
};

WinRasterizer::WinRasterizer() : impl_(new Impl) {}
WinRasterizer::~WinRasterizer() = default;

bool WinRasterizer::RenderText(const TextRequest& req, FImage& out, std::wstring& err) {
    using namespace Gdiplus;
    if (!impl_->gdip.ok) { err = L"GDI+"; return false; }

    auto family = ResolveFamily(req.font);
    int style = StyleFlags(req.bold, req.italic);
    if (!family->IsStyleAvailable(style)) style = FontStyleRegular;
    if (!family->IsStyleAvailable(style)) style = FontStyleBold;

    const float em = (float)req.sizePx;
    Font font(family.get(), em, style, UnitPixel);
    if (!font.IsAvailable()) { err = L"font"; return false; }

    // Wysokosc linii z metryk czcionki.
    float emHeight = (float)family->GetEmHeight(style);
    float lineSpacingEm = emHeight > 0 ? (float)family->GetLineSpacing(style) / emHeight : 1.2f;
    float lineH = lineSpacingEm * em * (float)(req.lineSpacing / 100.0);

    StringFormat fmt(StringFormat::GenericTypographic());
    fmt.SetFormatFlags(fmt.GetFormatFlags() | StringFormatFlagsMeasureTrailingSpaces | StringFormatFlagsNoWrap);

    // Bitmapa miernicza 1x1.
    Bitmap measureBmp(1, 1, PixelFormat32bppARGB);
    Graphics mg(&measureBmp);
    mg.SetTextRenderingHint(TextRenderingHintAntiAlias);

    std::vector<std::wstring> lines = SplitLines(req.text);
    const float trk = (float)(req.tracking / 1000.0 * em);

    struct Line { std::vector<float> xs; float width = 0; };
    std::vector<Line> info(lines.size());
    float maxW = 0;
    for (size_t li = 0; li < lines.size(); ++li) {
        const std::wstring& s = lines[li];
        Line& L = info[li];
        RectF big(0, 0, 1e6f, 1e6f), box;
        if (trk == 0.0f) {
            if (!s.empty()) {
                mg.MeasureString(s.c_str(), (INT)s.size(), &font, big, &fmt, &box);
                L.width = box.Width;
            }
        } else {
            // Pozycja kazdego znaku z kerningiem prefiksu + dodatkowy odstep.
            for (size_t i = 0; i < s.size(); ++i) {
                float x = 0;
                if (i > 0) {
                    mg.MeasureString(s.c_str(), (INT)i, &font, big, &fmt, &box);
                    x = box.Width;
                }
                const bool lowHalf = i > 0 && IS_LOW_SURROGATE(s[i]) && IS_HIGH_SURROGATE(s[i - 1]);
                L.xs.push_back(lowHalf ? -1.0f : x + trk * (float)i);
            }
            if (!s.empty()) {
                mg.MeasureString(s.c_str(), (INT)s.size(), &font, big, &fmt, &box);
                L.width = box.Width + trk * (float)(s.size() - 1);
            }
        }
        maxW = std::max(maxW, L.width);
    }

    const int pad = (int)std::ceil(em * 0.35f) + 2;
    double wD = std::ceil(maxW) + 2.0 * pad;
    double hD = std::ceil(lineH * lines.size()) + 2.0 * pad;
    if (wD * hD > (double)kMaxSignaturePixels || wD > 60000 || hD > 60000) { err = L"too large"; return false; }
    int cw = (int)wD, ch = (int)hD;

    Bitmap bmp(cw, ch, PixelFormat32bppARGB);
    if (bmp.GetLastStatus() != Ok) { err = L"bitmap"; return false; }
    {
        Graphics g(&bmp);
        g.SetTextRenderingHint(TextRenderingHintAntiAlias);
        g.SetCompositingMode(CompositingModeSourceOver);
        g.Clear(Color(0, 0, 0, 0));
        SolidBrush brush(Color(255, 255, 255, 255));
        for (size_t li = 0; li < lines.size(); ++li) {
            const std::wstring& s = lines[li];
            if (s.empty()) continue;
            const Line& L = info[li];
            float x0 = (float)pad;
            if (req.align == 1) x0 += (maxW - L.width) / 2.0f;
            else if (req.align == 2) x0 += (maxW - L.width);
            float y0 = (float)pad + lineH * li;
            if (trk == 0.0f) {
                g.DrawString(s.c_str(), (INT)s.size(), &font, PointF(x0, y0), &fmt, &brush);
            } else {
                for (size_t i = 0; i < s.size(); ++i) {
                    if (s[i] == L' ' || L.xs[i] < 0) continue;  // xs < 0: druga polowa pary zastepczej
                    // Para zastepcza (np. emoji) musi byc rysowana razem, inaczej rozpada sie na dwa znaki.
                    INT n = (IS_HIGH_SURROGATE(s[i]) && i + 1 < s.size() && IS_LOW_SURROGATE(s[i + 1])) ? 2 : 1;
                    g.DrawString(&s[i], n, &font, PointF(x0 + L.xs[i], y0), &fmt, &brush);
                }
            }
        }
    }

    BitmapData bd;
    Gdiplus::Rect r(0, 0, cw, ch);
    if (bmp.LockBits(&r, ImageLockModeRead, PixelFormat32bppARGB, &bd) != Ok) { err = L"lock"; return false; }
    out = FImage(cw, ch);
    for (int y = 0; y < ch; ++y) {
        const uint8_t* row = (const uint8_t*)bd.Scan0 + (ptrdiff_t)y * bd.Stride;
        float* o = out.px(0, y);
        for (int x = 0; x < cw; ++x) {
            float a = row[x * 4 + 3] / 255.0f;
            o[x * 4 + 0] = o[x * 4 + 1] = o[x * 4 + 2] = a;
            o[x * 4 + 3] = a;
        }
    }
    bmp.UnlockBits(&bd);
    return true;
}

std::shared_ptr<const FImage> WinRasterizer::LoadImageShared(const std::wstring& path, std::wstring& err) {
    WIN32_FILE_ATTRIBUTE_DATA fad{};
    if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &fad)) { err = L"file"; return nullptr; }
    if (impl_->cachePath == path && CompareFileTime(&impl_->cacheTime, &fad.ftLastWriteTime) == 0 &&
        impl_->cacheImg && !impl_->cacheImg->empty()) {
        return impl_->cacheImg;
    }
    FImage out;

    HRESULT hrInit = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    bool uninit = SUCCEEDED(hrInit);  // S_OK / S_FALSE: trzeba sparowac; RPC_E_CHANGED_MODE: nie
    bool ok = false;

    IWICImagingFactory* factory = nullptr;
    IWICBitmapDecoder* decoder = nullptr;
    IWICBitmapFrameDecode* frame = nullptr;
    IWICFormatConverter* conv = nullptr;
    do {
        HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                                      IID_PPV_ARGS(&factory));
        if (FAILED(hr)) { err = HrMessage(L"WIC", hr); break; }
        hr = factory->CreateDecoderFromFilename(path.c_str(), nullptr, GENERIC_READ,
                                                WICDecodeMetadataCacheOnDemand, &decoder);
        if (FAILED(hr)) { err = HrMessage(L"decode", hr); break; }
        hr = decoder->GetFrame(0, &frame);
        if (FAILED(hr)) { err = HrMessage(L"frame", hr); break; }
        UINT w = 0, h = 0;
        frame->GetSize(&w, &h);
        if (w == 0 || h == 0 || (long long)w * h > kMaxSignaturePixels) { err = L"size"; break; }
        hr = factory->CreateFormatConverter(&conv);
        if (FAILED(hr)) { err = HrMessage(L"convert", hr); break; }
        hr = conv->Initialize(frame, GUID_WICPixelFormat32bppPBGRA, WICBitmapDitherTypeNone, nullptr, 0.0,
                              WICBitmapPaletteTypeCustom);
        if (FAILED(hr)) { err = HrMessage(L"convert", hr); break; }
        std::vector<uint8_t> buf((size_t)w * h * 4);
        hr = conv->CopyPixels(nullptr, w * 4, (UINT)buf.size(), buf.data());
        if (FAILED(hr)) { err = HrMessage(L"pixels", hr); break; }
        out = FImage((int)w, (int)h);
        for (size_t i = 0; i < (size_t)w * h; ++i) {
            // PBGRA -> RGBA premult float
            out.d[i * 4 + 0] = buf[i * 4 + 2] / 255.0f;
            out.d[i * 4 + 1] = buf[i * 4 + 1] / 255.0f;
            out.d[i * 4 + 2] = buf[i * 4 + 0] / 255.0f;
            out.d[i * 4 + 3] = buf[i * 4 + 3] / 255.0f;
        }
        ok = true;
    } while (false);

    if (conv) conv->Release();
    if (frame) frame->Release();
    if (decoder) decoder->Release();
    if (factory) factory->Release();
    if (uninit) CoUninitialize();

    if (!ok) return nullptr;
    impl_->cacheImg = std::make_shared<const FImage>(std::move(out));
    impl_->cachePath = path;
    impl_->cacheTime = fad.ftLastWriteTime;
    return impl_->cacheImg;
}

namespace {
int CALLBACK EnumFontProc(const LOGFONTW* lf, const TEXTMETRICW*, DWORD, LPARAM lp) {
    auto* set = (std::set<std::wstring>*)lp;
    if (lf->lfFaceName[0] && lf->lfFaceName[0] != L'@') set->insert(lf->lfFaceName);
    return 1;
}
}  // namespace

std::vector<std::wstring> ListInstalledFonts() {
    std::set<std::wstring> names;
    HDC dc = GetDC(nullptr);
    LOGFONTW lf{};
    lf.lfCharSet = DEFAULT_CHARSET;
    EnumFontFamiliesExW(dc, &lf, EnumFontProc, (LPARAM)&names, 0);
    ReleaseDC(nullptr, dc);
    return std::vector<std::wstring>(names.begin(), names.end());
}

bool FontHasStyle(const std::wstring& family, bool bold, bool italic) {
    static std::map<std::wstring, bool> cache;
    std::wstring key = family + (bold ? L"|b" : L"|") + (italic ? L"i" : L"");
    auto it = cache.find(key);
    if (it != cache.end()) return it->second;
    bool r = false;
    {
        GdiplusSession s;
        if (s.ok) {
            Gdiplus::FontFamily f(family.c_str());
            r = f.IsAvailable() && f.IsStyleAvailable(StyleFlags(bold, italic)) != FALSE;
        }
    }
    cache[key] = r;
    return r;
}

}  // namespace wm
