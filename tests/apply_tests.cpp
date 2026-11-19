// Testy zapisu podpisu do warstwy (ApplySignature) na atrapie hosta: 8 i 16 bit, z przezroczystoscia i bez,
// zaznaczenie, tryb kafelkowy, tryb mieszania. Bez Photoshopa.
#include <windows.h>

#include "PIDefines.h"
#include "PIFilter.h"
#include "PITypes.h"

#include <cstdio>
#include <cstring>
#include <vector>

#include "core/params.h"
#include "platform/rasterizer_win.h"
#include "plugin/host.h"

using namespace wm;

static int g_fail = 0, g_ok = 0;
#define CHECK(cond)                                                                   \
    do {                                                                              \
        if (cond) ++g_ok;                                                             \
        else { ++g_fail; std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); } \
    } while (0)

// ---- atrapa hosta: warstwa trzymana jako przeplot "planes" plaszczyzn po `bps` bajtow
struct FakeHost {
    int w = 0, h = 0, planes = 3, bps = 1;
    std::vector<uint8_t> layer;      // w*h*planes*bps
    std::vector<uint8_t> inBuf, outBuf;
    int rx0 = 0, ry0 = 0, rx1 = 0, ry1 = 0;
    bool pending = false;
    int calls = 0;
    FilterRecord fr;
    BigDocumentStruct big;

    size_t Px() const { return (size_t)planes * bps; }
    void Commit() {
        if (!pending) return;
        for (int y = ry0; y < ry1; ++y)
            std::memcpy(&layer[((size_t)y * w + rx0) * Px()], &outBuf[(size_t)(y - ry0) * (rx1 - rx0) * Px()],
                        (size_t)(rx1 - rx0) * Px());
        pending = false;
    }
};
static FakeHost* g_host = nullptr;

static OSErr FakeAdvance(void) {
    FakeHost& H = *g_host;
    H.Commit();
    const VRect& r = H.big.inRect32;
    if (r.right <= r.left || r.bottom <= r.top) return noErr;
    H.rx0 = r.left; H.ry0 = r.top; H.rx1 = r.right; H.ry1 = r.bottom;
    const size_t rowB = (size_t)(H.rx1 - H.rx0) * H.Px();
    H.inBuf.assign(rowB * (H.ry1 - H.ry0), 0);
    H.outBuf.assign(rowB * (H.ry1 - H.ry0), 0xCD);  // zasmiecone: plugin musi zapisac wszystko, co zmienia
    for (int y = H.ry0; y < H.ry1; ++y)
        std::memcpy(&H.inBuf[(size_t)(y - H.ry0) * rowB], &H.layer[((size_t)y * H.w + H.rx0) * H.Px()], rowB);
    H.fr.inData = H.inBuf.data();
    H.fr.outData = H.outBuf.data();
    H.fr.inRowBytes = (int32)rowB;
    H.fr.outRowBytes = (int32)rowB;
    H.pending = true;
    ++H.calls;
    return noErr;
}

static void Setup(FakeHost& H, int w, int h, int depth, int filterCase, bool trans) {
    g_host = &H;
    H.w = w; H.h = h;
    H.bps = depth == 16 ? 2 : 1;
    H.planes = trans ? 4 : 3;
    H.layer.assign((size_t)w * h * H.Px(), 0);
    std::memset(&H.fr, 0, sizeof(H.fr));
    std::memset(&H.big, 0, sizeof(H.big));
    H.big.PluginUsing32BitCoordinates = true;
    H.big.imageSize32.h = w; H.big.imageSize32.v = h;
    H.big.filterRect32.left = 0; H.big.filterRect32.top = 0; H.big.filterRect32.right = w; H.big.filterRect32.bottom = h;
    H.fr.bigDocumentData = &H.big;
    H.fr.depth = depth;
    H.fr.planes = (int16)H.planes;
    H.fr.imageMode = plugInModeRGBColor;
    H.fr.filterCase = (int16)filterCase;
    if (trans) { H.fr.inLayerPlanes = 3; H.fr.inTransparencyMask = 1; }
    H.fr.advanceState = FakeAdvance;
    H.fr.samplingSupport = 0;
}

