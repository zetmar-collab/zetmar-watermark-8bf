// Glowne okno wtyczki: podglad po lewej, panel z zakladkami po prawej.
#include "ui/dialog.h"

#include <commctrl.h>
#include <commdlg.h>
#include <windowsx.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <cwchar>
#include <map>
#include <string>
#include <vector>

#include "core/blend.h"
#include "core/i18n.h"
#include "platform/rasterizer_win.h"
#include "ui/theme.h"

namespace wm {
namespace ui {

namespace {

const wchar_t kMainClass[] = L"ZetmarWmMain";
const wchar_t kPreviewClass[] = L"ZetmarWmPreview";
const UINT_PTR kPreviewTimer = 1;

enum Page { kPageType = 0, kPagePosition, kPageEffects, kPageVisibility, kPageSettings, kPageCount };
// group: 0 = zawsze na stronie; 1 = tylko tryb tekstu; 2 = tylko tryb grafiki
enum Group { kGAlways = 0, kGText = 1, kGImage = 2 };

enum class Kind { Label, Edit, MultiEdit, Check, Radio, Push, Swatch, Combo, Slider, Grid, Tab, Font };

enum Ids {
    IDC_TAB0 = 100,        // ..104
    IDC_OKB = 110, IDC_CANCELB, IDC_RESETB,
    IDC_BROWSE = 120, IDC_USEFORE, IDC_PRESET_COMBO, IDC_PRESET_LOAD, IDC_PRESET_SAVE, IDC_PRESET_DEL,
    IDC_LANG, IDC_THEME, IDC_SLIDER, IDC_FONT, IDC_TEXT, IDC_IMGPATH, IDC_PRESET_NAME,
    IDC_DYNAMIC = 1000,
};

struct Ctl {
    int id = 0;
    HWND h = nullptr;
    Kind kind = Kind::Label;
    int page = -1;  // -1 = zawsze widoczny (przyciski dolne, zakladki)
    int group = kGAlways;
    int rx = 0, ry = 0, rw = 0, rh = 0;
    bool bottomAnchored = false;  // ry liczone od dolu panelu
    double Params::*dfield = nullptr;
    int32_t Params::*ifield = nullptr;
    uint32_t Params::*cfield = nullptr;
    int radioValue = 0;
    int decimals = 0;
    Str textKey = Str::Count;
    // warunki aktywnosci: 0 = zawsze, 1 = obrys wlaczony, 2 = rzezba wlaczona, 3 = tryb bezposredni,
    // 4 = jest zaznaczenie, 5 = nie wymuszony tryb bezposredni
    int enableRule = 0;
};

int32_t g_dummy = 0;
COLORREF g_custom[16];

std::wstring FormatNumber(double v, int decimals) {
    wchar_t buf[48];
    swprintf_s(buf, L"%.*f", decimals, v);
    std::wstring s = buf;
    if (decimals > 0) {
        while (!s.empty() && s.back() == L'0') s.pop_back();
        if (!s.empty() && s.back() == L'.') s.pop_back();
    }
    return s;
}

bool ParseNumber(const std::wstring& in, double& out) {
    std::wstring s = in;
    for (auto& c : s) if (c == L',') c = L'.';
    size_t b = s.find_first_not_of(L' ');
    if (b == std::wstring::npos) return false;
    wchar_t* end = nullptr;
    double v = wcstod(s.c_str() + b, &end);
    if (end == s.c_str() + b) return false;
    while (*end == L' ') ++end;
    if (*end) return false;
    out = v;
    return true;
}

COLORREF ToColorRef(uint32_t rgb) { return RGB((rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF); }
uint32_t FromColorRef(COLORREF c) { return ((uint32_t)GetRValue(c) << 16) | ((uint32_t)GetGValue(c) << 8) | GetBValue(c); }

UINT DpiFor(HWND h) {
    typedef UINT(WINAPI * Fn)(HWND);
    static Fn fn = (Fn)GetProcAddress(GetModuleHandleW(L"user32.dll"), "GetDpiForWindow");
    if (fn && h) { UINT d = fn(h); if (d) return d; }
    HDC dc = GetDC(nullptr);
    int d = GetDeviceCaps(dc, LOGPIXELSX);
    ReleaseDC(nullptr, dc);
    return d ? d : 96;
}

class Dialog {
public:
    Dialog(DialogContext& c, Params& p) : ctx_(c), result_(p), p_(p) {}
    bool Run();

private:
    // --- tworzenie
    void RegisterClasses();
    void Build();
    void DestroyControls();
    HWND NewWindow(Ctl& c, const wchar_t* cls, const wchar_t* text, DWORD style, DWORD ex = 0);
    Ctl& AddLabel(int page, int group, int x, int y, Str key, int w = 0);
    Ctl& AddEdit(int page, int group, int x, int y, int w, double Params::*f, int decimals, int rule = 0);
    Ctl& AddCheck(int page, int group, int x, int y, int w, Str key, int32_t Params::*f, int rule = 0);
    Ctl& AddRadio(int page, int group, int x, int y, int w, Str key, int32_t Params::*f, int value);
    Ctl& AddPush(int page, int group, int x, int y, int w, int h, int id, Str key, int rule = 0);
    Ctl& AddSwatch(int page, int group, int x, int y, int w, uint32_t Params::*f, int rule = 0);
    Ctl& AddCombo(int page, int group, int x, int y, int w, int32_t Params::*f, std::initializer_list<Str> items,
                  int rule = 0);
    void BuildTypePage(int labelW);
    void BuildPositionPage(int labelW);
    void BuildEffectsPage(int labelW);
    void BuildVisibilityPage(int labelW);
    void BuildSettingsPage(int labelW);

    // --- stan
    void Layout();
    void UpdateStates();
    void SyncControls();
    void SwitchPage(int page);
    bool VisibleFor(const Ctl& c) const;
    bool EnabledFor(const Ctl& c) const;
    Ctl* Find(int id);
    Ctl* FindByHwnd(HWND h);
    void Changed();
    void RefreshFontStyles();
    void ReloadPresets();
    void Rebuild();

    // --- zdarzenia
    LRESULT OnCommand(WPARAM w, LPARAM l);
    LRESULT OnDrawItem(const DRAWITEMSTRUCT* d);
    LRESULT OnMeasureItem(MEASUREITEMSTRUCT* m);
    LRESULT OnCtlColor(UINT msg, HDC dc, HWND h);
    void OnClicked(Ctl& c);
    void OnEditChange(Ctl& c);
    void OnEditKillFocus(Ctl& c);
    void PickColor(uint32_t Params::*field);
    void BrowseImage();
    bool Validate();
    void DoReset();
    void PresetLoad();
    void PresetSave();
    void PresetDelete();
    void Finish(bool ok);

    // --- podglad
    void PreparePreviewBase();
    void RenderPreview();
    void PaintPreview(HWND h);
    LRESULT PreviewProc(HWND h, UINT m, WPARAM w, LPARAM l);

    static LRESULT CALLBACK MainProcThunk(HWND h, UINT m, WPARAM w, LPARAM l);
    static LRESULT CALLBACK PreviewProcThunk(HWND h, UINT m, WPARAM w, LPARAM l);
    LRESULT MainProc(HWND h, UINT m, WPARAM w, LPARAM l);

    int S(int v) const { return Scale(v, dpi_); }

    DialogContext& ctx_;
    Params& result_;
    Params p_;
    AppSettings* st_ = nullptr;
    HWND hwnd_ = nullptr, preview_ = nullptr;
    int dpi_ = 96;
    HFONT font_ = nullptr, fontBold_ = nullptr;
    Theme theme_;
    std::vector<Ctl> ctls_;
    int nextId_ = IDC_DYNAMIC;
    int page_ = kPageType;
    bool syncing_ = false, done_ = false, ok_ = false;
    int panelW_ = 0, tabH_ = 0, bottomH_ = 0;
    std::vector<std::wstring> fonts_;
    std::map<std::wstring, HFONT> fontCache_;
    std::vector<std::wstring> presets_;

    // podglad
    PreviewBase base_;
    bool haveBase_ = false;
    int pvW_ = 0, pvH_ = 0;
    std::vector<uint8_t> flatBase_;  // podglad tla splaszczony na szachownice (BGRA)
    HBITMAP pvBmp_ = nullptr;
    uint8_t* pvBits_ = nullptr;
    std::wstring pvMessage_;
    double zoom_ = 1.0, panX_ = 0, panY_ = 0;
    bool dragging_ = false;
    POINT dragStart_{};
    double dragPanX_ = 0, dragPanY_ = 0;
    bool renderBusy_ = false;
};

// ----------------------------------------------------------------------------------------------
// Tworzenie kontrolek

HWND Dialog::NewWindow(Ctl& c, const wchar_t* cls, const wchar_t* text, DWORD style, DWORD ex) {
    c.h = CreateWindowExW(ex, cls, text, style | WS_CHILD | WS_CLIPSIBLINGS, 0, 0, 10, 10, hwnd_,
                          (HMENU)(INT_PTR)c.id, ModuleInstance(), nullptr);
    if (c.h && font_) SendMessageW(c.h, WM_SETFONT, (WPARAM)font_, TRUE);
    return c.h;
}

Ctl& Dialog::AddLabel(int page, int group, int x, int y, Str key, int w) {
    ctls_.emplace_back();
    Ctl& c = ctls_.back();
    c.id = nextId_++;
    c.kind = Kind::Label;
    c.page = page; c.group = group; c.textKey = key;
    c.rx = x; c.ry = y + S(5); c.rw = w ? w : S(200); c.rh = S(20);
    NewWindow(c, L"STATIC", T(key), SS_LEFT | SS_NOPREFIX);
    return c;
}

Ctl& Dialog::AddEdit(int page, int group, int x, int y, int w, double Params::*f, int decimals, int rule) {
    ctls_.emplace_back();
    Ctl& c = ctls_.back();
    c.id = nextId_++;
    c.kind = Kind::Edit;
    c.page = page; c.group = group; c.dfield = f; c.decimals = decimals; c.enableRule = rule;
    c.rx = x; c.ry = y; c.rw = w; c.rh = S(26);
    NewWindow(c, L"EDIT", L"", ES_AUTOHSCROLL | WS_TABSTOP, WS_EX_CLIENTEDGE);
    ApplyControlTheme(c.h, theme_.dark, L"CFD");
    return c;
}

Ctl& Dialog::AddCheck(int page, int group, int x, int y, int w, Str key, int32_t Params::*f, int rule) {
    ctls_.emplace_back();
    Ctl& c = ctls_.back();
    c.id = nextId_++;
    c.kind = Kind::Check;
    c.page = page; c.group = group; c.ifield = f; c.textKey = key; c.enableRule = rule;
    c.rx = x; c.ry = y + S(2); c.rw = w; c.rh = S(24);
    NewWindow(c, L"BUTTON", T(key), BS_OWNERDRAW | WS_TABSTOP);
    return c;
}

Ctl& Dialog::AddRadio(int page, int group, int x, int y, int w, Str key, int32_t Params::*f, int value) {
    ctls_.emplace_back();
    Ctl& c = ctls_.back();
    c.id = nextId_++;
    c.kind = Kind::Radio;
    c.page = page; c.group = group; c.ifield = f; c.radioValue = value; c.textKey = key;
    c.rx = x; c.ry = y + S(2); c.rw = w; c.rh = S(24);
    NewWindow(c, L"BUTTON", T(key), BS_OWNERDRAW | WS_TABSTOP);
    return c;
}

Ctl& Dialog::AddPush(int page, int group, int x, int y, int w, int h, int id, Str key, int rule) {
    ctls_.emplace_back();
    Ctl& c = ctls_.back();
    c.id = id ? id : nextId_++;
    c.kind = Kind::Push;
    c.page = page; c.group = group; c.textKey = key; c.enableRule = rule;
    c.rx = x; c.ry = y; c.rw = w; c.rh = h;
    NewWindow(c, L"BUTTON", T(key), BS_OWNERDRAW | WS_TABSTOP);
    return c;
}

Ctl& Dialog::AddSwatch(int page, int group, int x, int y, int w, uint32_t Params::*f, int rule) {
    ctls_.emplace_back();
    Ctl& c = ctls_.back();
    c.id = nextId_++;
    c.kind = Kind::Swatch;
    c.page = page; c.group = group; c.cfield = f; c.enableRule = rule;
    c.rx = x; c.ry = y; c.rw = w; c.rh = S(26);
    NewWindow(c, L"BUTTON", L"", BS_OWNERDRAW | WS_TABSTOP);
    return c;
}

Ctl& Dialog::AddCombo(int page, int group, int x, int y, int w, int32_t Params::*f, std::initializer_list<Str> items,
                      int rule) {
    ctls_.emplace_back();
    Ctl& c = ctls_.back();
    c.id = nextId_++;
    c.kind = Kind::Combo;
    c.page = page; c.group = group; c.ifield = f; c.enableRule = rule;
    c.rx = x; c.ry = y; c.rw = w; c.rh = S(26) + S(26) * (int)items.size();
    NewWindow(c, L"COMBOBOX", L"", CBS_DROPDOWNLIST | CBS_OWNERDRAWFIXED | CBS_HASSTRINGS | WS_TABSTOP | WS_VSCROLL);
    ApplyControlTheme(c.h, theme_.dark, L"CFD");
    for (Str s : items) SendMessageW(c.h, CB_ADDSTRING, 0, (LPARAM)T(s));
    SendMessageW(c.h, CB_SETITEMHEIGHT, (WPARAM)-1, S(20));
    SendMessageW(c.h, CB_SETITEMHEIGHT, 0, S(24));
    return c;
}

void Dialog::BuildTypePage(int lw) {
    const int P = S(16), RH = S(32), fx = P + lw + S(8);
    const int fw = panelW_ - fx - P;
    int y = S(8);
    AddRadio(kPageType, kGAlways, P, y, S(150), Str::ModeText, &Params::mode, kModeText);
    AddRadio(kPageType, kGAlways, P + S(170), y, S(150), Str::ModeImage, &Params::mode, kModeImage);
    y += RH;

    // --- tekst
    AddLabel(kPageType, kGText, P, y, Str::TextLabel, panelW_ - 2 * P);
    y += S(26);
    {
        ctls_.emplace_back();
        Ctl& c = ctls_.back();
        c.id = IDC_TEXT; c.kind = Kind::MultiEdit; c.page = kPageType; c.group = kGText;
        c.rx = P; c.ry = y; c.rw = panelW_ - 2 * P; c.rh = S(72);
        NewWindow(c, L"EDIT", L"", ES_MULTILINE | ES_WANTRETURN | ES_AUTOVSCROLL | WS_VSCROLL | WS_TABSTOP,
                  WS_EX_CLIENTEDGE);
        ApplyControlTheme(c.h, theme_.dark, L"CFD");
        SendMessageW(c.h, EM_LIMITTEXT, 500, 0);
    }
    y += S(80);
    AddLabel(kPageType, kGText, P, y, Str::Font, lw);
    {
        ctls_.emplace_back();
        Ctl& c = ctls_.back();
        c.id = IDC_FONT; c.kind = Kind::Font; c.page = kPageType; c.group = kGText;
        c.rx = fx; c.ry = y; c.rw = fw; c.rh = S(26) + S(26) * 12;
        NewWindow(c, L"COMBOBOX", L"", CBS_DROPDOWNLIST | CBS_OWNERDRAWFIXED | CBS_HASSTRINGS | WS_TABSTOP | WS_VSCROLL);
        ApplyControlTheme(c.h, theme_.dark, L"CFD");
        SendMessageW(c.h, CB_SETITEMHEIGHT, (WPARAM)-1, S(20));
        SendMessageW(c.h, CB_SETITEMHEIGHT, 0, S(26));
        SendMessageW(c.h, WM_SETREDRAW, FALSE, 0);
        for (const auto& f : fonts_) SendMessageW(c.h, CB_ADDSTRING, 0, (LPARAM)f.c_str());
        SendMessageW(c.h, WM_SETREDRAW, TRUE, 0);
    }
    y += RH;
    AddCheck(kPageType, kGText, fx, y, S(150), Str::Bold, &Params::bold, 6).enableRule = 6;
    AddCheck(kPageType, kGText, fx + S(160), y, S(150), Str::Italic, &Params::italic, 7).enableRule = 7;
    y += RH;
    AddLabel(kPageType, kGText, P, y, Str::Size, lw);
    AddEdit(kPageType, kGText, fx, y, S(80), &Params::sizeValue, 1);
    AddCombo(kPageType, kGText, fx + S(88), y, fw - S(88), &Params::sizeUnit, {Str::UnitPx, Str::UnitPctWidth});
    y += RH;
    AddLabel(kPageType, kGText, P, y, Str::Tracking, lw);
    AddEdit(kPageType, kGText, fx, y, S(80), &Params::tracking, 0);
    y += RH;
    AddLabel(kPageType, kGText, P, y, Str::LineSpacing, lw);
    AddEdit(kPageType, kGText, fx, y, S(80), &Params::lineSpacing, 0);
    y += RH;

    // --- grafika (ten sam obszar co tekst; widoczne naprzemiennie)
    int yi = S(8) + RH;
    AddLabel(kPageType, kGImage, P, yi, Str::ImageFile, lw);
    {
        ctls_.emplace_back();
        Ctl& c = ctls_.back();
        c.id = IDC_IMGPATH; c.kind = Kind::Edit; c.page = kPageType; c.group = kGImage;
        c.rx = fx; c.ry = yi; c.rw = fw - S(108); c.rh = S(26);
        NewWindow(c, L"EDIT", L"", ES_AUTOHSCROLL | WS_TABSTOP, WS_EX_CLIENTEDGE);
        ApplyControlTheme(c.h, theme_.dark, L"CFD");
        SendMessageW(c.h, EM_LIMITTEXT, 259, 0);
    }
    AddPush(kPageType, kGImage, fx + fw - S(100), yi, S(100), S(26), IDC_BROWSE, Str::Browse);
    yi += RH;
    AddLabel(kPageType, kGImage, P, yi, Str::Scale, lw);
    AddEdit(kPageType, kGImage, fx, yi, S(80), &Params::imgScale, 1);
    AddCombo(kPageType, kGImage, fx + S(88), yi, fw - S(88), &Params::imgScaleUnit,
             {Str::UnitPctNative, Str::UnitPctDocWidth});
    yi += RH;
    AddLabel(kPageType, kGImage, P, yi, Str::Rotation, lw);
    AddEdit(kPageType, kGImage, fx, yi, S(80), &Params::rotation, 1);
    yi += RH;
    AddCheck(kPageType, kGImage, fx, yi, S(170), Str::FlipH, &Params::flipH);
    AddCheck(kPageType, kGImage, fx + S(180), yi, S(170), Str::FlipV, &Params::flipV);
    yi += RH;
    AddCheck(kPageType, kGImage, P, yi, panelW_ - 2 * P, Str::Recolor, &Params::recolor);

    // --- kolor (wspolny)
    int yc = std::max(y, yi + RH) + S(6);
    AddLabel(kPageType, kGAlways, P, yc, Str::Color, lw);
    AddSwatch(kPageType, kGAlways, fx, yc, S(60), &Params::color);
    AddPush(kPageType, kGAlways, fx + S(70), yc, fw - S(70), S(26), IDC_USEFORE, Str::UseForeColor);
}

void Dialog::BuildPositionPage(int lw) {
    const int P = S(16), RH = S(32), fx = P + lw + S(8), fw = panelW_ - fx - P;
    int y = S(8);
    AddLabel(kPagePosition, kGAlways, P, y, Str::AnchorGrid, lw);
    const int cell = S(40), gap = S(4);
    for (int r = 0; r < 3; ++r)
        for (int c = 0; c < 3; ++c) {
            ctls_.emplace_back();
            Ctl& g = ctls_.back();
            g.id = nextId_++; g.kind = Kind::Grid; g.page = kPagePosition; g.group = kGAlways;
            g.ifield = &Params::anchor; g.radioValue = r * 3 + c;
            g.rx = fx + c * (cell + gap); g.ry = y + r * (cell + gap); g.rw = cell; g.rh = cell;
            NewWindow(g, L"BUTTON", L"", BS_OWNERDRAW | WS_TABSTOP);
        }
    y += 3 * (cell + gap) + S(10);
    AddLabel(kPagePosition, kGAlways, P, y, Str::MarginX, lw);
    AddEdit(kPagePosition, kGAlways, fx, y, S(80), &Params::marginX, 1);
    AddCombo(kPagePosition, kGAlways, fx + S(88), y, fw - S(88), &Params::marginUnit, {Str::UnitPx, Str::UnitPctShort});
    y += RH;
    AddLabel(kPagePosition, kGAlways, P, y, Str::MarginY, lw);
    AddEdit(kPagePosition, kGAlways, fx, y, S(80), &Params::marginY, 1);
    y += RH;
    AddLabel(kPagePosition, kGAlways, P, y, Str::OffsetX, lw);
    AddEdit(kPagePosition, kGAlways, fx, y, S(80), &Params::offsetX, 0);
    y += RH;
    AddLabel(kPagePosition, kGAlways, P, y, Str::OffsetY, lw);
    AddEdit(kPagePosition, kGAlways, fx, y, S(80), &Params::offsetY, 0);
    y += RH + S(6);
    AddCheck(kPagePosition, kGAlways, P, y, panelW_ - 2 * P, Str::RelSelection, &Params::relToSelection, 4);
}

void Dialog::BuildEffectsPage(int lw) {
    const int P = S(16), RH = S(30), fx = P + lw + S(8), fw = panelW_ - fx - P;
    int y = S(4);
    const int pg = kPageEffects;
    AddCheck(pg, kGAlways, P, y, panelW_ - 2 * P, Str::Stroke, &Params::strokeOn);
    y += RH;
    AddLabel(pg, kGAlways, P, y, Str::StrokeColor, lw).enableRule = 1;
    AddSwatch(pg, kGAlways, fx, y, S(60), &Params::strokeColor, 1);
    y += RH;
    AddLabel(pg, kGAlways, P, y, Str::StrokeWidth, lw).enableRule = 1;
    AddEdit(pg, kGAlways, fx, y, S(80), &Params::strokeWidth, 1, 1);
    y += RH;
    AddLabel(pg, kGAlways, P, y, Str::StrokePos, lw).enableRule = 1;
    AddCombo(pg, kGAlways, fx, y, fw, &Params::strokePos, {Str::PosOutside, Str::PosCenter, Str::PosInside}, 1);
    y += RH;
    AddLabel(pg, kGAlways, P, y, Str::Opacity, lw).enableRule = 1;
    AddEdit(pg, kGAlways, fx, y, S(80), &Params::strokeOpacity, 0, 1);
    y += RH + S(8);

    AddCheck(pg, kGAlways, P, y, panelW_ - 2 * P, Str::Emboss, &Params::embossOn);
    y += RH;
    AddLabel(pg, kGAlways, P, y, Str::EmbossStyle, lw).enableRule = 2;
    AddCombo(pg, kGAlways, fx, y, fw, &Params::embossStyle, {Str::StEmboss, Str::StPillow, Str::StOuter, Str::StInner}, 2);
    y += RH;
    AddLabel(pg, kGAlways, P, y, Str::Direction, lw).enableRule = 2;
    AddCombo(pg, kGAlways, fx, y, fw, &Params::embossDown, {Str::DirUp, Str::DirDown}, 2);
    y += RH;
    struct { Str k; double Params::*f; } rows[] = {{Str::Depth, &Params::depth},       {Str::EmbossSize, &Params::size},
                                                   {Str::Soften, &Params::soften},     {Str::Angle, &Params::angle},
                                                   {Str::Altitude, &Params::altitude}};
    for (auto& r : rows) {
        AddLabel(pg, kGAlways, P, y, r.k, lw).enableRule = 2;
        AddEdit(pg, kGAlways, fx, y, S(80), r.f, r.f == &Params::size || r.f == &Params::soften ? 1 : 0, 2);
        y += RH;
    }
    AddLabel(pg, kGAlways, P, y, Str::Highlight, lw).enableRule = 2;
    AddSwatch(pg, kGAlways, fx, y, S(60), &Params::hiColor, 2);
    AddEdit(pg, kGAlways, fx + S(70), y, S(70), &Params::hiOpacity, 0, 2);
    y += RH;
    AddLabel(pg, kGAlways, P, y, Str::Shadow, lw).enableRule = 2;
    AddSwatch(pg, kGAlways, fx, y, S(60), &Params::shColor, 2);
    AddEdit(pg, kGAlways, fx + S(70), y, S(70), &Params::shOpacity, 0, 2);
}

void Dialog::BuildVisibilityPage(int lw) {
    const int P = S(16), RH = S(34), pg = kPageVisibility;
    (void)lw;
    int y = S(8);
    AddLabel(pg, kGAlways, P, y, Str::Visibility, panelW_ - 2 * P);
    y += S(30);
    {
        ctls_.emplace_back();
        Ctl& c = ctls_.back();
        c.id = IDC_SLIDER; c.kind = Kind::Slider; c.page = pg;
        c.rx = P; c.ry = y; c.rw = panelW_ - 2 * P - S(90); c.rh = S(30);
        NewWindow(c, TRACKBAR_CLASSW, L"", TBS_HORZ | TBS_NOTICKS | WS_TABSTOP);
        SendMessageW(c.h, TBM_SETRANGE, TRUE, MAKELPARAM(0, 100));
        ApplyControlTheme(c.h, theme_.dark, L"Explorer");
    }
    AddEdit(pg, kGAlways, panelW_ - P - S(80), y, S(80), &Params::visibility, 0);
    y += RH + S(16);
    AddCheck(pg, kGAlways, P, y, panelW_ - 2 * P, Str::Direct, &Params::direct, 5);
    y += S(30);
    {
        Ctl& n = AddLabel(pg, kGAlways, P + S(24), y, Str::DirectNote, panelW_ - 2 * P - S(24));
        n.rh = S(100);
    }
    y += S(106);
    AddLabel(pg, kGAlways, P, y, Str::Blend, S(140)).enableRule = 3;
    AddCombo(pg, kGAlways, P + S(150), y, panelW_ - 2 * P - S(150), &Params::blend,
             {Str::BlNormal, Str::BlMultiply, Str::BlScreen, Str::BlOverlay, Str::BlSoftLight, Str::BlHardLight,
              Str::BlLighten, Str::BlDarken, Str::BlDifference},
             3);
}

void Dialog::BuildSettingsPage(int lw) {
    const int P = S(16), RH = S(34), pg = kPageSettings, fx = P + lw + S(8), fw = panelW_ - fx - P;
    int y = S(8);
    AddLabel(pg, kGAlways, P, y, Str::Language, lw);
    {
        ctls_.emplace_back();
        Ctl& c = ctls_.back();
        c.id = IDC_LANG; c.kind = Kind::Combo; c.page = pg;
        c.rx = fx; c.ry = y; c.rw = fw; c.rh = S(26) + S(26) * 3;
        NewWindow(c, L"COMBOBOX", L"", CBS_DROPDOWNLIST | CBS_OWNERDRAWFIXED | CBS_HASSTRINGS | WS_TABSTOP);
        ApplyControlTheme(c.h, theme_.dark, L"CFD");
        SendMessageW(c.h, CB_ADDSTRING, 0, (LPARAM)T(Str::LangAuto));
        SendMessageW(c.h, CB_ADDSTRING, 0, (LPARAM)L"Polski");
        SendMessageW(c.h, CB_ADDSTRING, 0, (LPARAM)L"English");
        SendMessageW(c.h, CB_SETITEMHEIGHT, (WPARAM)-1, S(20));
        SendMessageW(c.h, CB_SETITEMHEIGHT, 0, S(24));
    }
    y += RH;
    AddLabel(pg, kGAlways, P, y, Str::Theme, lw);
    {
        ctls_.emplace_back();
        Ctl& c = ctls_.back();
        c.id = IDC_THEME; c.kind = Kind::Combo; c.page = pg;
        c.rx = fx; c.ry = y; c.rw = fw; c.rh = S(26) + S(26) * 3;
        NewWindow(c, L"COMBOBOX", L"", CBS_DROPDOWNLIST | CBS_OWNERDRAWFIXED | CBS_HASSTRINGS | WS_TABSTOP);
        ApplyControlTheme(c.h, theme_.dark, L"CFD");
        SendMessageW(c.h, CB_ADDSTRING, 0, (LPARAM)T(Str::ThemeAuto));
        SendMessageW(c.h, CB_ADDSTRING, 0, (LPARAM)T(Str::ThemeLight));
        SendMessageW(c.h, CB_ADDSTRING, 0, (LPARAM)T(Str::ThemeDark));
        SendMessageW(c.h, CB_SETITEMHEIGHT, (WPARAM)-1, S(20));
        SendMessageW(c.h, CB_SETITEMHEIGHT, 0, S(24));
    }
    y += RH + S(14);
    Ctl& hdr = AddLabel(pg, kGAlways, P, y, Str::Presets, panelW_ - 2 * P);
    (void)hdr;
    y += S(32);
    {
        ctls_.emplace_back();
        Ctl& c = ctls_.back();
        c.id = IDC_PRESET_COMBO; c.kind = Kind::Combo; c.page = pg;
        c.rx = P; c.ry = y; c.rw = panelW_ - 2 * P - S(2 * 100 + 16); c.rh = S(26) + S(26) * 8;
        NewWindow(c, L"COMBOBOX", L"", CBS_DROPDOWNLIST | CBS_OWNERDRAWFIXED | CBS_HASSTRINGS | WS_TABSTOP | WS_VSCROLL);
        ApplyControlTheme(c.h, theme_.dark, L"CFD");
        SendMessageW(c.h, CB_SETITEMHEIGHT, (WPARAM)-1, S(20));
        SendMessageW(c.h, CB_SETITEMHEIGHT, 0, S(24));
    }
    AddPush(pg, kGAlways, panelW_ - P - S(208), y, S(100), S(26), IDC_PRESET_LOAD, Str::Load);
    AddPush(pg, kGAlways, panelW_ - P - S(100), y, S(100), S(26), IDC_PRESET_DEL, Str::Delete);
    y += RH + S(6);
    AddLabel(pg, kGAlways, P, y, Str::PresetName, lw);
    {
        ctls_.emplace_back();
        Ctl& c = ctls_.back();
        c.id = IDC_PRESET_NAME; c.kind = Kind::Edit; c.page = pg;
        c.rx = fx; c.ry = y; c.rw = fw - S(108); c.rh = S(26);
        NewWindow(c, L"EDIT", L"", ES_AUTOHSCROLL | WS_TABSTOP, WS_EX_CLIENTEDGE);
        ApplyControlTheme(c.h, theme_.dark, L"CFD");
        SendMessageW(c.h, EM_LIMITTEXT, 80, 0);
    }
    AddPush(pg, kGAlways, panelW_ - P - S(100), y, S(100), S(26), IDC_PRESET_SAVE, Str::Save);
}

void Dialog::Build() {
    // Szerokosc kolumny etykiet = najszersza etykieta (bez ucinania w obu jezykach).
    static const Str kLabels[] = {Str::Font, Str::Size, Str::Color, Str::Tracking, Str::LineSpacing, Str::ImageFile,
                                  Str::Scale, Str::Rotation, Str::AnchorGrid, Str::MarginX, Str::MarginY,
                                  Str::OffsetX, Str::OffsetY, Str::StrokeColor, Str::StrokeWidth, Str::StrokePos,
                                  Str::Opacity, Str::EmbossStyle, Str::Direction, Str::Depth, Str::EmbossSize,
                                  Str::Soften, Str::Angle, Str::Altitude, Str::Highlight, Str::Shadow,
                                  Str::Language, Str::Theme, Str::PresetName};
    HDC dc = GetDC(hwnd_);
    HGDIOBJ of = SelectObject(dc, font_);
    int lw = 0;
    for (Str k : kLabels) {
        SIZE sz;
        const wchar_t* t = T(k);
        GetTextExtentPoint32W(dc, t, (int)wcslen(t), &sz);
        lw = std::max(lw, (int)sz.cx);
    }
    // Pozostale napisy, ktore musza zmiescic sie w swoim polu.
    SelectObject(dc, of);
    ReleaseDC(hwnd_, dc);
    lw += S(6);

    nextId_ = IDC_DYNAMIC;
    ctls_.reserve(300);

    // Zakladki
    static const Str tabKeys[kPageCount] = {Str::TabType, Str::TabPosition, Str::TabEffects, Str::TabVisibility,
                                            Str::TabSettings};
    const int tw = panelW_ / kPageCount;
    for (int i = 0; i < kPageCount; ++i) {
        ctls_.emplace_back();
        Ctl& c = ctls_.back();
        c.id = IDC_TAB0 + i; c.kind = Kind::Tab; c.page = -1; c.textKey = tabKeys[i];
        c.rx = i * tw; c.ry = 0; c.rw = i == kPageCount - 1 ? panelW_ - i * tw : tw; c.rh = tabH_;
        NewWindow(c, L"BUTTON", T(tabKeys[i]), BS_OWNERDRAW | WS_TABSTOP);
    }
    // Przyciski dolne
    {
        const int bw = S(110), bh = S(32), gap = S(8), P = S(16);
        int x = panelW_ - P - bw;
        ctls_.emplace_back(); Ctl* c = &ctls_.back();
        c->id = IDC_CANCELB; c->kind = Kind::Push; c->page = -1; c->textKey = Str::Cancel;
        c->rx = x; c->ry = bh + S(10); c->rw = bw; c->rh = bh; c->bottomAnchored = true;
        NewWindow(*c, L"BUTTON", T(Str::Cancel), BS_OWNERDRAW | WS_TABSTOP);
        x -= bw + gap;
        ctls_.emplace_back(); c = &ctls_.back();
        c->id = IDC_OKB; c->kind = Kind::Push; c->page = -1; c->textKey = Str::Ok;
        c->rx = x; c->ry = bh + S(10); c->rw = bw; c->rh = bh; c->bottomAnchored = true;
        NewWindow(*c, L"BUTTON", T(Str::Ok), BS_OWNERDRAW | WS_TABSTOP);
        ctls_.emplace_back(); c = &ctls_.back();
        c->id = IDC_RESETB; c->kind = Kind::Push; c->page = -1; c->textKey = Str::Reset;
        c->rx = P; c->ry = bh + S(10); c->rw = bw; c->rh = bh; c->bottomAnchored = true;
        NewWindow(*c, L"BUTTON", T(Str::Reset), BS_OWNERDRAW | WS_TABSTOP);
    }

    BuildTypePage(lw);
    BuildPositionPage(lw);
    BuildEffectsPage(lw);
    BuildVisibilityPage(lw);
    BuildSettingsPage(lw);
    ReloadPresets();
}

void Dialog::DestroyControls() {
    for (auto& c : ctls_) if (c.h) DestroyWindow(c.h);
    ctls_.clear();
}

Ctl* Dialog::Find(int id) {
    for (auto& c : ctls_) if (c.id == id) return &c;
    return nullptr;
}

Ctl* Dialog::FindByHwnd(HWND h) {
    for (auto& c : ctls_) if (c.h == h) return &c;
    return nullptr;
}

// ----------------------------------------------------------------------------------------------
// Uklad i stan

void Dialog::Layout() {
    RECT rc;
    GetClientRect(hwnd_, &rc);
    const int W = rc.right, H = rc.bottom;
    const int px = W - panelW_;
    HDWP dwp = BeginDeferWindowPos((int)ctls_.size() + 1);
    for (auto& c : ctls_) {
        int y = c.bottomAnchored ? H - c.ry : c.ry + (c.page >= 0 ? tabH_ + S(6) : 0);
        dwp = DeferWindowPos(dwp, c.h, nullptr, px + c.rx, y, c.rw, c.rh, SWP_NOZORDER | SWP_NOACTIVATE);
    }
    if (preview_) dwp = DeferWindowPos(dwp, preview_, nullptr, 0, 0, std::max(px, 1), H, SWP_NOZORDER | SWP_NOACTIVATE);
    EndDeferWindowPos(dwp);
    InvalidateRect(hwnd_, nullptr, TRUE);
}

bool Dialog::VisibleFor(const Ctl& c) const {
    if (c.page < 0) return true;
    if (c.page != page_) return false;
    if (c.group == kGText) return p_.mode == kModeText;
    if (c.group == kGImage) return p_.mode == kModeImage;
    return true;
}

bool Dialog::EnabledFor(const Ctl& c) const {
    switch (c.enableRule) {
        case 1: return p_.strokeOn != 0;
        case 2: return p_.embossOn != 0;
        case 3: return p_.direct != 0;
        case 4: return ctx_.host && ctx_.host->hasSelection;
        case 5: return !ctx_.forceDirect;
        case 6: return FontHasStyle(p_.fontName, true, false);
        case 7: return FontHasStyle(p_.fontName, false, true);
        default: return true;
    }
}

void Dialog::UpdateStates() {
    for (auto& c : ctls_) {
        bool vis = VisibleFor(c);
        ShowWindow(c.h, vis ? SW_SHOWNA : SW_HIDE);
        bool en = EnabledFor(c);
        if (IsWindowEnabled(c.h) != (en ? TRUE : FALSE)) EnableWindow(c.h, en);
        if (vis) InvalidateRect(c.h, nullptr, TRUE);
    }
}

void Dialog::SwitchPage(int page) {
    page_ = page;
    UpdateStates();
}

void Dialog::SyncControls() {
    syncing_ = true;
    for (auto& c : ctls_) {
        switch (c.kind) {
            case Kind::Edit:
                if (c.dfield) SetWindowTextW(c.h, FormatNumber(p_.*c.dfield, c.decimals).c_str());
                else if (c.id == IDC_IMGPATH) SetWindowTextW(c.h, p_.imagePath);
                break;
            case Kind::MultiEdit: {
                // Photoshop/Windows: nowe linie w kontrolce to \r\n
                std::wstring t;
                for (const wchar_t* s = p_.text; *s; ++s) {
                    if (*s == L'\n' && (s == p_.text || s[-1] != L'\r')) t += L'\r';
                    t += *s;
                }
                SetWindowTextW(c.h, t.c_str());
                break;
            }
            case Kind::Combo:
                if (c.ifield) SendMessageW(c.h, CB_SETCURSEL, p_.*c.ifield, 0);
                else if (c.id == IDC_LANG) SendMessageW(c.h, CB_SETCURSEL, st_->language, 0);
                else if (c.id == IDC_THEME) SendMessageW(c.h, CB_SETCURSEL, st_->theme, 0);
                break;
            case Kind::Font: {
                int idx = (int)SendMessageW(c.h, CB_FINDSTRINGEXACT, (WPARAM)-1, (LPARAM)p_.fontName);
                if (idx == CB_ERR) {
                    SendMessageW(c.h, CB_INSERTSTRING, 0, (LPARAM)p_.fontName);
                    idx = 0;
                }
                SendMessageW(c.h, CB_SETCURSEL, idx, 0);
                break;
            }
            case Kind::Slider:
                SendMessageW(c.h, TBM_SETPOS, TRUE, (LPARAM)std::lround(p_.visibility));
                break;
            default: break;
        }
    }
    syncing_ = false;
    UpdateStates();
}

void Dialog::Changed() {
    if (!hwnd_) return;
    SetTimer(hwnd_, kPreviewTimer, 50, nullptr);
}

void Dialog::ReloadPresets() {
    presets_ = ListPresets();
    Ctl* c = Find(IDC_PRESET_COMBO);
    if (!c) return;
    SendMessageW(c->h, CB_RESETCONTENT, 0, 0);
    for (auto& n : presets_) SendMessageW(c->h, CB_ADDSTRING, 0, (LPARAM)n.c_str());
    if (!presets_.empty()) SendMessageW(c->h, CB_SETCURSEL, 0, 0);
}

// ----------------------------------------------------------------------------------------------
// Zdarzenia

void Dialog::OnEditChange(Ctl& c) {
    if (syncing_) return;
    wchar_t buf[600];
    GetWindowTextW(c.h, buf, 600);
    if (c.id == IDC_TEXT) {
        std::wstring t;
        for (const wchar_t* s = buf; *s; ++s)
            if (!(*s == L'\r' && s[1] == L'\n')) t += *s;
        wcsncpy_s(p_.text, t.c_str(), _TRUNCATE);
    } else if (c.id == IDC_IMGPATH) {
        wcsncpy_s(p_.imagePath, buf, _TRUNCATE);
    } else if (c.dfield) {
        double v;
        if (!ParseNumber(buf, v)) return;
        p_.*c.dfield = v;
        Sanitize(p_);
        if (c.dfield == &Params::visibility) {
            if (Ctl* s = Find(IDC_SLIDER)) SendMessageW(s->h, TBM_SETPOS, TRUE, (LPARAM)std::lround(p_.visibility));
        }
    } else {
        return;  // np. nazwa presetu
    }
    Changed();
}

void Dialog::OnEditKillFocus(Ctl& c) {
    if (!c.dfield) return;
    syncing_ = true;
    SetWindowTextW(c.h, FormatNumber(p_.*c.dfield, c.decimals).c_str());
    syncing_ = false;
}

void Dialog::PickColor(uint32_t Params::*field) {
    CHOOSECOLORW cc{};
    cc.lStructSize = sizeof(cc);
    cc.hwndOwner = hwnd_;
    cc.rgbResult = ToColorRef(p_.*field);
    cc.lpCustColors = g_custom;
    cc.Flags = CC_RGBINIT | CC_FULLOPEN;
    if (ChooseColorW(&cc)) {
        p_.*field = FromColorRef(cc.rgbResult);
        UpdateStates();
        Changed();
    }
}

void Dialog::BrowseImage() {
    wchar_t file[MAX_PATH] = {0};
    wcsncpy_s(file, p_.imagePath, _TRUNCATE);
    std::wstring filter = std::wstring(T(Str::FileFilterImages)) + L'\0' + L"*.png;*.jpg;*.jpeg;*.tif;*.tiff" + L'\0' +
                          T(Str::FileFilterAll) + L'\0' + L"*.*" + L'\0';
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd_;
    ofn.lpstrFilter = filter.c_str();
    ofn.lpstrFile = file;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrInitialDir = st_->lastImageDir.empty() ? nullptr : st_->lastImageDir.c_str();
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY | OFN_EXPLORER;
    if (GetOpenFileNameW(&ofn)) {
        wcsncpy_s(p_.imagePath, file, _TRUNCATE);
        std::wstring dir = file;
        size_t sl = dir.find_last_of(L'\\');
        if (sl != std::wstring::npos) st_->lastImageDir = dir.substr(0, sl);
        SyncControls();
        Changed();
    }
}

void Dialog::DoReset() {
    Params d;
    SetDefaults(d);
    d.color = ctx_.host ? ctx_.host->foreColor : d.color;
    if (ctx_.forceDirect) d.direct = 1;
    p_ = d;
    SyncControls();
    Changed();
}

void Dialog::PresetLoad() {
    Ctl* c = Find(IDC_PRESET_COMBO);
    int i = c ? (int)SendMessageW(c->h, CB_GETCURSEL, 0, 0) : -1;
    if (i < 0 || i >= (int)presets_.size()) { ShowMessage(hwnd_, T(Str::PresetNone), *st_); return; }
    Params q;
    if (LoadPreset(presets_[i], q)) {
        if (ctx_.forceDirect) q.direct = 1;
        p_ = q;
        SyncControls();
        Changed();
    }
}

void Dialog::PresetSave() {
    Ctl* n = Find(IDC_PRESET_NAME);
    wchar_t buf[200] = {0};
    if (n) GetWindowTextW(n->h, buf, 200);
    std::wstring name = SanitizePresetName(buf);
    if (name.empty()) { ShowMessage(hwnd_, T(Str::PresetBadName), *st_); return; }
    Params q = p_;
    Sanitize(q);
    if (SavePreset(name, q)) {
        ReloadPresets();
        if (Ctl* c = Find(IDC_PRESET_COMBO)) {
            int idx = (int)SendMessageW(c->h, CB_FINDSTRINGEXACT, (WPARAM)-1, (LPARAM)name.c_str());
            if (idx != CB_ERR) SendMessageW(c->h, CB_SETCURSEL, idx, 0);
        }
        ShowMessage(hwnd_, T(Str::PresetSaved), *st_);
    }
}

void Dialog::PresetDelete() {
    Ctl* c = Find(IDC_PRESET_COMBO);
    int i = c ? (int)SendMessageW(c->h, CB_GETCURSEL, 0, 0) : -1;
    if (i < 0 || i >= (int)presets_.size()) { ShowMessage(hwnd_, T(Str::PresetNone), *st_); return; }
    if (ShowChoice(hwnd_, T(Str::PresetConfirmDelete), T(Str::Delete), T(Str::Cancel), *st_) != 1) return;
    DeletePreset(presets_[i]);
    ReloadPresets();
}

bool Dialog::Validate() {
    Params q = p_;
    Sanitize(q);
    if (q.mode == kModeText) {
        bool blank = true;
        for (const wchar_t* s = q.text; *s; ++s)
            if (*s != L' ' && *s != L'\t' && *s != L'\r' && *s != L'\n') { blank = false; break; }
        if (blank) { ShowMessage(hwnd_, T(Str::MsgEmptyText), *st_); return false; }
    } else {
        if (!q.imagePath[0]) { ShowMessage(hwnd_, T(Str::MsgNoImage), *st_); return false; }
        FImage img;
        std::wstring err;
        if (!ctx_.ras->LoadImageFile(q.imagePath, img, err)) { ShowMessage(hwnd_, T(Str::MsgImageFail), *st_); return false; }
    }
    return true;
}

void Dialog::Finish(bool ok) {
    ok_ = ok;
    done_ = true;
}

void Dialog::OnClicked(Ctl& c) {
    switch (c.id) {
        case IDC_OKB:
            if (Validate()) { Sanitize(p_); Finish(true); }
            return;
        case IDC_CANCELB: Finish(false); return;
        case IDC_RESETB: DoReset(); return;
        case IDC_BROWSE: BrowseImage(); return;
        case IDC_USEFORE:
            if (ctx_.host) p_.color = ctx_.host->foreColor;
            p_.useForeColor = 1;
            InvalidateRect(hwnd_, nullptr, FALSE);
            UpdateStates();
            Changed();
            return;
        case IDC_PRESET_LOAD: PresetLoad(); return;
        case IDC_PRESET_SAVE: PresetSave(); return;
        case IDC_PRESET_DEL: PresetDelete(); return;
        default: break;
    }
    if (c.kind == Kind::Tab) { SwitchPage(c.id - IDC_TAB0); for (auto& t : ctls_) if (t.kind == Kind::Tab) InvalidateRect(t.h, nullptr, TRUE); return; }
    if (c.kind == Kind::Swatch && c.cfield) { PickColor(c.cfield); InvalidateRect(c.h, nullptr, TRUE); return; }
    if (c.kind == Kind::Check && c.ifield) {
        p_.*c.ifield = p_.*c.ifield ? 0 : 1;
    } else if ((c.kind == Kind::Radio || c.kind == Kind::Grid) && c.ifield) {
        p_.*c.ifield = c.radioValue;
    } else {
        return;
    }
    UpdateStates();
    Changed();
}

LRESULT Dialog::OnCommand(WPARAM w, LPARAM l) {
    int id = LOWORD(w), code = HIWORD(w);
    Ctl* c = FindByHwnd((HWND)l);
    if (!c) {
        if (id == IDOK) { if (Ctl* ok = Find(IDC_OKB)) OnClicked(*ok); }
        else if (id == IDCANCEL) Finish(false);
        return 0;
    }
    if (c->kind == Kind::Edit || c->kind == Kind::MultiEdit) {
        if (code == EN_CHANGE) OnEditChange(*c);
        else if (code == EN_KILLFOCUS) OnEditKillFocus(*c);
        return 0;
    }
    if (c->kind == Kind::Combo || c->kind == Kind::Font) {
        if (code != CBN_SELCHANGE) return 0;
        int sel = (int)SendMessageW(c->h, CB_GETCURSEL, 0, 0);
        if (sel < 0) return 0;
        if (c->kind == Kind::Font) {
            wchar_t name[64];
            SendMessageW(c->h, CB_GETLBTEXT, sel, (LPARAM)name);
            wcsncpy_s(p_.fontName, name, _TRUNCATE);
            UpdateStates();  // styl pogrubiony/kursywa zalezy od czcionki
            Changed();
        } else if (c->id == IDC_LANG) {
            st_->language = sel;
            SetLanguage(ResolveLanguage(st_->language));
            Rebuild();
        } else if (c->id == IDC_THEME) {
            st_->theme = sel;
            Rebuild();
        } else if (c->ifield) {
            p_.*c->ifield = sel;
            if (c->ifield == &Params::sizeUnit || c->ifield == &Params::imgScaleUnit) {
                // zmiana jednostki: ustaw sensowna wartosc domyslna
                if (c->ifield == &Params::sizeUnit) p_.sizeValue = sel == kUnitPx ? 120 : 5;
                else p_.imgScale = sel == kUnitPx ? 100 : 20;
                SyncControls();
            }
            UpdateStates();
            Changed();
        }
        return 0;
    }
    if (code == BN_CLICKED) OnClicked(*c);
    return 0;
}

LRESULT Dialog::OnMeasureItem(MEASUREITEMSTRUCT* m) {
    m->itemHeight = S(24);
    return TRUE;
}

LRESULT Dialog::OnDrawItem(const DRAWITEMSTRUCT* d) {
    if (d->CtlType == ODT_COMBOBOX) {
        HDC dc = d->hDC;
        RECT rc = d->rcItem;
        bool sel = (d->itemState & ODS_SELECTED) != 0 && !(d->itemState & ODS_COMBOBOXEDIT);
        bool disabled = (d->itemState & ODS_DISABLED) != 0;
        HBRUSH b = CreateSolidBrush(sel ? theme_.accent : theme_.edit);
        FillRect(dc, &rc, b);
        DeleteObject(b);
        if ((INT)d->itemID < 0) return TRUE;
        wchar_t text[128] = {0};
        SendMessageW(d->hwndItem, CB_GETLBTEXT, d->itemID, (LPARAM)text);
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, disabled ? theme_.disabled : (sel ? RGB(255, 255, 255) : theme_.text));
        HGDIOBJ old = nullptr;
        Ctl* c = FindByHwnd(d->hwndItem);
        if (c && c->kind == Kind::Font) {
            auto it = fontCache_.find(text);
            if (it == fontCache_.end()) {
                if (fontCache_.size() > 400) {
                    for (auto& kv : fontCache_) DeleteObject(kv.second);
                    fontCache_.clear();
                }
                HFONT f = CreateFontW(-S(16), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                                      OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH,
                                      text);
                it = fontCache_.emplace(text, f).first;
            }
            old = SelectObject(dc, it->second);
        } else {
            old = SelectObject(dc, font_);
        }
        RECT tr = rc;
        tr.left += S(6);
        DrawTextW(dc, text, -1, &tr, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);
        SelectObject(dc, old);
        return TRUE;
    }
    Ctl* c = FindByHwnd(d->hwndItem);
    if (!c) return FALSE;
    SelectObject(d->hDC, font_);
    switch (c->kind) {
        case Kind::Tab:
            DrawButton(d, theme_, ButtonKind::Tab, T(c->textKey), c->id - IDC_TAB0 == page_, 0, dpi_);
            break;
        case Kind::Check:
            DrawButton(d, theme_, ButtonKind::Check, T(c->textKey), p_.*c->ifield != 0, 0, dpi_);
            break;
        case Kind::Radio:
            DrawButton(d, theme_, ButtonKind::Radio, T(c->textKey), p_.*c->ifield == c->radioValue, 0, dpi_);
            break;
        case Kind::Grid:
            DrawButton(d, theme_, ButtonKind::Grid, L"", p_.*c->ifield == c->radioValue, 0, dpi_);
            break;
        case Kind::Swatch:
            DrawButton(d, theme_, ButtonKind::Swatch, L"", false, ToColorRef(p_.*c->cfield), dpi_);
            break;
        default:
            DrawButton(d, theme_, ButtonKind::Push, T(c->textKey), c->id == IDC_OKB, 0, dpi_);
            break;
    }
    return TRUE;
}

LRESULT Dialog::OnCtlColor(UINT msg, HDC dc, HWND h) {
    if (msg == WM_CTLCOLOREDIT || msg == WM_CTLCOLORLISTBOX) {
        SetTextColor(dc, IsWindowEnabled(h) ? theme_.text : theme_.disabled);
        SetBkColor(dc, theme_.edit);
        return (LRESULT)theme_.editBrush;
    }
    SetBkColor(dc, theme_.bg);
    SetTextColor(dc, IsWindowEnabled(h) ? theme_.text : theme_.disabled);
    return (LRESULT)theme_.bgBrush;
}

// ----------------------------------------------------------------------------------------------
// Podglad

void Dialog::PreparePreviewBase() {
    const HostInfo& hi = *ctx_.host;
    haveBase_ = ctx_.fr && ReadPreviewBase(ctx_.fr, hi, 1800, base_);
    if (!haveBase_) {
        // Brak danych z hosta: neutralna plansza o proporcjach obrazu.
        double s = std::min(1.0, 1400.0 / std::max(1, std::max(hi.imageW, hi.imageH)));
        base_.w = std::max(1, (int)std::lround(hi.imageW * s));
        base_.h = std::max(1, (int)std::lround(hi.imageH * s));
        base_.merged = false;
        base_.rgba.assign((size_t)base_.w * base_.h * 4, 0.0f);
        base_.scaleX = (double)base_.w / std::max(1, hi.imageW);
        base_.scaleY = (double)base_.h / std::max(1, hi.imageH);
    }
    pvW_ = base_.w;
    pvH_ = base_.h;

    BITMAPINFO bi{};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = pvW_;
    bi.bmiHeader.biHeight = -pvH_;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    if (pvBmp_) DeleteObject(pvBmp_);
    pvBmp_ = CreateDIBSection(nullptr, &bi, DIB_RGB_COLORS, (void**)&pvBits_, nullptr, 0);

    flatBase_.assign((size_t)pvW_ * pvH_ * 4, 255);
    for (int y = 0; y < pvH_; ++y)
        for (int x = 0; x < pvW_; ++x) {
            const float* s = &base_.rgba[((size_t)y * pvW_ + x) * 4];
            float chk = (((x / 8) + (y / 8)) & 1) ? 0.78f : 0.62f;
            uint8_t* o = &flatBase_[((size_t)y * pvW_ + x) * 4];
            float a = s[3];
            for (int c = 0; c < 3; ++c) {
                float v = s[c] * a + chk * (1.0f - a);
                o[2 - c] = (uint8_t)std::lround(std::min(std::max(v, 0.0f), 1.0f) * 255.0f);
            }
            o[3] = 255;
        }
}

void Dialog::RenderPreview() {
    if (!pvBits_ || renderBusy_) return;
    renderBusy_ = true;
    std::memcpy(pvBits_, flatBase_.data(), flatBase_.size());
    pvMessage_.clear();

    const HostInfo& hi = *ctx_.host;
    DocContext dc;
    dc.docW = pvW_;
    dc.docH = pvH_;
    dc.scale = base_.scaleX;
    if (p_.relToSelection && hi.hasSelection) {
        dc.ref.left = (int)std::lround(hi.filterRect.left * base_.scaleX);
        dc.ref.top = (int)std::lround(hi.filterRect.top * base_.scaleY);
        dc.ref.right = (int)std::lround(hi.filterRect.right * base_.scaleX);
        dc.ref.bottom = (int)std::lround(hi.filterRect.bottom * base_.scaleY);
    } else {
        dc.ref = IRect{0, 0, pvW_, pvH_};
    }
    Params q = p_;
    Sanitize(q);
    RenderResult rr = RenderSignature(q, dc, *ctx_.ras);
    if (rr.status != Status::Ok) {
        switch (rr.status) {
            case Status::EmptyText: pvMessage_ = T(Str::MsgEmptyText); break;
            case Status::ImageNotSet: pvMessage_ = T(Str::MsgNoImage); break;
            case Status::ImageLoadFailed: pvMessage_ = T(Str::MsgImageFail); break;
            case Status::TooLarge: pvMessage_ = T(Str::MsgTooLarge); break;
            case Status::NothingToDraw: pvMessage_ = T(Str::MsgNothing); break;
            default: pvMessage_ = T(Str::MsgRenderFail); break;
        }
    } else {
        const Rendered& s = rr.sig;
        const int mode = q.direct ? q.blend : (int)kBlendNormal;
        for (int y = std::max(s.y0, 0); y < std::min(s.y0 + s.img.h, pvH_); ++y) {
            for (int x = std::max(s.x0, 0); x < std::min(s.x0 + s.img.w, pvW_); ++x) {
                const float* sp = s.img.px(x - s.x0, y - s.y0);
                float sa = sp[3];
                if (sa <= 0.0f) continue;
                const float* bp = &base_.rgba[((size_t)y * pvW_ + x) * 4];
                float ba = base_.merged ? 1.0f : bp[3];
                float cs[3] = {std::min(sp[0] / sa, 1.0f), std::min(sp[1] / sa, 1.0f), std::min(sp[2] / sa, 1.0f)};
                float oc[3], oa;
                CompositePixel(mode, bp, ba, cs, sa, oc, oa);
                float chk = (((x / 8) + (y / 8)) & 1) ? 0.78f : 0.62f;
                uint8_t* o = pvBits_ + ((size_t)y * pvW_ + x) * 4;
                for (int c = 0; c < 3; ++c) {
                    float v = oc[c] * oa + chk * (1.0f - oa);
                    o[2 - c] = (uint8_t)std::lround(std::min(std::max(v, 0.0f), 1.0f) * 255.0f);
                }
            }
        }
    }
    renderBusy_ = false;
    if (preview_) InvalidateRect(preview_, nullptr, FALSE);
}

void Dialog::PaintPreview(HWND h) {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(h, &ps);
    RECT rc;
    GetClientRect(h, &rc);
    HDC mem = CreateCompatibleDC(hdc);
    HBITMAP buf = CreateCompatibleBitmap(hdc, rc.right, rc.bottom);
    HGDIOBJ ob = SelectObject(mem, buf);
    HBRUSH bg = CreateSolidBrush(theme_.dark ? RGB(0x14, 0x14, 0x14) : RGB(0xdd, 0xdd, 0xdd));
    FillRect(mem, &rc, bg);
    DeleteObject(bg);
    if (pvBmp_ && pvW_ > 0 && pvH_ > 0) {
        double fit = std::min((double)(rc.right - 24) / pvW_, (double)(rc.bottom - 24) / pvH_);
        if (fit <= 0) fit = 0.01;
        double sc = fit * zoom_;
        int dw = std::max(1, (int)std::lround(pvW_ * sc)), dh = std::max(1, (int)std::lround(pvH_ * sc));
        int dx = (int)std::lround((rc.right - dw) / 2.0 + panX_), dy = (int)std::lround((rc.bottom - dh) / 2.0 + panY_);
        HDC src = CreateCompatibleDC(hdc);
        HGDIOBJ os = SelectObject(src, pvBmp_);
        SetStretchBltMode(mem, HALFTONE);
        SetBrushOrgEx(mem, 0, 0, nullptr);
        StretchBlt(mem, dx, dy, dw, dh, src, 0, 0, pvW_, pvH_, SRCCOPY);
        SelectObject(src, os);
        DeleteDC(src);
    }
    // Komunikaty na dole
    std::wstring note = pvMessage_;
    if (note.empty() && haveBase_) note = base_.merged ? T(Str::PreviewMerged) : T(Str::PreviewLayerOnly);
    else if (note.empty()) note = T(Str::PreviewLayerOnly);
    if (!note.empty()) {
        SelectObject(mem, font_);
        SetBkMode(mem, TRANSPARENT);
        RECT tr = {S(12), rc.bottom - S(36), rc.right - S(12), rc.bottom - S(8)};
        SetTextColor(mem, pvMessage_.empty() ? (theme_.dark ? RGB(0xa0, 0xa0, 0xa0) : RGB(0x55, 0x55, 0x55))
                                              : RGB(0xff, 0x80, 0x60));
        DrawTextW(mem, note.c_str(), -1, &tr, DT_LEFT | DT_BOTTOM | DT_WORDBREAK | DT_NOPREFIX);
    }
    BitBlt(hdc, 0, 0, rc.right, rc.bottom, mem, 0, 0, SRCCOPY);
    SelectObject(mem, ob);
    DeleteObject(buf);
    DeleteDC(mem);
    EndPaint(h, &ps);
}

LRESULT Dialog::PreviewProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
        case WM_PAINT: PaintPreview(h); return 0;
        case WM_ERASEBKGND: return 1;
        case WM_MOUSEWHEEL: {
            int d = GET_WHEEL_DELTA_WPARAM(w);
            zoom_ = std::min(8.0, std::max(0.25, zoom_ * std::pow(1.15, d / 120.0)));
            if (zoom_ < 1.001 && zoom_ > 0.999) { zoom_ = 1; panX_ = panY_ = 0; }
            InvalidateRect(h, nullptr, FALSE);
            return 0;
        }
        case WM_LBUTTONDOWN:
            dragging_ = true;
            dragStart_ = {GET_X_LPARAM(l), GET_Y_LPARAM(l)};
            dragPanX_ = panX_; dragPanY_ = panY_;
            SetCapture(h);
            return 0;
        case WM_MOUSEMOVE:
            if (dragging_) {
                panX_ = dragPanX_ + (GET_X_LPARAM(l) - dragStart_.x);
                panY_ = dragPanY_ + (GET_Y_LPARAM(l) - dragStart_.y);
                InvalidateRect(h, nullptr, FALSE);
            }
            return 0;
        case WM_LBUTTONUP:
            dragging_ = false;
            ReleaseCapture();
            return 0;
        case WM_LBUTTONDBLCLK:
            zoom_ = 1; panX_ = panY_ = 0;
            InvalidateRect(h, nullptr, FALSE);
            return 0;
        case WM_SETCURSOR:
            SetCursor(LoadCursor(nullptr, dragging_ ? IDC_SIZEALL : IDC_HAND));
            return TRUE;
    }
    return DefWindowProcW(h, m, w, l);
}

LRESULT CALLBACK Dialog::PreviewProcThunk(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_NCCREATE) SetWindowLongPtrW(h, GWLP_USERDATA, (LONG_PTR)((CREATESTRUCTW*)l)->lpCreateParams);
    Dialog* d = (Dialog*)GetWindowLongPtrW(h, GWLP_USERDATA);
    return d ? d->PreviewProc(h, m, w, l) : DefWindowProcW(h, m, w, l);
}

