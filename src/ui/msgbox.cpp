// Okna komunikatow w motywie okna, z wlasnymi napisami przyciskow.
#include <windows.h>

#include <string>

#include "core/i18n.h"
#include "ui/dialog.h"
#include "ui/theme.h"

namespace wm {
namespace ui {

namespace {

const wchar_t kClass[] = L"ZetmarWmMessage";

struct State {
    Theme theme;
    std::wstring text, b1, b2;
    int dpi = 96;
    HFONT font = nullptr;
    int result = 2;
    bool done = false;
    RECT textRect{};
};

LRESULT CALLBACK MsgProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    State* s = (State*)GetWindowLongPtrW(h, GWLP_USERDATA);
    switch (m) {
        case WM_NCCREATE:
            SetWindowLongPtrW(h, GWLP_USERDATA, (LONG_PTR)((CREATESTRUCTW*)l)->lpCreateParams);
            break;
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC dc = BeginPaint(h, &ps);
            RECT rc;
            GetClientRect(h, &rc);
            FillRect(dc, &rc, s->theme.bgBrush);
            SelectObject(dc, s->font);
            SetBkMode(dc, TRANSPARENT);
            SetTextColor(dc, s->theme.text);
            DrawTextW(dc, s->text.c_str(), -1, &s->textRect, DT_WORDBREAK | DT_NOPREFIX);
            EndPaint(h, &ps);
            return 0;
        }
        case WM_ERASEBKGND: return 1;
        case WM_DRAWITEM: {
            const DRAWITEMSTRUCT* d = (const DRAWITEMSTRUCT*)l;
            SelectObject(d->hDC, s->font);
            DrawButton(d, s->theme, ButtonKind::Push, d->CtlID == 1 ? s->b1.c_str() : s->b2.c_str(), d->CtlID == 1,
                       0, s->dpi);
            return TRUE;
        }
        case WM_COMMAND:
            if (LOWORD(w) == 1 || LOWORD(w) == 2) { s->result = LOWORD(w); s->done = true; return 0; }
            if (LOWORD(w) == IDCANCEL) { s->result = 2; s->done = true; return 0; }
            if (LOWORD(w) == IDOK) { s->result = 1; s->done = true; return 0; }
            break;
        case WM_CLOSE: s->result = 2; s->done = true; return 0;
    }
    return DefWindowProcW(h, m, w, l);
}

int Run(HWND parent, const wchar_t* text, const wchar_t* b1, const wchar_t* b2, const AppSettings& as) {
    static bool registered = false;
    HINSTANCE inst = ModuleInstance();
    if (!registered) {
        WNDCLASSW wc{};
        wc.lpfnWndProc = MsgProc;
        wc.hInstance = inst;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.lpszClassName = kClass;
        RegisterClassW(&wc);
        registered = true;
    }
    State st;
    MakeTheme(st.theme, ResolveDark(as.theme));
    st.text = text;
    st.b1 = b1;
    st.b2 = b2 ? b2 : L"";
    HDC sdc = GetDC(nullptr);
    st.dpi = GetDeviceCaps(sdc, LOGPIXELSX);
    ReleaseDC(nullptr, sdc);
    if (parent) {
        typedef UINT(WINAPI * GetDpiForWindowFn)(HWND);
        auto fn = (GetDpiForWindowFn)GetProcAddress(GetModuleHandleW(L"user32.dll"), "GetDpiForWindow");
        if (fn) { UINT d = fn(parent); if (d) st.dpi = (int)d; }
    }
    NONCLIENTMETRICSW ncm{sizeof(ncm)};
    SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0);
    ncm.lfMessageFont.lfHeight = -Scale(15, st.dpi);
    wcscpy_s(ncm.lfMessageFont.lfFaceName, L"Segoe UI");
    st.font = CreateFontIndirectW(&ncm.lfMessageFont);

    const int pad = Scale(20, st.dpi), clientW = Scale(480, st.dpi);
    HDC dc = GetDC(nullptr);
    HGDIOBJ of = SelectObject(dc, st.font);
    RECT tr = {0, 0, clientW - 2 * pad, 0};
    DrawTextW(dc, text, -1, &tr, DT_CALCRECT | DT_WORDBREAK | DT_NOPREFIX);
    SelectObject(dc, of);
    ReleaseDC(nullptr, dc);
    const int btnH = Scale(32, st.dpi), btnW = Scale(190, st.dpi), gap = Scale(10, st.dpi);
    const int clientH = pad + (tr.bottom - tr.top) + pad + btnH + pad;
    st.textRect = {pad, pad, clientW - pad, pad + (tr.bottom - tr.top)};

    DWORD style = WS_POPUP | WS_CAPTION | WS_SYSMENU;
    RECT wr = {0, 0, clientW, clientH};
    AdjustWindowRectEx(&wr, style, FALSE, WS_EX_DLGMODALFRAME);
    int ww = wr.right - wr.left, wh = wr.bottom - wr.top;
    RECT pr;
    if (parent && GetWindowRect(parent, &pr)) {}
    else { pr = {0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN)}; }
    int x = (pr.left + pr.right - ww) / 2, y = (pr.top + pr.bottom - wh) / 2;

    HWND h = CreateWindowExW(WS_EX_DLGMODALFRAME, kClass, T(Str::Title), style, x, y, ww, wh, parent,
                             nullptr, inst, &st);
    if (!h) { FreeTheme(st.theme); DeleteObject(st.font); return 2; }
    ApplyWindowTheme(h, st.theme.dark);
    int nb = st.b2.empty() ? 1 : 2;
    int totalW = nb * btnW + (nb - 1) * gap;
    int bx = (clientW - totalW) / 2, by = clientH - pad - btnH;
    for (int i = 0; i < nb; ++i)
        CreateWindowW(L"BUTTON", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW, bx + i * (btnW + gap), by,
                      btnW, btnH, h, (HMENU)(INT_PTR)(i + 1), inst, nullptr);

    if (parent) EnableWindow(parent, FALSE);
    ShowWindow(h, SW_SHOW);
    SetForegroundWindow(h);
    MSG msg;
    BOOL got = 1;
    while (!st.done && (got = GetMessageW(&msg, nullptr, 0, 0)) > 0) {
        if (!IsDialogMessageW(h, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }
    if (!st.done && got == 0) PostQuitMessage((int)msg.wParam);
    if (parent) EnableWindow(parent, TRUE);
    DestroyWindow(h);
    if (parent) SetForegroundWindow(parent);
    FreeTheme(st.theme);
    DeleteObject(st.font);
    return st.result;
}

}  // namespace

void ShowMessage(HWND parent, const wchar_t* text, const AppSettings& s) { Run(parent, text, T(Str::Ok), nullptr, s); }

int ShowChoice(HWND parent, const wchar_t* text, const wchar_t* b1, const wchar_t* b2, const AppSettings& s) {
    return Run(parent, text, b1, b2, s);
}

}  // namespace ui
}  // namespace wm
