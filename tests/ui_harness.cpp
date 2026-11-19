// Uruchamia okno wtyczki bez Photoshopa i zapisuje zrzuty kazdej zakladki (PNG).
// Uzycie: wm_ui_harness.exe katalog_wyjsciowy [pl|en] [light|dark]
#include <windows.h>

#include <wincodec.h>

#include <cstdio>
#include <string>
#include <vector>

#include "core/i18n.h"
#include "platform/rasterizer_win.h"
#include "ui/dialog.h"

using namespace wm;

static std::wstring g_dir, g_tag;

static void SaveWindowPng(HWND h, const std::wstring& path) {
    wprintf(L"saving %s\n", path.c_str());
    RECT rc;
    GetWindowRect(h, &rc);
    int w = rc.right - rc.left, hgt = rc.bottom - rc.top;
    HDC sdc = GetDC(nullptr);
    HDC mdc = CreateCompatibleDC(sdc);
    BITMAPINFO bi{};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = w;
    bi.bmiHeader.biHeight = -hgt;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    void* bits = nullptr;
    HBITMAP bmp = CreateDIBSection(sdc, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    HGDIOBJ old = SelectObject(mdc, bmp);
    PrintWindow(h, mdc, PW_RENDERFULLCONTENT);
    SelectObject(mdc, old);
    for (int i = 0; i < w * hgt; ++i) ((BYTE*)bits)[i * 4 + 3] = 255;

    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    IWICImagingFactory* f = nullptr;
    CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&f));
    if (f) {
        IWICStream* st = nullptr;
        IWICBitmapEncoder* enc = nullptr;
        IWICBitmapFrameEncode* fr = nullptr;
        IPropertyBag2* pb = nullptr;
        f->CreateStream(&st);
        HRESULT hrs = st ? st->InitializeFromFilename(path.c_str(), GENERIC_WRITE) : E_FAIL;
        wprintf(L"stream hr=%08x\n", (unsigned)hrs);
        if (SUCCEEDED(hrs)) {
            f->CreateEncoder(GUID_ContainerFormatPng, nullptr, &enc);
            enc->Initialize(st, WICBitmapEncoderNoCache);
            enc->CreateNewFrame(&fr, &pb);
            fr->Initialize(pb);
            fr->SetSize(w, hgt);
            WICPixelFormatGUID fmt = GUID_WICPixelFormat32bppBGRA;
            fr->SetPixelFormat(&fmt);
            fr->WritePixels(hgt, w * 4, (UINT)(w * hgt * 4), (BYTE*)bits);
            fr->Commit();
            enc->Commit();
        }
        if (pb) pb->Release();
        if (fr) fr->Release();
        if (enc) enc->Release();
        if (st) st->Release();
        f->Release();
    }
    DeleteObject(bmp);
    DeleteDC(mdc);
    ReleaseDC(nullptr, sdc);
}

static void OnShown(HWND dlg, void*) {
    const wchar_t* names[] = {L"type", L"position", L"effects", L"emboss", L"visibility", L"settings"};
    for (int i = 0; i < 6; ++i) {
        HWND tab = GetDlgItem(dlg, 100 + i);
        SendMessageW(dlg, WM_COMMAND, MAKEWPARAM(100 + i, BN_CLICKED), (LPARAM)tab);
        Sleep(300);
        MSG m;
        while (PeekMessageW(&m, nullptr, 0, 0, PM_REMOVE)) { TranslateMessage(&m); DispatchMessageW(&m); }
        SaveWindowPng(dlg, g_dir + L"\\ui_" + g_tag + L"_" + names[i] + L".png");
    }
    PostMessageW(dlg, WM_CLOSE, 0, 0);
}

int wmain(int argc, wchar_t** argv) {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    g_dir = argc > 1 ? argv[1] : L".";
    std::wstring lang = argc > 2 ? argv[2] : L"pl";
    std::wstring theme = argc > 3 ? argv[3] : L"light";
    g_tag = lang + L"_" + theme;
    CreateDirectoryW(g_dir.c_str(), nullptr);

    AppSettings st;
    SetDefaults(st.params);
    st.language = lang == L"pl" ? 1 : 2;
    st.theme = theme == L"dark" ? 2 : 1;

    HostInfo hi;
    hi.imageW = 3000; hi.imageH = 2000;
    hi.filterRect = {0, 0, 3000, 2000};
    hi.hasSelection = false;
    hi.rgbOk = true;
    hi.foreColor = 0xFFD400;

    WinRasterizer ras;
    ui::DialogContext ctx;
    ctx.host = &hi;
    ctx.ras = &ras;
    ctx.settings = &st;
    ctx.onShown = OnShown;
    Params p = st.params;
    p.color = hi.foreColor;
    p.embossOn = 1;
    p.strokeOn = 1;
    ui::ShowDialog(ctx, p);
    return 0;
}