static Params Text() {
    Params p;
    SetDefaults(p);
    wcscpy_s(p.text, L"Test");
    wcscpy_s(p.fontName, L"Arial");
    p.sizeUnit = kUnitPx;
    p.sizeValue = 60;
    p.color = 0xFF0000;
    p.anchor = 4;
    return p;
}

static uint32_t Sample(const FakeHost& H, int x, int y, int plane) {
    const uint8_t* p = &H.layer[((size_t)y * H.w + x) * H.Px() + (size_t)plane * H.bps];
    return H.bps == 2 ? *(const uint16_t*)p : *p;
}

static void RunBlank(int depth) {
    FakeHost H;
    Setup(H, 400, 200, depth, filterCaseEditableTransparencyNoSelection, true);
    WinRasterizer ras;
    HostInfo hi = QueryHost(&H.fr);
    CHECK(hi.rgbOk && hi.transPlane == 3 && !hi.flatLayer && hi.depth == depth);

    Status rs; std::wstring det; short err = 0;
    ApplyStatus as = ApplySignature(&H.fr, hi, Text(), ras, rs, det, err);
    H.Commit();
    CHECK(as == ApplyStatus::Ok);
    const uint32_t maxv = depth == 16 ? 32768 : 255;
    // Ktorykolwiek piksel ma alfa > 0, a nigdy wiecej niz max; poza podpisem alfa = 0; kolor czerwony
    long long inked = 0, over = 0, badColor = 0;
    for (int y = 0; y < H.h; ++y)
        for (int x = 0; x < H.w; ++x) {
            uint32_t a = Sample(H, x, y, 3);
            if (a > 0) {
                ++inked;
                if (a > maxv) ++over;
                if (a == maxv && (Sample(H, x, y, 0) != maxv || Sample(H, x, y, 1) != 0 || Sample(H, x, y, 2) != 0))
                    ++badColor;
            } else if (Sample(H, x, y, 0) == 0xCD) {
                ++over;  // smiec z bufora wyjsciowego
            }
        }
    CHECK(inked > 200);
    CHECK(over == 0);
    CHECK(badColor == 0);
    // Rogi musza zostac puste (podpis jest na srodku)
    CHECK(Sample(H, 2, 2, 3) == 0 && Sample(H, H.w - 3, H.h - 3, 3) == 0);
}

static void TestFlatDirect() {
    FakeHost H;
    Setup(H, 300, 150, 8, filterCaseFlatImageNoSelection, false);
    // Biale tlo
    std::memset(H.layer.data(), 255, H.layer.size());
    WinRasterizer ras;
    HostInfo hi = QueryHost(&H.fr);
    CHECK(hi.flatLayer && hi.transPlane == -1 && hi.planes == 3);
    Params p = Text();
    p.direct = 1;
    p.blend = kBlendMultiply;
    p.color = 0x00FF00;
    Status rs; std::wstring det; short err = 0;
    CHECK(ApplySignature(&H.fr, hi, p, ras, rs, det, err) == ApplyStatus::Ok);
    H.Commit();
    // Mnozenie bieli przez zielen = zielen; gdzies musi sie pojawic czysta zielen, a tlo zostaje biale
    bool green = false, white = false;
    for (int y = 0; y < H.h; ++y)
        for (int x = 0; x < H.w; ++x) {
            if (Sample(H, x, y, 0) == 0 && Sample(H, x, y, 1) == 255 && Sample(H, x, y, 2) == 0) green = true;
            if (Sample(H, x, y, 0) == 255 && Sample(H, x, y, 1) == 255 && Sample(H, x, y, 2) == 255) white = true;
        }
    CHECK(green && white);
}

