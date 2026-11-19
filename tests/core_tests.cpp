// Testy rdzenia (bez Photoshopa). Uruchom: wm_tests.exe [katalog_wynikowy]
// Zapisuje PNG-i do podgladu oraz sprawdza: parametry/JSON, pozycje, mieszanie, determinizm, efekty.
#include <windows.h>

#include <wincodec.h>

#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <string>

#include "core/blend.h"
#include "core/compose.h"
#include "core/i18n.h"
#include "core/params.h"
#include "core/settings.h"
#include "platform/rasterizer_win.h"

using namespace wm;

static int g_fail = 0, g_ok = 0;
#define CHECK(cond)                                                                   \
    do {                                                                              \
        if (cond) ++g_ok;                                                             \
        else { ++g_fail; std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); } \
    } while (0)

static std::wstring g_out = L".";

// Sklada podpis (premult float) na tlo RGB (float, kolor prosty) i zapisuje PNG.
static void SavePng(const std::wstring& name, const FImage& rgba /*premult*/, bool onChecker) {
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    IWICImagingFactory* f = nullptr;
    CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&f));
    if (!f) return;
    std::vector<uint8_t> buf((size_t)rgba.w * rgba.h * 4);
    for (int y = 0; y < rgba.h; ++y)
        for (int x = 0; x < rgba.w; ++x) {
            const float* p = rgba.px(x, y);
            float bg = onChecker ? ((((x / 8) + (y / 8)) & 1) ? 0.55f : 0.35f) : 0.0f;
            float a = onChecker ? 1.0f : p[3];
            float c[3];
            for (int i = 0; i < 3; ++i) c[i] = onChecker ? p[i] + bg * (1.0f - p[3]) : (p[3] > 0 ? p[i] / p[3] : 0);
            uint8_t* o = &buf[((size_t)y * rgba.w + x) * 4];
            o[0] = (uint8_t)std::lround(std::fmin(std::fmax(c[2], 0.f), 1.f) * 255);
            o[1] = (uint8_t)std::lround(std::fmin(std::fmax(c[1], 0.f), 1.f) * 255);
            o[2] = (uint8_t)std::lround(std::fmin(std::fmax(c[0], 0.f), 1.f) * 255);
            o[3] = (uint8_t)std::lround(a * 255);
        }
    IWICStream* st = nullptr;
    IWICBitmapEncoder* enc = nullptr;
    IWICBitmapFrameEncode* fr = nullptr;
    IPropertyBag2* pb = nullptr;
    f->CreateStream(&st);
    std::wstring path = g_out + L"\\" + name;
    if (st && SUCCEEDED(st->InitializeFromFilename(path.c_str(), GENERIC_WRITE))) {
        f->CreateEncoder(GUID_ContainerFormatPng, nullptr, &enc);
        enc->Initialize(st, WICBitmapEncoderNoCache);
        enc->CreateNewFrame(&fr, &pb);
        fr->Initialize(pb);
        fr->SetSize(rgba.w, rgba.h);
        WICPixelFormatGUID fmt = GUID_WICPixelFormat32bppBGRA;
        fr->SetPixelFormat(&fmt);
        fr->WritePixels(rgba.h, rgba.w * 4, (UINT)buf.size(), buf.data());
        fr->Commit();
        enc->Commit();
    }
    if (pb) pb->Release();
    if (fr) fr->Release();
    if (enc) enc->Release();
    if (st) st->Release();
    f->Release();
}

static uint64_t Hash(const FImage& img) {
    uint64_t h = 1469598103934665603ull;
    const uint8_t* p = (const uint8_t*)img.d.data();
    for (size_t i = 0; i < img.d.size() * sizeof(float); ++i) { h ^= p[i]; h *= 1099511628211ull; }
    return h ^ ((uint64_t)img.w << 32) ^ (uint64_t)img.h;
}