// ----------------------------------------------------------------------------------------------
// Okno glowne

LRESULT CALLBACK Dialog::MainProcThunk(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_NCCREATE) SetWindowLongPtrW(h, GWLP_USERDATA, (LONG_PTR)((CREATESTRUCTW*)l)->lpCreateParams);
    Dialog* d = (Dialog*)GetWindowLongPtrW(h, GWLP_USERDATA);
    if (!d) return DefWindowProcW(h, m, w, l);
    return d->MainProc(h, m, w, l);
}

LRESULT Dialog::MainProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
        case WM_COMMAND: return OnCommand(w, l);
        case WM_DRAWITEM: return OnDrawItem((const DRAWITEMSTRUCT*)l);
        case WM_MEASUREITEM: return OnMeasureItem((MEASUREITEMSTRUCT*)l);
        case WM_CTLCOLOREDIT:
        case WM_CTLCOLORLISTBOX:
        case WM_CTLCOLORSTATIC:
        case WM_CTLCOLORBTN:
            return OnCtlColor(m, (HDC)w, (HWND)l);
        case WM_ERASEBKGND: {
            RECT rc;
            GetClientRect(h, &rc);
            FillRect((HDC)w, &rc, theme_.bgBrush);
            return 1;
        }
        case WM_HSCROLL:
            if ((HWND)l == (Find(IDC_SLIDER) ? Find(IDC_SLIDER)->h : nullptr)) {
                p_.visibility = (double)SendMessageW((HWND)l, TBM_GETPOS, 0, 0);
                for (auto& c : ctls_)
                    if (c.dfield == &Params::visibility) {
                        syncing_ = true;
                        SetWindowTextW(c.h, FormatNumber(p_.visibility, 0).c_str());
                        syncing_ = false;
                    }
                Changed();
            }
            return 0;
        case WM_TIMER:
            if (w == kPreviewTimer) {
                KillTimer(h, kPreviewTimer);
                RenderPreview();
            }
            return 0;
        case WM_SIZE:
            if (w != SIZE_MINIMIZED) Layout();
            return 0;
        case WM_GETMINMAXINFO: {
            MINMAXINFO* mm = (MINMAXINFO*)l;
            mm->ptMinTrackSize.x = S(900);
            mm->ptMinTrackSize.y = S(720);
            return 0;
        }
        case WM_DPICHANGED: {
            dpi_ = HIWORD(w);
            const RECT* r = (const RECT*)l;
            SetWindowPos(h, nullptr, r->left, r->top, r->right - r->left, r->bottom - r->top, SWP_NOZORDER | SWP_NOACTIVATE);
            Rebuild();
            return 0;
        }
        case WM_APP + 1:
            if (ctx_.onShown) ctx_.onShown(h, ctx_.onShownUser);
            return 0;
        case WM_CLOSE: Finish(false); return 0;
    }
    return DefWindowProcW(h, m, w, l);
}

