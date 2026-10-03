// Okno dialogowe wtyczki (Win32, motyw jasny/ciemny, podglad na zywo).
#pragma once
#include <windows.h>

#include "core/compose.h"
#include "core/params.h"
#include "core/settings.h"
#include "plugin/host.h"

namespace wm {
namespace ui {

struct DialogContext {
    HWND parent = nullptr;
    HINSTANCE instance = nullptr;
    FilterRecord* fr = nullptr;
    const HostInfo* host = nullptr;
    Rasterizer* ras = nullptr;
    AppSettings* settings = nullptr;  // jezyk/motyw/ostatnia sciezka; dialog moze je zmienic
    bool forceDirect = false;         // warstwa tla: tryb "bezposrednio" wymuszony
    // Hak testowy: wywolywany po pokazaniu okna (np. do zrzutow ekranu); okno zamyka sie przez WM_CLOSE.
    void (*onShown)(HWND dlg, void* user) = nullptr;
    void* onShownUser = nullptr;
};

// true = OK (p zawiera wynik), false = Anuluj.
bool ShowDialog(DialogContext& ctx, Params& p);

// Proste okna komunikatow w motywie okna (przyciski z wlasnymi napisami).
void ShowMessage(HWND parent, const wchar_t* text, const AppSettings& s);
// Zwraca 1 albo 2 (numer klikniętego przycisku); zamkniecie okna = 2.
int ShowChoice(HWND parent, const wchar_t* text, const wchar_t* btn1, const wchar_t* btn2, const AppSettings& s);

}  // namespace ui
}  // namespace wm