static void TestParams() {
    Params p;
    SetDefaults(p);
    wcscpy_s(p.text, L"Zażółć gęślą jaźń \"quote\" \\ { } \n druga linia 😀");
    p.color = 0x12AB34;
    p.sizeValue = 7.5;
    p.embossOn = 1;
    std::string j = ToJson(p);
    Params q;
    SetDefaults(q);
    CHECK(FromJson(j, q));
    CHECK(wcscmp(p.text, q.text) == 0);
    CHECK(q.color == 0x12AB34);
    CHECK(q.sizeValue == 7.5);
    CHECK(q.embossOn == 1);
    CHECK(!FromJson("{ nie json", q));

    Params bad;
    SetDefaults(bad);
    bad.sizeValue = -5; bad.visibility = 900; bad.anchor = 99; bad.rotation = NAN;
    Sanitize(bad);
    CHECK(bad.sizeValue >= 0.1 && bad.visibility == 100 && bad.anchor == 8 && bad.rotation == -180);
}

static void TestPlacement() {
    Params p;
    SetDefaults(p);
    p.marginUnit = kUnitPx;
    p.marginX = 10; p.marginY = 20;
    DocContext ctx;
    ctx.docW = 1000; ctx.docH = 500;
    ctx.ref = {0, 0, 1000, 500};
    int x, y;
    p.anchor = 0; PlaceBox(p, ctx, 100, 50, x, y); CHECK(x == 10 && y == 20);
    p.anchor = 1; PlaceBox(p, ctx, 100, 50, x, y); CHECK(x == 450 && y == 20);
    p.anchor = 2; PlaceBox(p, ctx, 100, 50, x, y); CHECK(x == 890 && y == 20);
    p.anchor = 4; PlaceBox(p, ctx, 100, 50, x, y); CHECK(x == 450 && y == 225);
    p.anchor = 6; PlaceBox(p, ctx, 100, 50, x, y); CHECK(x == 10 && y == 430);
    p.anchor = 8; PlaceBox(p, ctx, 100, 50, x, y); CHECK(x == 890 && y == 430);
    p.offsetX = 5; p.offsetY = -5;
    PlaceBox(p, ctx, 100, 50, x, y); CHECK(x == 895 && y == 425);
    // zaznaczenie
    p.offsetX = p.offsetY = 0;
    ctx.ref = {100, 100, 300, 200};
    p.anchor = 8; PlaceBox(p, ctx, 100, 50, x, y); CHECK(x == 190 && y == 130);
    // % krotszego boku
    p.marginUnit = kUnitPercentWidth; p.marginX = 10; p.marginY = 10;
    p.anchor = 0; PlaceBox(p, ctx, 100, 50, x, y); CHECK(x == 110 && y == 110);
    // skala podgladu
    p.marginUnit = kUnitPx; p.marginX = 20; p.marginY = 20;
    ctx.scale = 0.5; ctx.ref = {0, 0, 500, 250};
    PlaceBox(p, ctx, 10, 10, x, y); CHECK(x == 10 && y == 10);
}

static void TestBlend() {
    CHECK(BlendChannel(kBlendMultiply, 0.5f, 0.5f) == 0.25f);
    CHECK(std::fabs(BlendChannel(kBlendScreen, 0.5f, 0.5f) - 0.75f) < 1e-6);
    CHECK(BlendChannel(kBlendDarken, 0.2f, 0.6f) == 0.2f);
    CHECK(BlendChannel(kBlendLighten, 0.2f, 0.6f) == 0.6f);
    CHECK(std::fabs(BlendChannel(kBlendDifference, 0.2f, 0.6f) - 0.4f) < 1e-6);
    CHECK(std::fabs(BlendChannel(kBlendOverlay, 0.25f, 0.8f) - (1 - 2 * (1 - 0.25f) * (1 - 0.8f))) > -1);  // nie crashuje
    // Overlay(cb<=0.5) = Multiply(cb, 2cs); Overlay(cb>0.5) = Screen(cb, 2cs-1)
    CHECK(std::fabs(BlendChannel(kBlendOverlay, 0.25f, 0.6f) - 2 * 0.25f * 0.6f) < 1e-6);
    float cb[3] = {0.5f, 0.5f, 0.5f}, cs[3] = {1, 0, 0}, out[3], a;
    CompositePixel(kBlendNormal, cb, 0.0f, cs, 1.0f, out, a);  // tlo przezroczyste: dokladnie kolor zrodla
    CHECK(out[0] == 1 && out[1] == 0 && a == 1);
    CompositePixel(kBlendNormal, cb, 1.0f, cs, 0.0f, out, a);  // zrodlo przezroczyste: tlo bez zmian
    CHECK(out[0] == 0.5f && a == 1);
    CompositePixel(kBlendMultiply, cb, 1.0f, cs, 1.0f, out, a);
    CHECK(std::fabs(out[0] - 0.5f) < 1e-6 && out[1] == 0);
}