void Dialog::RegisterClasses() {
    static bool done = false;
    if (done) return;
    HINSTANCE inst = ModuleInstance();
    WNDCLASSW wc{};
    wc.lpfnWndProc = MainProcThunk;
    wc.hInstance = inst;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = kMainClass;
    RegisterClassW(&wc);
    WNDCLASSW pc{};
    pc.style = CS_DBLCLKS;
    pc.lpfnWndProc = PreviewProcThunk;
    pc.hInstance = inst;
    pc.hCursor = LoadCursor(nullptr, IDC_HAND);
    pc.lpszClassName = kPreviewClass;
    RegisterClassW(&pc);
    done = true;
}

void Dialog::Rebuild() {
    // Przebudowa po zmianie jezyka/motywu/DPI; parametry (p_) zostaja.
    SetWindowRedraw(hwnd_, FALSE);
    DestroyControls();
    MakeTheme(theme_, ResolveDark(st_->theme));
    ApplyWindowTheme(hwnd_, theme_.dark);
    if (font_) DeleteObject(font_);
    if (fontBold_) DeleteObject(fontBold_);
    NONCLIENTMETRICSW ncm{sizeof(ncm)};
    SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0);
    ncm.lfMessageFont.lfHeight = -S(15);
    wcscpy_s(ncm.lfMessageFont.lfFaceName, L"Segoe UI");
    font_ = CreateFontIndirectW(&ncm.lfMessageFont);
    panelW_ = S(520);
    tabH_ = S(38);
    bottomH_ = S(60);
    SetWindowTextW(hwnd_, T(Str::Title));
    Build();
    Layout();
    SyncControls();
    for (auto& c : ctls_) if (c.kind == Kind::Tab) InvalidateRect(c.h, nullptr, TRUE);
    SetWindowRedraw(hwnd_, TRUE);
    RedrawWindow(hwnd_, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_UPDATENOW);
    if (preview_) InvalidateRect(preview_, nullptr, FALSE);
}

