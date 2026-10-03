#include "settings.h"

#include <windows.h>

#include <shlobj.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>

namespace wm {

namespace {

bool ReadWholeFile(const std::wstring& path, std::string& out) {
    HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                           FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    LARGE_INTEGER sz;
    bool ok = GetFileSizeEx(h, &sz) && sz.QuadPart < (64 << 20);
    if (ok) {
        out.resize((size_t)sz.QuadPart);
        DWORD got = 0;
        ok = ReadFile(h, out.data(), (DWORD)out.size(), &got, nullptr) && got == out.size();
    }
    CloseHandle(h);
    if (ok && out.size() >= 3 && (unsigned char)out[0] == 0xEF && (unsigned char)out[1] == 0xBB &&
        (unsigned char)out[2] == 0xBF)
        out.erase(0, 3);
    return ok;
}

// Zapis przez plik tymczasowy, zeby przerwany zapis nie zostawil uszkodzonego pliku.
bool WriteFileAtomic(const std::wstring& path, const std::string& data) {
    std::wstring tmp = path + L".tmp";
    HANDLE h = CreateFileW(tmp.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    DWORD wrote = 0;
    bool ok = WriteFile(h, data.data(), (DWORD)data.size(), &wrote, nullptr) && wrote == data.size();
    CloseHandle(h);
    if (!ok) { DeleteFileW(tmp.c_str()); return false; }
    if (!MoveFileExW(tmp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING)) { DeleteFileW(tmp.c_str()); return false; }
    return true;
}

std::wstring PresetsDir() {
    std::wstring d = DataDir();
    return d.empty() ? d : d + L"\\presets";
}

// Wydobywa wartosc napisowa/liczbowa klucza z plaskiego JSON-a (na potrzeby pol poza Params).
std::string GetRaw(const std::string& json, const char* key) {
    std::string k = std::string("\"") + key + "\"";
    size_t p = json.find(k);
    if (p == std::string::npos) return "";
    p = json.find(':', p + k.size());
    if (p == std::string::npos) return "";
    ++p;
    while (p < json.size() && (json[p] == ' ' || json[p] == '\n' || json[p] == '\r' || json[p] == '\t')) ++p;
    return json.substr(p);
}

int GetInt(const std::string& json, const char* key, int def) {
    std::string r = GetRaw(json, key);
    if (r.empty() || r[0] == '"' || r[0] == '{') return def;
    return std::atoi(r.c_str());
}

// Koniec obiektu JSON zaczynajacego sie w `b` ('{'), z pominieciem nawiasow w napisach.
size_t ObjectEnd(const std::string& j, size_t b) {
    bool inStr = false;
    for (size_t i = b; i < j.size(); ++i) {
        char c = j[i];
        if (inStr) {
            if (c == '\\') ++i;
            else if (c == '"') inStr = false;
        } else if (c == '"') inStr = true;
        else if (c == '}') return i;
    }
    return std::string::npos;
}

std::string EscapeJson(const std::string& s) {
    std::string r;
    for (unsigned char c : s) {
        if (c == '"') r += "\\\"";
        else if (c == '\\') r += "\\\\";
        else if (c < 0x20) r += ' ';
        else r += (char)c;
    }
    return r;
}

std::wstring GetString(const std::string& json, const char* key) {
    std::string r = GetRaw(json, key);
    if (r.empty() || r[0] != '"') return L"";
    std::string out;
    for (size_t i = 1; i < r.size() && r[i] != '"'; ++i) {
        if (r[i] == '\\' && i + 1 < r.size()) { ++i; out += r[i]; }
        else out += r[i];
    }
    return Utf8ToWide(out);
}

}  // namespace

std::wstring DataDir() {
    wchar_t buf[MAX_PATH];
    if (FAILED(SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, SHGFP_TYPE_CURRENT, buf))) return L"";
    return std::wstring(buf) + L"\\ZetmarWatermark";
}

AppSettings LoadSettings() {
    AppSettings s;
    SetDefaults(s.params);
    std::wstring dir = DataDir();
    if (dir.empty()) return s;
    std::string json;
    if (!ReadWholeFile(dir + L"\\settings.json", json)) return s;
    s.language = std::min(std::max(GetInt(json, "language", 0), 0), 2);
    s.theme = std::min(std::max(GetInt(json, "theme", 0), 0), 2);
    s.lastImageDir = GetString(json, "lastImageDir");
    // Parametry siedza w zagniezdzonym obiekcie "params": { ... }
    size_t p = json.find("\"params\"");
    if (p != std::string::npos) {
        size_t b = json.find('{', p);
        size_t e = b == std::string::npos ? b : ObjectEnd(json, b);
        if (b != std::string::npos && e != std::string::npos) {
            Params pr;
            SetDefaults(pr);
            if (FromJson(json.substr(b, e - b + 1), pr)) {
                s.params = pr;
                s.hasParams = true;
            }
        }
    }
    return s;
}

bool SaveSettings(const AppSettings& s) {
    std::wstring dir = DataDir();
    if (dir.empty()) return false;
    CreateDirectoryW(dir.c_str(), nullptr);
    std::string j = "{\n";
    j += "  \"language\": " + std::to_string(s.language) + ",\n";
    j += "  \"theme\": " + std::to_string(s.theme) + ",\n";
    j += "  \"lastImageDir\": \"" + EscapeJson(WideToUtf8(s.lastImageDir)) + "\",\n";
    j += "  \"params\": " + ToJson(s.params);
    j += "}\n";
    return WriteFileAtomic(dir + L"\\settings.json", j);
}

std::wstring SanitizePresetName(const std::wstring& name) {
    std::wstring r;
    for (wchar_t c : name) {
        if (c < 32 || wcschr(L"\\/:*?\"<>|", c)) continue;
        r += c;
    }
    while (!r.empty() && (r.back() == L' ' || r.back() == L'.')) r.pop_back();
    size_t b = r.find_first_not_of(L' ');
    r = b == std::wstring::npos ? L"" : r.substr(b);
    if (r.size() > 80) r.resize(80);
    return r;
}

std::vector<std::wstring> ListPresets() {
    std::vector<std::wstring> out;
    std::wstring dir = PresetsDir();
    if (dir.empty()) return out;
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW((dir + L"\\*.json").c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return out;
    do {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
        std::wstring n = fd.cFileName;
        out.push_back(n.substr(0, n.size() - 5));
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    std::sort(out.begin(), out.end(), [](const std::wstring& a, const std::wstring& b) {
        return CompareStringOrdinal(a.c_str(), -1, b.c_str(), -1, TRUE) == CSTR_LESS_THAN;
    });
    return out;
}

bool LoadPreset(const std::wstring& name, Params& out) {
    std::wstring n = SanitizePresetName(name);
    if (n.empty()) return false;
    std::string json;
    if (!ReadWholeFile(PresetsDir() + L"\\" + n + L".json", json)) return false;
    Params p;
    SetDefaults(p);
    if (!FromJson(json, p)) return false;
    out = p;
    return true;
}

bool SavePreset(const std::wstring& name, const Params& p) {
    std::wstring n = SanitizePresetName(name);
    std::wstring dir = DataDir();
    if (n.empty() || dir.empty()) return false;
    CreateDirectoryW(dir.c_str(), nullptr);
    CreateDirectoryW((dir + L"\\presets").c_str(), nullptr);
    return WriteFileAtomic(PresetsDir() + L"\\" + n + L".json", ToJson(p));
}

bool DeletePreset(const std::wstring& name) {
    std::wstring n = SanitizePresetName(name);
    if (n.empty()) return false;
    return DeleteFileW((PresetsDir() + L"\\" + n + L".json").c_str()) != 0;
}

}  // namespace wm