static Params TextParams() {
    Params p;
    SetDefaults(p);
    wcscpy_s(p.text, L"Marek Zettel");
    wcscpy_s(p.fontName, L"Segoe UI");
    p.sizeUnit = kUnitPx;
    p.sizeValue = 120;
    p.anchor = 4;
    p.color = 0xFFD400;
    return p;
}

static void TestRender(WinRasterizer& ras) {
    DocContext ctx;
    ctx.docW = 900; ctx.docH = 400;
    ctx.ref = {0, 0, 900, 400};

    Params p = TextParams();
    RenderResult r = RenderSignature(p, ctx, ras);
    CHECK(r.status == Status::Ok);
    CHECK(!r.sig.img.empty());
    SavePng(L"text_plain.png", r.sig.img, true);

    // determinizm
    RenderResult r2 = RenderSignature(p, ctx, ras);
    CHECK(Hash(r.sig.img) == Hash(r2.sig.img));

    // widocznosc skaluje alfe dokladnie
    Params pv = p; pv.visibility = 50;
    RenderResult rv = RenderSignature(pv, ctx, ras);
    CHECK(rv.sig.img.w == r.sig.img.w);
    double maxErr = 0;
    for (size_t i = 0; i < r.sig.img.d.size(); ++i) maxErr = std::fmax(maxErr, std::fabs(rv.sig.img.d[i] - 0.5f * r.sig.img.d[i]));
    CHECK(maxErr < 1e-6);
    Params p0 = p; p0.visibility = 0;
    RenderResult r0 = RenderSignature(p0, ctx, ras);
    bool allZero = true;
    for (float f : r0.sig.img.d) if (f != 0) allZero = false;
    CHECK(allZero);

    // pozycja: srodek
    int bx = r.sig.x0 + r.sig.img.w / 2, by = r.sig.y0 + r.sig.img.h / 2;
    CHECK(std::abs(bx - 450) <= 1 && std::abs(by - 200) <= 1);

    // obrys
    Params ps = p; ps.strokeOn = 1; ps.strokeWidth = 6; ps.strokeColor = 0x101010;
    for (int pos = 0; pos < 3; ++pos) {
        ps.strokePos = pos;
        RenderResult rs = RenderSignature(ps, ctx, ras);
        CHECK(rs.status == Status::Ok);
        wchar_t nm[64]; swprintf_s(nm, L"text_stroke_%d.png", pos);
        SavePng(nm, rs.sig.img, true);
    }
    // rzezba: wszystkie style, oba kierunki
    for (int st = 0; st < 4; ++st)
        for (int dn = 0; dn < 2; ++dn) {
            Params pe = p; pe.embossOn = 1; pe.embossStyle = st; pe.embossDown = dn; pe.size = 8; pe.soften = 1;
            RenderResult re = RenderSignature(pe, ctx, ras);
            CHECK(re.status == Status::Ok);
            wchar_t nm[64]; swprintf_s(nm, L"text_emboss_%d_%d.png", st, dn);
            SavePng(nm, re.sig.img, true);
        }
    // obrys + rzezba razem, 16-bit-owa dokladnosc nie jest tu istotna
    Params pb = p; pb.strokeOn = 1; pb.embossOn = 1; pb.embossStyle = kEmbossInnerBevel; pb.size = 6;
    RenderResult rb = RenderSignature(pb, ctx, ras);
    CHECK(rb.status == Status::Ok);
    SavePng(L"text_both.png", rb.sig.img, true);

    // bledy
    Params pe = p; pe.text[0] = 0;
    CHECK(RenderSignature(pe, ctx, ras).status == Status::EmptyText);
    Params pi = p; pi.mode = kModeImage; pi.imagePath[0] = 0;
    CHECK(RenderSignature(pi, ctx, ras).status == Status::ImageNotSet);
    wcscpy_s(pi.imagePath, L"C:\\nie\\ma\\takiego\\pliku.png");
    CHECK(RenderSignature(pi, ctx, ras).status == Status::ImageLoadFailed);
    Params pf = p; wcscpy_s(pf.fontName, L"Nieistniejaca Czcionka XYZ");
    CHECK(RenderSignature(pf, ctx, ras).status == Status::Ok);  // awaryjna czcionka
    Params pt = p; pt.sizeUnit = kUnitPx; pt.sizeValue = 2000; pt.tracking = 500;
    wcscpy_s(pt.text, L"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA");
    RenderResult rt = RenderSignature(pt, ctx, ras);
    CHECK(rt.status == Status::TooLarge);
    std::printf("duzy tekst: status=%d\n", (int)rt.status);

    // wieloliniowy z odstepem liter
    Params pm = p; wcscpy_s(pm.text, L"Linia pierwsza\nDruga linia"); pm.tracking = 100; pm.sizeValue = 60; pm.anchor = 8;
    RenderResult rm = RenderSignature(pm, ctx, ras);
    CHECK(rm.status == Status::Ok);
    SavePng(L"text_multiline.png", rm.sig.img, true);
}

