// Punkt wejscia filtra Photoshopa / Photoshop Elements (.8bf).
#include <windows.h>

#include "PIDefines.h"
#include "PIAbout.h"
#include "PIFilter.h"
#include "PITypes.h"

#include <new>

#include "core/i18n.h"
#include "core/settings.h"
#include "platform/rasterizer_win.h"
#include "plugin/host.h"
#include "ui/dialog.h"

using namespace wm;

namespace {

HINSTANCE g_instance = nullptr;

// Dane trwale miedzy wywolaniami (wskaznik oddawany hostowi przez *data).
struct PluginData {
    bool queryForParameters = true;
};

FilterRecord* g_fr = nullptr;
int16* g_result = nullptr;
PluginData* g_data = nullptr;
Params* g_params = nullptr;

// Host bez Handle Suite (nietypowe programy) dostaje statyczny zapas zamiast uchwytu.
Params g_fallbackParams;
bool g_fallbackReady = false;
bool g_paramsLocked = false;  // true = g_params wskazuje na zablokowany uchwyt hosta

void LockParams() {
    g_params = nullptr;
    g_paramsLocked = false;
    if (!g_fr->handleProcs) {
        if (!g_fallbackReady) { SetDefaults(g_fallbackParams); g_fallbackReady = true; }
        g_params = &g_fallbackParams;
        return;
    }
    if (!g_fr->parameters) { *g_result = filterBadParameters; return; }
    g_params = (Params*)g_fr->handleProcs->lockProc(g_fr->parameters, TRUE);
    if (!g_params) { *g_result = memFullErr; return; }
    g_paramsLocked = true;
    if (!IsValid(*g_params)) SetDefaults(*g_params);
}

void UnlockParams() {
    if (g_fr && g_paramsLocked && g_fr->parameters && g_fr->handleProcs) g_fr->handleProcs->unlockProc(g_fr->parameters);
    g_paramsLocked = false;
    g_params = nullptr;
}

void EnsureStorage() {
    if (g_fr->handleProcs && !g_fr->parameters) {
        g_fr->parameters = g_fr->handleProcs->newProc((int32)sizeof(Params));
        if (!g_fr->parameters) { *g_result = memFullErr; return; }
        Params* p = (Params*)g_fr->handleProcs->lockProc(g_fr->parameters, TRUE);
        if (p) { SetDefaults(*p); g_fr->handleProcs->unlockProc(g_fr->parameters); }
    }
    if (!g_data) {
        static PluginData storage;  // jeden egzemplarz na czas zycia biblioteki (host tylko przechowuje wskaznik)
        g_data = &storage;
    }
}

Str MessageFor(Status s) {
    switch (s) {
        case Status::EmptyText: return Str::MsgEmptyText;
        case Status::ImageNotSet: return Str::MsgNoImage;
        case Status::ImageLoadFailed: return Str::MsgImageFail;
        case Status::TooLarge: return Str::MsgTooLarge;
        case Status::NothingToDraw: return Str::MsgNothing;
        default: return Str::MsgRenderFail;
    }
}

void DoAbout(AboutRecord* ar) {
    AppSettings s = LoadSettings();
    SetLanguage(ResolveLanguage(s.language));
    HWND hwnd = ar && ar->platformData ? (HWND)((PlatformData*)ar->platformData)->hwnd : nullptr;
    ui::ShowMessage(hwnd, T(Str::AboutText), s);
}

void DoParameters() {
    EnsureStorage();
    if (*g_result == noErr) g_data->queryForParameters = true;
}

void DoPrepare() {
    EnsureStorage();
    if (*g_result != noErr) return;
    g_fr->bufferSpace = 0;
}

void DoStart() {
    EnsureStorage();
    if (*g_result != noErr) return;
    LockParams();
    if (*g_result != noErr) return;

    AppSettings settings = LoadSettings();
    SetLanguage(ResolveLanguage(settings.language));
    HostInfo hi = QueryHost(g_fr);

    if (!hi.rgbOk) {
        ui::ShowMessage(hi.hwnd, T(Str::MsgUnsupportedMode), settings);
        *g_result = userCanceledErr;
        return;
    }

    Params p = *g_params;
    WinRasterizer ras;

    if (g_data->queryForParameters) {
        p = settings.hasParams ? settings.params : p;
        if (p.useForeColor) p.color = hi.foreColor;
        bool force = false;
        const int savedDirect = p.direct;  // wymuszenie trybu bezposredniego dotyczy tylko tego uruchomienia
        if (hi.flatLayer && !hi.lockedTransparency) {
            if (ui::ShowChoice(hi.hwnd, T(Str::MsgBackground), T(Str::BtnApplyDirect), T(Str::Cancel), settings) != 1) {
                *g_result = userCanceledErr;
                return;
            }
            p.direct = 1;
            force = true;
        } else if (hi.lockedTransparency) {
            ui::ShowMessage(hi.hwnd, T(Str::MsgProtected), settings);
        }
        ui::DialogContext dc;
        dc.parent = hi.hwnd;
        dc.instance = g_instance;
        dc.fr = g_fr;
        dc.host = &hi;
        dc.ras = &ras;
        dc.settings = &settings;
        dc.forceDirect = force;
        if (!ui::ShowDialog(dc, p)) {
            *g_result = userCanceledErr;
            return;
        }
        Sanitize(p);
        *g_params = p;
        settings.params = p;
        if (force) settings.params.direct = savedDirect;
        settings.hasParams = true;
        SaveSettings(settings);
        SetLanguage(ResolveLanguage(settings.language));
    } else if (hi.flatLayer && !hi.lockedTransparency && !p.direct) {
        if (ui::ShowChoice(hi.hwnd, T(Str::MsgBackground), T(Str::BtnApplyDirect), T(Str::Cancel), settings) != 1) {
            *g_result = userCanceledErr;
            return;
        }
        p.direct = 1;
    }
    g_data->queryForParameters = false;

    Status rs = Status::Ok;
    std::wstring detail;
    short hostErr = 0;
    ApplyStatus as = ApplySignature(g_fr, hi, p, ras, rs, detail, hostErr);
    if (as == ApplyStatus::HostError) {
        *g_result = hostErr;
    } else if (as == ApplyStatus::Cancelled) {
        if (rs != Status::Ok) {
            std::wstring msg = T(MessageFor(rs));
            if (!detail.empty() && rs == Status::RenderFailed) msg += L"\n(" + detail + L")";
            ui::ShowMessage(hi.hwnd, msg.c_str(), settings);
        }
        *g_result = userCanceledErr;
    }
}

void DoContinue() {
    // Pracujemy wylacznie przez advanceState(); nic wiecej do zrobienia.
    VRect z;
    z.left = z.top = z.right = z.bottom = 0;
    if (g_fr->bigDocumentData && g_fr->bigDocumentData->PluginUsing32BitCoordinates) {
        g_fr->bigDocumentData->inRect32 = z;
        g_fr->bigDocumentData->outRect32 = z;
        g_fr->bigDocumentData->maskRect32 = z;
    } else {
        Rect r;
        r.left = r.top = r.right = r.bottom = 0;
        g_fr->inRect = r;
        g_fr->outRect = r;
        g_fr->maskRect = r;
    }
}

}  // namespace

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        g_instance = module;
        DisableThreadLibraryCalls(module);
    }
    return TRUE;
}

DLLExport MACPASCAL void PluginMain(const int16 selector, FilterRecordPtr filterRecord, intptr_t* data,
                                    int16* result) {
    try {
        g_fr = filterRecord;
        g_result = result;
        g_data = (PluginData*)*data;

        if (selector == filterSelectorAbout) {
            DoAbout((AboutRecord*)filterRecord);
            return;
        }
        if (g_fr->bigDocumentData != nullptr) g_fr->bigDocumentData->PluginUsing32BitCoordinates = true;

        switch (selector) {
            case filterSelectorParameters: DoParameters(); break;
            case filterSelectorPrepare: DoPrepare(); break;
            case filterSelectorStart: DoStart(); break;
            case filterSelectorContinue: DoContinue(); break;
            case filterSelectorFinish: break;
            default: break;
        }
        UnlockParams();
        *data = (intptr_t)g_data;
    } catch (const std::bad_alloc&) {
        UnlockParams();
        if (result) *result = memFullErr;
    } catch (...) {
        UnlockParams();
        if (result) *result = -1;
    }
}