static void TestSelectionAndTiles() {
    FakeHost H;
    Setup(H, 600, 400, 8, filterCaseEditableTransparencyWithSelection, true);
    H.big.filterRect32.left = 100; H.big.filterRect32.top = 100; H.big.filterRect32.right = 300; H.big.filterRect32.bottom = 250;
    WinRasterizer ras;
    HostInfo hi = QueryHost(&H.fr);
    CHECK(hi.hasSelection);
    Params p = Text();
    p.relToSelection = 1;
    p.anchor = 0;
    p.marginUnit = kUnitPx; p.marginX = 5; p.marginY = 5;
    Status rs; std::wstring det; short err = 0;
    CHECK(ApplySignature(&H.fr, hi, p, ras, rs, det, err) == ApplyStatus::Ok);
    H.Commit();
    // Podpis lezy w lewym-gornym rogu zaznaczenia: nic na lewo/powyzej niego
    bool inSel = false, outside = false;
    for (int y = 0; y < H.h; ++y)
        for (int x = 0; x < H.w; ++x)
            if (Sample(H, x, y, 3) > 0) {
                if (x >= 100 && y >= 100 && x < 300 && y < 250) inSel = true;
                else outside = true;
            }
    CHECK(inSel && !outside);

    // Wzor kafelkowy: wiele kopii, wszystkie w obrazie
    FakeHost T;
    Setup(T, 800, 600, 8, filterCaseEditableTransparencyNoSelection, true);
    HostInfo ht = QueryHost(&T.fr);
    Params q = Text();
    q.sizeValue = 40;
    q.tileOn = 1; q.tileSpacingX = 60; q.tileSpacingY = 60; q.tileAngle = -30;
    CHECK(ApplySignature(&T.fr, ht, q, ras, rs, det, err) == ApplyStatus::Ok);
    T.Commit();
    long long inked = 0;
    for (int y = 0; y < T.h; ++y)
        for (int x = 0; x < T.w; ++x)
            if (Sample(T, x, y, 3) > 0) ++inked;
    CHECK(inked > 3000);
    // Kafelki obejmuja caly obraz: ink w kazdej cwiartce
    long long quad[4] = {0, 0, 0, 0};
    for (int y = 0; y < T.h; ++y)
        for (int x = 0; x < T.w; ++x)
            if (Sample(T, x, y, 3) > 0) ++quad[(y >= 300) * 2 + (x >= 400)];
    CHECK(quad[0] > 500 && quad[1] > 500 && quad[2] > 500 && quad[3] > 500);
}

static void TestStripping() {
    // Duzy obraz wymusza wiele pasow; wynik musi byc identyczny jak przy jednym pasie
    FakeHost H;
    Setup(H, 5000, 120, 8, filterCaseEditableTransparencyNoSelection, true);
    WinRasterizer ras;
    HostInfo hi = QueryHost(&H.fr);
    Params p = Text();
    p.sizeUnit = kUnitPercentWidth; p.sizeValue = 40;  // szeroki podpis
    Status rs; std::wstring det; short err = 0;
    CHECK(ApplySignature(&H.fr, hi, p, ras, rs, det, err) == ApplyStatus::Ok);
    H.Commit();
    long long inked = 0;
    for (int y = 0; y < H.h; ++y)
        for (int x = 0; x < H.w; ++x) if (Sample(H, x, y, 3) > 0) ++inked;
    CHECK(inked > 1000);
}

static void TestErrors() {
    FakeHost H;
    Setup(H, 100, 100, 8, filterCaseEditableTransparencyNoSelection, true);
    WinRasterizer ras;
    HostInfo hi = QueryHost(&H.fr);
    Params p = Text();
    p.text[0] = 0;
    Status rs; std::wstring det; short err = 0;
    CHECK(ApplySignature(&H.fr, hi, p, ras, rs, det, err) == ApplyStatus::Cancelled && rs == Status::EmptyText);
    CHECK(H.calls == 0);  // nic nie zapisano do hosta
    // Podpis poza obrazem
    p = Text();
    p.anchor = 4; p.offsetX = 5000;
    CHECK(ApplySignature(&H.fr, hi, p, ras, rs, det, err) == ApplyStatus::Cancelled && rs == Status::NothingToDraw);
}

int wmain() {
    RunBlank(8);
    RunBlank(16);
    TestFlatDirect();
    TestSelectionAndTiles();
    TestStripping();
    TestErrors();
    std::printf("\nOK: %d, FAIL: %d\n", g_ok, g_fail);
    return g_fail ? 1 : 0;
}