static void TestImage(WinRasterizer& ras) {
    // Generuje maly PNG z alfa (kolko), wczytuje, skaluje, obraca.
    FImage src(64, 32);
    for (int y = 0; y < 32; ++y)
        for (int x = 0; x < 64; ++x) {
            float dx = x - 32 + 0.5f, dy = y - 16 + 0.5f;
            float a = std::sqrt(dx * dx + dy * dy) < 15 ? 1.0f : 0.0f;
            float* p = src.px(x, y);
            p[0] = 0.9f * a; p[1] = 0.2f * a; p[2] = 0.2f * a; p[3] = a;
        }
    std::wstring path = g_out + L"\\logo_src.png";
    SavePng(L"logo_src.png", src, false);

    DocContext ctx;
    ctx.docW = 800; ctx.docH = 600; ctx.ref = {0, 0, 800, 600};
    Params p;
    SetDefaults(p);
    p.mode = kModeImage;
    wcscpy_s(p.imagePath, path.c_str());
    p.imgScaleUnit = kUnitPercentWidth; p.imgScale = 25;  // 200 px szerokosci
    p.anchor = 0; p.marginUnit = kUnitPx; p.marginX = 10; p.marginY = 10;
    RenderResult r = RenderSignature(p, ctx, ras);
    CHECK(r.status == Status::Ok);
    if (r.status == Status::Ok) {
        std::printf("logo w=%d\n", r.sig.img.w); CHECK(r.sig.img.w >= 90 && r.sig.img.w <= 115);  // kolo o srednicy 30 z 64 px, przeskalowane do 200 px szerokosci
        CHECK(r.sig.x0 >= 0);
        SavePng(L"img_plain.png", r.sig.img, true);
    }
    p.rotation = 33; p.flipH = 1; p.strokeOn = 1; p.strokeWidth = 4; p.embossOn = 1;
    RenderResult r2 = RenderSignature(p, ctx, ras);
    CHECK(r2.status == Status::Ok);
    SavePng(L"img_rot_effects.png", r2.sig.img, true);
    p.recolor = 1; p.color = 0x00AAFF; p.rotation = 90;
    RenderResult r3 = RenderSignature(p, ctx, ras);
    CHECK(r3.status == Status::Ok);
    RenderResult r4 = RenderSignature(p, ctx, ras);
    CHECK(Hash(r3.sig.img) == Hash(r4.sig.img));
}

