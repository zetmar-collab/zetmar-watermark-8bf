#include "ui/theme.h"

#include <dwmapi.h>
#include <uxtheme.h>

extern "C" IMAGE_DOS_HEADER __ImageBase;

namespace wm {
namespace ui {

HINSTANCE ModuleInstance() { return (HINSTANCE)&__ImageBase; }

bool ResolveDark(int themeSetting) {
    if (themeSetting == 1) return false;
    if (themeSetting == 2) return true;
    DWORD v = 1, sz = sizeof(v);
    if (RegGetValueW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                     L"AppsUseLightTheme", RRF_RT_REG_DWORD, nullptr, &v, &sz) != ERROR_SUCCESS)
        return false;
    return v == 0;
}

void MakeTheme(Theme& t, bool dark) {
    FreeTheme(t);
    t.dark = dark;
    if (dark) {
        t.bg = RGB(0x20, 0x20, 0x20);
        t.panel = RGB(0x2b, 0x2b, 0x2b);
        t.text = RGB(0xe8, 0xe8, 0xe8);
        t.textDim = RGB(0xa0, 0xa0, 0xa0);
        t.edit = RGB(0x33, 0x33, 0x33);
        t.border = RGB(0x58, 0x58, 0x58);
        t.accent = RGB(0x4c, 0xa0, 0xff);
        t.button = RGB(0x3a, 0x3a, 0x3a);
        t.buttonHot = RGB(0x4a, 0x4a, 0x4a);
        t.buttonDown = RGB(0x2e, 0x2e, 0x2e);
        t.disabled = RGB(0x6e, 0x6e, 0x6e);
    } else {
        t.bg = RGB(0xf3, 0xf3, 0xf3);
        t.panel = RGB(0xff, 0xff, 0xff);
        t.text = RGB(0x1b, 0x1b, 0x1b);
        t.textDim = RGB(0x60, 0x60, 0x60);
        t.edit = RGB(0xff, 0xff, 0xff);
        t.border = RGB(0xad, 0xad, 0xad);
        t.accent = RGB(0x0b, 0x6b, 0xcb);
        t.button = RGB(0xe6, 0xe6, 0xe6);
        t.buttonHot = RGB(0xd8, 0xd8, 0xd8);
        t.buttonDown = RGB(0xc8, 0xc8, 0xc8);
        t.disabled = RGB(0x9a, 0x9a, 0x9a);
    }
    t.bgBrush = CreateSolidBrush(t.bg);
    t.editBrush = CreateSolidBrush(t.edit);
    t.panelBrush = CreateSolidBrush(t.panel);
}

void FreeTheme(Theme& t) {
    if (t.bgBrush) DeleteObject(t.bgBrush);
    if (t.editBrush) DeleteObject(t.editBrush);
    if (t.panelBrush) DeleteObject(t.panelBrush);
    t.bgBrush = t.editBrush = t.panelBrush = nullptr;
}

void ApplyWindowTheme(HWND hwnd, bool dark) {
    BOOL d = dark ? TRUE : FALSE;
    // 20 = DWMWA_USE_IMMERSIVE_DARK_MODE (19 w starszych kompilacjach Windows 10)
    if (FAILED(DwmSetWindowAttribute(hwnd, 20, &d, sizeof(d)))) DwmSetWindowAttribute(hwnd, 19, &d, sizeof(d));
}

void ApplyControlTheme(HWND ctl, bool dark, const wchar_t* kind) {
    if (!dark) { SetWindowTheme(ctl, nullptr, nullptr); return; }
    wchar_t name[40];
    wsprintfW(name, L"DarkMode_%s", kind);
    SetWindowTheme(ctl, name, nullptr);
}

static void FillRoundRect(HDC dc, const RECT& r, COLORREF fill, COLORREF border, int radius) {
    HBRUSH b = CreateSolidBrush(fill);
    HPEN p = CreatePen(PS_SOLID, 1, border);
    HGDIOBJ ob = SelectObject(dc, b), op = SelectObject(dc, p);
    RoundRect(dc, r.left, r.top, r.right, r.bottom, radius, radius);
    SelectObject(dc, ob);
    SelectObject(dc, op);
    DeleteObject(b);
    DeleteObject(p);
}

void DrawButton(const DRAWITEMSTRUCT* dis, const Theme& t, ButtonKind kind, const wchar_t* text, bool checked,
                COLORREF swatch, int dpi) {
    HDC dc = dis->hDC;
    RECT rc = dis->rcItem;
    const bool disabled = (dis->itemState & ODS_DISABLED) != 0;
    const bool down = (dis->itemState & ODS_SELECTED) != 0;
    const bool focus = (dis->itemState & ODS_FOCUS) != 0;
    POINT pt;
    GetCursorPos(&pt);
    ScreenToClient(dis->hwndItem, &pt);
    const bool hot = PtInRect(&rc, pt) != 0;
    const COLORREF txt = disabled ? t.disabled : t.text;

    SetBkMode(dc, TRANSPARENT);
    if (kind == ButtonKind::Check || kind == ButtonKind::Radio) {
        FillRect(dc, &rc, t.bgBrush);
        int box = Scale(16, dpi);
        RECT b = {rc.left + 1, (rc.top + rc.bottom - box) / 2, rc.left + 1 + box, (rc.top + rc.bottom + box) / 2};
        COLORREF edge = disabled ? t.disabled : (checked ? t.accent : t.border);
        if (kind == ButtonKind::Check) {
            FillRoundRect(dc, b, checked && !disabled ? t.accent : t.edit, edge, Scale(4, dpi));
            if (checked) {
                HPEN pen = CreatePen(PS_SOLID, Scale(2, dpi), disabled ? t.disabled : RGB(255, 255, 255));
                HGDIOBJ op = SelectObject(dc, pen);
                int w = box;
                MoveToEx(dc, b.left + w * 22 / 100, b.top + w * 52 / 100, nullptr);
                LineTo(dc, b.left + w * 42 / 100, b.top + w * 72 / 100);
                LineTo(dc, b.left + w * 78 / 100, b.top + w * 28 / 100);
                SelectObject(dc, op);
                DeleteObject(pen);
            }
        } else {
            FillRoundRect(dc, b, t.edit, edge, box);
            if (checked) {
                int in = Scale(4, dpi);
                RECT d = {b.left + in, b.top + in, b.right - in, b.bottom - in};
                FillRoundRect(dc, d, disabled ? t.disabled : t.accent, disabled ? t.disabled : t.accent, box);
            }
        }
        RECT tr = {b.right + Scale(8, dpi), rc.top, rc.right, rc.bottom};
        SetTextColor(dc, txt);
        DrawTextW(dc, text, -1, &tr, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        if (focus) {
            RECT f = {tr.left - 2, rc.top + 1, rc.right, rc.bottom - 1};
            DrawFocusRect(dc, &f);
        }
        return;
    }

    if (kind == ButtonKind::Tab) {
        FillRect(dc, &rc, t.bgBrush);
        SetTextColor(dc, checked ? t.text : t.textDim);
        DrawTextW(dc, text, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        if (checked) {
            RECT u = {rc.left + Scale(6, dpi), rc.bottom - Scale(3, dpi), rc.right - Scale(6, dpi), rc.bottom};
            HBRUSH b = CreateSolidBrush(t.accent);
            FillRect(dc, &u, b);
            DeleteObject(b);
        } else if (hot) {
            RECT u = {rc.left + Scale(6, dpi), rc.bottom - Scale(2, dpi), rc.right - Scale(6, dpi), rc.bottom};
            HBRUSH b = CreateSolidBrush(t.border);
            FillRect(dc, &u, b);
            DeleteObject(b);
        }
        return;
    }

    COLORREF fill = down ? t.buttonDown : (hot && !disabled ? t.buttonHot : t.button);
    COLORREF edge = checked ? t.accent : t.border;
    if (kind == ButtonKind::Grid) fill = checked ? t.accent : fill;
    FillRect(dc, &rc, t.bgBrush);
    RECT r2 = rc;
    InflateRect(&r2, -1, -1);
    if (kind == ButtonKind::Swatch) {
        FillRoundRect(dc, r2, t.edit, edge, Scale(4, dpi));
        RECT in = r2;
        InflateRect(&in, -Scale(3, dpi), -Scale(3, dpi));
        HBRUSH b = CreateSolidBrush(swatch);
        FillRect(dc, &in, b);
        DeleteObject(b);
    } else {
        FillRoundRect(dc, r2, fill, edge, Scale(5, dpi));
        if (kind == ButtonKind::Grid) {
            if (checked) {
                int d = Scale(10, dpi);
                RECT dot = {(rc.left + rc.right - d) / 2, (rc.top + rc.bottom - d) / 2, (rc.left + rc.right + d) / 2,
                            (rc.top + rc.bottom + d) / 2};
                FillRoundRect(dc, dot, RGB(255, 255, 255), RGB(255, 255, 255), d);
            }
        } else {
            SetTextColor(dc, txt);
            RECT tr = rc;
            InflateRect(&tr, -Scale(6, dpi), 0);
            DrawTextW(dc, text, -1, &tr, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        }
    }
    if (focus) {
        RECT f = rc;
        InflateRect(&f, -Scale(3, dpi), -Scale(3, dpi));
        DrawFocusRect(dc, &f);
    }
}

}  // namespace ui
}  // namespace wm
