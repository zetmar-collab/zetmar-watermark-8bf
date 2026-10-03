// Motyw okna (jasny/ciemny) i wspolne funkcje rysowania wlasnych kontrolek.
#pragma once
#include <windows.h>

namespace wm {
namespace ui {

struct Theme {
    bool dark = false;
    COLORREF bg, panel, text, textDim, edit, border, accent, button, buttonHot, buttonDown, disabled;
    HBRUSH bgBrush = nullptr, editBrush = nullptr, panelBrush = nullptr;
};

// theme: 0 = automatycznie (ustawienie Windows), 1 = jasny, 2 = ciemny
bool ResolveDark(int themeSetting);
void MakeTheme(Theme& t, bool dark);
void FreeTheme(Theme& t);

// Ciemny pasek tytulu i ciemne systemowe kontrolki (combo, trackbar, przewijanie) - gdy system to umozliwia.
void ApplyWindowTheme(HWND hwnd, bool dark);
void ApplyControlTheme(HWND ctl, bool dark, const wchar_t* kind);  // kind: L"CFD" (combo/edit), L"Explorer"

// Rysowanie przycisku/checkboxa/radia/tabu wg stanu DRAWITEMSTRUCT.
enum class ButtonKind { Push, Check, Radio, Tab, Grid, Swatch };
void DrawButton(const DRAWITEMSTRUCT* dis, const Theme& t, ButtonKind kind, const wchar_t* text, bool checked,
                COLORREF swatch, int dpi);

// Uchwyt modulu (DLL wtyczki albo exe testowego).
HINSTANCE ModuleInstance();

inline int Scale(int v, int dpi) { return MulDiv(v, dpi, 96); }

}  // namespace ui
}  // namespace wm