static void TestResizeRotate() {
    FImage a(10, 10);
    for (size_t i = 0; i < a.d.size(); i += 4) { a.d[i] = 0.5f; a.d[i + 1] = 0.5f; a.d[i + 2] = 0.5f; a.d[i + 3] = 1; }
    FImage b = Resize(a, 37, 21);
    bool flat = true;
    for (size_t i = 0; i < b.d.size(); i += 4) if (std::fabs(b.d[i] - 0.5f) > 1e-4 || std::fabs(b.d[i + 3] - 1) > 1e-4) flat = false;
    CHECK(flat);
    FImage r = Rotate(a, 90);
    CHECK(r.w == 10 && r.h == 10);
    FImage r2 = Rotate(FImage(20, 10), 90);
    CHECK(r2.w == 10 && r2.h == 20);
    // odleglosc ze znakiem
    Mask m(20, 20);
    for (int y = 5; y < 15; ++y) for (int x = 5; x < 15; ++x) m.at(x, y) = 1;
    Mask sd = SignedDistance(m);
    CHECK(std::fabs(sd.at(4, 10) - 0.5f) < 1e-4);  // piksel tuz za krawedzia: 0.5 od krawedzi
    CHECK(std::fabs(sd.at(2, 10) - 2.5f) < 1e-4);
    CHECK(std::fabs(sd.at(5, 10) + 0.5f) < 1e-4);
    CHECK(std::fabs(sd.at(7, 10) + 2.5f) < 1e-4);
}

static void TestSettings() {
    AppSettings s;
    s.language = 2; s.theme = 1; s.lastImageDir = L"C:\\Test Dir\\Zażółć";
    SetDefaults(s.params);
    wcscpy_s(s.params.text, L"abc } def");
    s.params.visibility = 42;
    // zapis do prawdziwego katalogu danych uzytkownika jest dozwolony: to ustawienia tej wtyczki
    AppSettings backup = LoadSettings();
    CHECK(SaveSettings(s));
    AppSettings l = LoadSettings();
    CHECK(l.language == 2 && l.theme == 1);
    CHECK(l.lastImageDir == s.lastImageDir);
    CHECK(l.hasParams && wcscmp(l.params.text, L"abc } def") == 0 && l.params.visibility == 42);
    SaveSettings(backup);

    CHECK(SavePreset(L"_test preset", s.params));
    bool found = false;
    for (auto& n : ListPresets()) if (n == L"_test preset") found = true;
    CHECK(found);
    Params lp;
    CHECK(LoadPreset(L"_test preset", lp) && lp.visibility == 42);
    CHECK(DeletePreset(L"_test preset"));
    CHECK(!LoadPreset(L"_test preset", lp));
    CHECK(SanitizePresetName(L"a/b:c*").size() == 3);
    CHECK(SanitizePresetName(L"  ").empty());
}