bool Dialog::Run() {
    st_ = ctx_.settings;
    RegisterClasses();
    INITCOMMONCONTROLSEX icc{sizeof(icc), ICC_BAR_CLASSES};
    InitCommonControlsEx(&icc);

    SetLanguage(ResolveLanguage(st_->language));
    MakeTheme(theme_, ResolveDark(st_->theme));
    dpi_ = (int)DpiFor(ctx_.parent);
    fonts_ = ListInstalledFonts();
    if (ctx_.forceDirect) p_.direct = 1;
    Sanitize(p_);

    // Rozmiar okna: ~85% obszaru roboczego, w granicach rozsadku.
    RECT wa;
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &wa, 0);
    int ww = std::min(S(1300), (int)((wa.right - wa.left) * 0.92));
    int wh = std::min(S(800), (int)((wa.bottom - wa.top) * 0.92));
    ww = std::max(ww, S(900));
    wh = std::max(wh, S(720));
    int x = wa.left + ((wa.right - wa.left) - ww) / 2, y = wa.top + ((wa.bottom - wa.top) - wh) / 2;

    hwnd_ = CreateWindowExW(0, kMainClass, T(Str::Title), WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME |
                            WS_MAXIMIZEBOX | WS_CLIPCHILDREN, x, y, ww, wh, ctx_.parent, nullptr, ModuleInstance(), this);
    if (!hwnd_) { DebugLog(L"CreateWindow(main) failed: %lu", GetLastError()); return false; }
    ApplyWindowTheme(hwnd_, theme_.dark);

    preview_ = CreateWindowExW(0, kPreviewClass, L"", WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS, 0, 0, 100, 100, hwnd_,
                               nullptr, ModuleInstance(), this);

    NONCLIENTMETRICSW ncm{sizeof(ncm)};
    SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0);
    ncm.lfMessageFont.lfHeight = -S(15);
    wcscpy_s(ncm.lfMessageFont.lfFaceName, L"Segoe UI");
    font_ = CreateFontIndirectW(&ncm.lfMessageFont);
    panelW_ = S(520);
    tabH_ = S(38);
    bottomH_ = S(60);

    DebugLog(L"dlg: created, building");
    PreparePreviewBase();
    Build();
    DebugLog(L"dlg: built %d controls", (int)ctls_.size());
    Layout();
    SyncControls();

    if (ctx_.parent) EnableWindow(ctx_.parent, FALSE);
    ShowWindow(hwnd_, SW_SHOW);
    UpdateWindow(hwnd_);
    RenderPreview();
    SetForegroundWindow(hwnd_);
    if (ctx_.onShown) PostMessageW(hwnd_, WM_APP + 1, 0, 0);

    MSG msg;
    while (!done_ && GetMessageW(&msg, nullptr, 0, 0) > 0) {
        if (!IsDialogMessageW(hwnd_, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }
    DebugLog(L"dlg: loop ended ok=%d done=%d", (int)ok_, (int)done_);
    if (ctx_.parent) EnableWindow(ctx_.parent, TRUE);
    if (ok_) result_ = p_;
    DestroyWindow(hwnd_);
    if (ctx_.parent) SetForegroundWindow(ctx_.parent);

    for (auto& kv : fontCache_) DeleteObject(kv.second);
    if (font_) DeleteObject(font_);
    if (pvBmp_) DeleteObject(pvBmp_);
    FreeTheme(theme_);
    return ok_;
}

}  // namespace

bool ShowDialog(DialogContext& ctx, Params& p) {
    Dialog d(ctx, p);
    return d.Run();
}

}  // namespace ui
}  // namespace wm
