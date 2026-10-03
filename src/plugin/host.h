// Informacje o dokumencie/warstwie odczytane z FilterRecord oraz zapis podpisu do warstwy.
#pragma once
#include <windows.h>

#include <string>
#include <vector>

#include "core/compose.h"
#include "core/params.h"

struct FilterRecord;

namespace wm {

struct HostInfo {
    int imageW = 0, imageH = 0;  // rozmiar warstwy (uklad wspolrzednych filtra)
    IRect filterRect;             // zaznaczenie (obwiednia) albo caly obraz
    bool hasSelection = false;
    int depth = 8;               // bitow na kanal
    int planes = 0;              // liczba plaszczyzn dostepnych w buforze
    int transPlane = -1;         // indeks plaszczyzny przezroczystosci albo -1
    bool flatLayer = false;      // warstwa tla / brak edytowalnej przezroczystosci
    bool lockedTransparency = false;
    bool rgbOk = false;          // tryb RGB 8/16 bit
    int layerOffsetX = 0, layerOffsetY = 0;  // polozenie warstwy w dokumencie (gdy znane)
    uint32_t foreColor = 0xFFFFFF;
    HWND hwnd = nullptr;
};

HostInfo QueryHost(FilterRecord* fr);

// Podglad: pomniejszony obraz (RGB prosty 0..1 + alfa) odczytany z hosta.
struct PreviewBase {
    int w = 0, h = 0;
    bool merged = false;  // true = zlozony dokument (nieprzezroczysty), false = tylko aktywna warstwa
    std::vector<float> rgba;  // prosty RGBA, 4 floaty na piksel
    double scaleX = 1.0, scaleY = 1.0;  // podglad / rozmiar warstwy
};
bool ReadPreviewBase(FilterRecord* fr, const HostInfo& hi, int maxDim, PreviewBase& out);

enum class ApplyStatus { Ok, Cancelled, HostError };

// Rysuje podpis na warstwie. `status` dostaje wynik renderowania (np. EmptyText), `hostErr` kod bledu hosta.
ApplyStatus ApplySignature(FilterRecord* fr, const HostInfo& hi, const Params& p, Rasterizer& ras,
                           Status& renderStatus, std::wstring& detail, short& hostErr);

void DebugLog(const wchar_t* fmt, ...);

}  // namespace wm