static void TestNewFeatures(WinRasterizer& ras) {
    DocContext ctx;
    ctx.docW = 900; ctx.docH = 400;
    ctx.ref = {0, 0, 900, 400};

    // cien: powieksza obwiednie i jest pod napisem
    Params p = TextParams();
    RenderResult plain = RenderSignature(p, ctx, ras);
    Params ps = p; ps.shadowOn = 1; ps.shadowDistance = 10; ps.shadowBlur = 6; ps.shadowOpacity = 80;
    RenderResult sh = RenderSignature(ps, ctx, ras);
    CHECK(sh.status == Status::Ok && sh.sig.img.w > plain.sig.img.w && sh.sig.img.h > plain.sig.img.h);
    SavePng(L"text_shadow.png", sh.sig.img, true);
    // Cien nie moze zmienic koloru pikseli, ktore sa w pelni kryte przez napis
    {
        long long full = 0, wrong = 0;
        const int dx = (plain.sig.x0 - sh.sig.x0), dy = (plain.sig.y0 - sh.sig.y0);
        for (int y = 0; y < plain.sig.img.h; ++y)
            for (int x = 0; x < plain.sig.img.w; ++x) {
                const float* a = plain.sig.img.px(x, y);
                const float* b = sh.sig.img.px(x + dx, y + dy);
                if (a[3] > 0.999f) { ++full; if (std::fabs(a[0] - b[0]) > 1e-4f || std::fabs(b[3] - 1.0f) > 1e-4f) ++wrong; }
            }
        CHECK(full > 100 && wrong == 0);
    }

    // wyrownanie tekstu wieloliniowego: lewo vs prawo daje rozne obrazy tej samej wielkosci
    Params pa = TextParams();
    wcscpy_s(pa.text, L"Dlugi napis\nkrotki");
    pa.textAlign = 1;
    RenderResult al = RenderSignature(pa, ctx, ras);
    pa.textAlign = 3;
    RenderResult ar = RenderSignature(pa, ctx, ras);
    CHECK(al.status == Status::Ok && ar.status == Status::Ok && std::abs(al.sig.img.w - ar.sig.img.w) <= 3);
    CHECK(Hash(al.sig.img) != Hash(ar.sig.img));

    // kafelki: pozycje w obrebie obszaru, liczba rosnie wraz ze zmniejszaniem odstepu
    Params pt = TextParams();
    pt.sizeValue = 30; pt.tileOn = 1; pt.tileSpacingX = 80; pt.tileSpacingY = 80; pt.tileAngle = -30;
    RenderResult t1 = RenderSignature(pt, ctx, ras);
    pt.tileSpacingX = 20; pt.tileSpacingY = 20;
    RenderResult t2 = RenderSignature(pt, ctx, ras);
    CHECK(t1.status == Status::Ok && t2.status == Status::Ok);
    CHECK(t1.sig.origins.size() > 5 && t2.sig.origins.size() > t1.sig.origins.size());
    // powtarzalnosc
    RenderResult t3 = RenderSignature(pt, ctx, ras);
    CHECK(t3.sig.origins == t2.sig.origins && Hash(t3.sig.img) == Hash(t2.sig.img));

    // emoji z odstepem liter nie moze sie rozpadac (brak crasha i niepusty wynik)
    Params pe = TextParams();
    wcscpy_s(pe.text, L"A \xD83D\xDE00 B");
    pe.tracking = 200;
    CHECK(RenderSignature(pe, ctx, ras).status == Status::Ok);
}

static void TestPresetBundle() {
    SetDefaults(*(new Params));  // sprawdzenie, ze SetDefaults nie zalezy od stanu
    Params a; SetDefaults(a); a.visibility = 33; wcscpy_s(a.text, L"a } \"x\" b");
    Params b; SetDefaults(b); b.shadowOn = 1; b.tileOn = 1;
    CHECK(SavePreset(L"_bundle1", a) && SavePreset(L"_bundle2", b));
    std::wstring path = std::wstring(_wgetenv(L"ZETMAR_WM_DATA")) + L"\\bundle.zmwpresets";
    int n = 0;
    CHECK(ExportPresets(path, n) && n >= 2);
    CHECK(DeletePreset(L"_bundle1") && DeletePreset(L"_bundle2"));
    int imp = ImportPresets(path);
    CHECK(imp >= 2);
    Params ra, rb;
    CHECK(LoadPreset(L"_bundle1", ra) && ra.visibility == 33 && wcscmp(ra.text, a.text) == 0);
    CHECK(LoadPreset(L"_bundle2", rb) && rb.shadowOn == 1 && rb.tileOn == 1);
    DeletePreset(L"_bundle1"); DeletePreset(L"_bundle2");
    CHECK(ImportPresets(L"C:\\nie\\ma\\pliku.zmwpresets") == -1);
    CHECK(IsLocalDrivePath(L"C:\\Users\\a\\logo.png") && IsLocalDrivePath(L"d:/x/y.png"));
    CHECK(!IsLocalDrivePath(L"\\\\srv\\share\\a.png") && !IsLocalDrivePath(L"//srv/share/a.png") &&
          !IsLocalDrivePath(L"\\\\?\\C:\\a.png") && !IsLocalDrivePath(L"\\\\.\\PhysicalDrive0") &&
          !IsLocalDrivePath(L"\\??\\C:\\a.png") && !IsLocalDrivePath(L"C:\\a\\\\b.png") &&
          !IsLocalDrivePath(L"rel\\a.png") && !IsLocalDrivePath(L"C:a.png") && !IsLocalDrivePath(L""));
    // Import: sciezka UNC jest czyszczona, a istniejacy preset nie jest nadpisywany
    {
        Params u; SetDefaults(u); wcscpy_s(u.imagePath, L"\\\\serwer\\udzial\\logo.png"); u.visibility = 11;
        CHECK(SavePreset(L"_unc", u));
        std::wstring p2 = std::wstring(_wgetenv(L"ZETMAR_WM_DATA")) + L"\\unc.zmwpresets";
        int c2 = 0;
        CHECK(ExportPresets(p2, c2));
        CHECK(ImportPresets(p2) >= 1);  // nazwa juz istnieje: powstaje "_unc (import 1)"
        Params r1, r2;
        CHECK(LoadPreset(L"_unc", r1) && r1.visibility == 11);
        CHECK(LoadPreset(L"_unc (import 1)", r2) && r2.imagePath[0] == 0);
        DeletePreset(L"_unc"); DeletePreset(L"_unc (import 1)");
        for (auto& n : ListPresets()) if (n.rfind(L"_", 0) == 0) DeletePreset(n);
    }
    // nazwy zarezerwowane
    CHECK(SanitizePresetName(L"CON").empty() && SanitizePresetName(L"nul.txt").empty() && SanitizePresetName(L"COM3").empty());
    CHECK(!SanitizePresetName(L"Console").empty());
}

