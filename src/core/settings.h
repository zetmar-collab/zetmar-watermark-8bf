// Ustawienia trwale: %APPDATA%\ZetmarWatermark\settings.json oraz presety (presets\*.json).
#pragma once
#include <string>
#include <vector>

#include "params.h"

namespace wm {

struct AppSettings {
    int language = 0;  // 0 = automatycznie (z Windows), 1 = polski, 2 = angielski
    int theme = 0;     // 0 = automatycznie, 1 = jasny, 2 = ciemny
    std::wstring lastImageDir;
    std::wstring lastFontsFilter;  // zarezerwowane
    bool hasParams = false;
    Params params;  // ustawienia ostatniego uzycia
};

// Katalog %APPDATA%\ZetmarWatermark (tworzony przy zapisie). Pusty, gdy nie da sie ustalic.
std::wstring DataDir();

AppSettings LoadSettings();
bool SaveSettings(const AppSettings& s);

std::vector<std::wstring> ListPresets();  // same nazwy (bez .json), posortowane
bool LoadPreset(const std::wstring& name, Params& out);
bool SavePreset(const std::wstring& name, const Params& p);
bool DeletePreset(const std::wstring& name);
// Zamienia znaki niedozwolone w nazwie pliku; pusty wynik => nazwa niepoprawna.
std::wstring SanitizePresetName(const std::wstring& name);

}  // namespace wm