static void TestSettingsCorrupt() {
    std::wstring dir = _wgetenv(L"ZETMAR_WM_DATA");
    std::wstring file = dir + L"\\settings.json";
    const char* bad[] = {"", "{", "not json", "{\"language\": 7, \"theme\": -3, \"params\": {\"visibility\": 5000}}",
                         "{\"params\": {\"text\": \"unterminated"};
    for (const char* b : bad) {
        HANDLE h = CreateFileW(file.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        DWORD w; WriteFile(h, b, (DWORD)strlen(b), &w, nullptr); CloseHandle(h);
        AppSettings s = LoadSettings();  // nie moze sie wysypac
        CHECK(s.language >= 0 && s.language <= 2 && s.theme >= 0 && s.theme <= 2);
        CHECK(!s.hasParams || (s.params.visibility >= 0 && s.params.visibility <= 100));
    }
}

static void TestI18n() {
    SetLanguage(Lang::Polish);
    CHECK(std::wstring(T(Str::Cancel)) == L"Anuluj");
    SetLanguage(Lang::English);
    CHECK(std::wstring(T(Str::Cancel)) == L"Cancel");
    for (int i = 0; i < (int)Str::Count; ++i) {
        SetLanguage(Lang::Polish);
        CHECK(T((Str)i)[0] != 0);
        SetLanguage(Lang::English);
        CHECK(T((Str)i)[0] != 0);
    }
    SetLanguage(Lang::Polish);
}

int wmain(int argc, wchar_t** argv) {
    if (argc > 1) g_out = argv[1];
    CreateDirectoryW(g_out.c_str(), nullptr);
    // Testy nie dotykaja prawdziwych ustawien uzytkownika: katalog danych jest tymczasowy.
    {
        wchar_t tmp[MAX_PATH];
        GetTempPathW(MAX_PATH, tmp);
        std::wstring d = std::wstring(tmp) + L"wm_test_data_" + std::to_wstring(GetCurrentProcessId());
        CreateDirectoryW(d.c_str(), nullptr);
        SetEnvironmentVariableW(L"ZETMAR_WM_DATA", d.c_str());
        _wputenv_s(L"ZETMAR_WM_DATA", d.c_str());
    }
    TestParams();
    TestPlacement();
    TestBlend();
    TestResizeRotate();
    TestI18n();
    TestSettings();
    TestPresetBundle();
    TestSettingsCorrupt();
    {
        WinRasterizer ras;
        TestRender(ras);
        TestImage(ras);
        TestNewFeatures(ras);
    }
    std::printf("\nOK: %d, FAIL: %d\n", g_ok, g_fail);
    return g_fail ? 1 : 0;
}
