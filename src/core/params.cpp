#include "params.h"

#include <windows.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>

namespace wm {

std::string WideToUtf8(const std::wstring& w) {
    if (w.empty()) return std::string();
    int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    std::string s(n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), &s[0], n, nullptr, nullptr);
    return s;
}

std::wstring Utf8ToWide(const std::string& s) {
    if (s.empty()) return std::wstring();
    int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
    std::wstring w(n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), &w[0], n);
    return w;
}

void SetDefaults(Params& p) {
    std::memset(&p, 0, sizeof(p));
    p.magic = kParamsMagic;
    p.version = kParamsVersion;
    p.mode = kModeText;
    wcscpy_s(p.text, L"© Marek Zettel");
    wcscpy_s(p.fontName, L"Segoe UI");
    p.sizeUnit = kUnitPercentWidth;
    p.sizeValue = 5.0;
    p.tracking = 0;
    p.lineSpacing = 100;
    p.color = 0xFFFFFF;
    p.useForeColor = 1;
    p.imgScaleUnit = kUnitPercentWidth;
    p.imgScale = 20.0;
    p.rotation = 0;
    p.anchor = 8;  // dol-prawo
    p.marginUnit = kUnitPercentWidth;
    p.marginX = 2.0;
    p.marginY = 2.0;
    p.strokeOn = 0;
    p.strokeColor = 0x000000;
    p.strokeWidth = 2.0;
    p.strokePos = kStrokeOutside;
    p.strokeOpacity = 100;
    p.embossOn = 0;
    p.embossStyle = kEmbossEmboss;
    p.depth = 100;
    p.size = 4;
    p.soften = 1;
    p.angle = 120;
    p.altitude = 30;
    p.hiColor = 0xFFFFFF;
    p.shColor = 0x000000;
    p.hiOpacity = 75;
    p.shOpacity = 75;
    p.visibility = 100;
    p.direct = 0;
    p.blend = kBlendNormal;
    p.textAlign = 0;
    p.shadowOn = 0;
    p.shadowColor = 0x000000;
    p.shadowOpacity = 60;
    p.shadowAngle = 120;
    p.shadowDistance = 4;
    p.shadowBlur = 4;
    p.tileOn = 0;
    p.tileSpacingX = 80;
    p.tileSpacingY = 80;
    p.tileAngle = -30;
    p.tileStagger = 1;
}

static double Clamp(double v, double lo, double hi) {
    if (!(v == v)) return lo;  // NaN
    return v < lo ? lo : (v > hi ? hi : v);
}
static int32_t ClampI(int32_t v, int32_t lo, int32_t hi) { return v < lo ? lo : (v > hi ? hi : v); }

template <size_t N>
static void Terminate(wchar_t (&a)[N]) { a[N - 1] = 0; }

bool IsLocalDrivePath(const wchar_t* path) {
    // Tylko "X:\..." albo "X:/...". Odrzuca UNC (\\serwer), \\?\ , \\.\ , \??\ i sciezki wzgledne.
    if (!((path[0] >= L'A' && path[0] <= L'Z') || (path[0] >= L'a' && path[0] <= L'z'))) return false;
    if (path[1] != L':' || (path[2] != L'\\' && path[2] != L'/')) return false;
    for (const wchar_t* s = path + 3; *s; ++s)
        if ((s[0] == L'\\' || s[0] == L'/') && (s[1] == L'\\' || s[1] == L'/')) return false;
    return true;
}

void Sanitize(Params& p) {
    p.magic = kParamsMagic;
    p.version = kParamsVersion;
    Terminate(p.text);
    Terminate(p.fontName);
    Terminate(p.imagePath);
    p.mode = ClampI(p.mode, 0, 1);
    p.bold = p.bold ? 1 : 0;
    p.italic = p.italic ? 1 : 0;
    p.sizeUnit = ClampI(p.sizeUnit, 0, 1);
    p.sizeValue = p.sizeUnit == kUnitPx ? Clamp(p.sizeValue, 4, 2000) : Clamp(p.sizeValue, 0.1, 100);
    p.tracking = Clamp(p.tracking, -200, 2000);
    p.lineSpacing = Clamp(p.lineSpacing, 20, 400);
    p.color &= 0xFFFFFF;
    p.useForeColor = p.useForeColor ? 1 : 0;
    p.imgScaleUnit = ClampI(p.imgScaleUnit, 0, 1);
    p.imgScale = Clamp(p.imgScale, p.imgScaleUnit == kUnitPx ? 1 : 0.1, p.imgScaleUnit == kUnitPx ? 400 : 100);
    p.rotation = Clamp(p.rotation, -180, 180);
    p.flipH = p.flipH ? 1 : 0;
    p.flipV = p.flipV ? 1 : 0;
    p.recolor = p.recolor ? 1 : 0;
    p.anchor = ClampI(p.anchor, 0, 8);
    p.marginUnit = ClampI(p.marginUnit, 0, 1);
    p.marginX = Clamp(p.marginX, 0, 100000);
    p.marginY = Clamp(p.marginY, 0, 100000);
    p.offsetX = Clamp(p.offsetX, -100000, 100000);
    p.offsetY = Clamp(p.offsetY, -100000, 100000);
    p.relToSelection = p.relToSelection ? 1 : 0;
    p.strokeOn = p.strokeOn ? 1 : 0;
    p.strokeColor &= 0xFFFFFF;
    p.strokeWidth = Clamp(p.strokeWidth, 0, 100);
    p.strokePos = ClampI(p.strokePos, 0, 2);
    p.strokeOpacity = Clamp(p.strokeOpacity, 0, 100);
    p.embossOn = p.embossOn ? 1 : 0;
    p.embossStyle = ClampI(p.embossStyle, 0, 3);
    p.embossDown = p.embossDown ? 1 : 0;
    p.depth = Clamp(p.depth, 1, 1000);
    p.size = Clamp(p.size, 1, 100);
    p.soften = Clamp(p.soften, 0, 16);
    p.angle = Clamp(p.angle, 0, 360);
    p.altitude = Clamp(p.altitude, 0, 90);
    p.hiColor &= 0xFFFFFF;
    p.shColor &= 0xFFFFFF;
    p.hiOpacity = Clamp(p.hiOpacity, 0, 100);
    p.shOpacity = Clamp(p.shOpacity, 0, 100);
    p.visibility = Clamp(p.visibility, 0, 100);
    p.direct = p.direct ? 1 : 0;
    p.blend = ClampI(p.blend, 0, kBlendCount - 1);
    p.textAlign = ClampI(p.textAlign, 0, 3);
    p.shadowOn = p.shadowOn ? 1 : 0;
    p.shadowColor &= 0xFFFFFF;
    p.shadowOpacity = Clamp(p.shadowOpacity, 0, 100);
    p.shadowAngle = Clamp(p.shadowAngle, 0, 360);
    p.shadowDistance = Clamp(p.shadowDistance, 0, 200);
    p.shadowBlur = Clamp(p.shadowBlur, 0, 100);
    p.tileOn = p.tileOn ? 1 : 0;
    p.tileSpacingX = Clamp(p.tileSpacingX, -500, 5000);
    p.tileSpacingY = Clamp(p.tileSpacingY, -500, 5000);
    p.tileAngle = Clamp(p.tileAngle, -90, 90);
    p.tileStagger = p.tileStagger ? 1 : 0;
}

bool IsValid(const Params& p) { return p.magic == kParamsMagic && p.version == kParamsVersion; }

// ---------------------------------------------------------------------------------------------
// Serializacja: jedno miejsce z lista pol, dwa "odwiedzajacy" (zapis i odczyt).

namespace {

struct Writer {
    std::string out;
    bool first = true;
    void key(const char* k) {
        out += first ? "\n  \"" : ",\n  \"";
        first = false;
        out += k;
        out += "\": ";
    }
    static std::string Escape(const std::string& s) {
        std::string r;
        for (unsigned char c : s) {
            switch (c) {
                case '"': r += "\\\""; break;
                case '\\': r += "\\\\"; break;
                case '\n': r += "\\n"; break;
                case '\r': r += "\\r"; break;
                case '\t': r += "\\t"; break;
                default:
                    if (c < 0x20) { char b[8]; std::snprintf(b, sizeof b, "\\u%04x", c); r += b; }
                    else r += (char)c;
            }
        }
        return r;
    }
    template <size_t N> void operator()(const char* k, wchar_t (&v)[N]) {
        key(k); out += "\"" + Escape(WideToUtf8(v)) + "\"";
    }
    void operator()(const char* k, double& v) { key(k); char b[40]; std::snprintf(b, sizeof b, "%.6g", v); out += b; }
    void operator()(const char* k, int32_t& v) { key(k); out += std::to_string(v); }
    void color(const char* k, uint32_t& v) { key(k); char b[16]; std::snprintf(b, sizeof b, "\"#%06X\"", v & 0xFFFFFF); out += b; }
};

struct Value { bool isString = false; std::string s; double n = 0; };

struct Reader {
    std::map<std::string, Value> kv;
    template <size_t N> void operator()(const char* k, wchar_t (&v)[N]) {
        auto it = kv.find(k);
        if (it == kv.end() || !it->second.isString) return;
        std::wstring w = Utf8ToWide(it->second.s);
        wcsncpy_s(v, N, w.c_str(), _TRUNCATE);
    }
    void operator()(const char* k, double& v) {
        auto it = kv.find(k);
        if (it != kv.end() && !it->second.isString) v = it->second.n;
    }
    void operator()(const char* k, int32_t& v) {
        auto it = kv.find(k);
        if (it != kv.end() && !it->second.isString) v = (int32_t)std::lround(it->second.n);
    }
    void color(const char* k, uint32_t& v) {
        auto it = kv.find(k);
        if (it == kv.end() || !it->second.isString) return;
        const std::string& s = it->second.s;
        if (s.size() >= 2 && s[0] == '#') v = (uint32_t)std::strtoul(s.c_str() + 1, nullptr, 16) & 0xFFFFFF;
    }
};

template <class V>
void Visit(Params& p, V& v) {
    v("mode", p.mode);
    v("text", p.text);
    v("fontName", p.fontName);
    v("bold", p.bold);
    v("italic", p.italic);
    v("sizeUnit", p.sizeUnit);
    v("sizeValue", p.sizeValue);
    v("tracking", p.tracking);
    v("lineSpacing", p.lineSpacing);
    v.color("color", p.color);
    v("useForeColor", p.useForeColor);
    v("imagePath", p.imagePath);
    v("imgScaleUnit", p.imgScaleUnit);
    v("imgScale", p.imgScale);
    v("rotation", p.rotation);
    v("flipH", p.flipH);
    v("flipV", p.flipV);
    v("recolor", p.recolor);
    v("anchor", p.anchor);
    v("marginUnit", p.marginUnit);
    v("marginX", p.marginX);
    v("marginY", p.marginY);
    v("offsetX", p.offsetX);
    v("offsetY", p.offsetY);
    v("relToSelection", p.relToSelection);
    v("strokeOn", p.strokeOn);
    v.color("strokeColor", p.strokeColor);
    v("strokeWidth", p.strokeWidth);
    v("strokePos", p.strokePos);
    v("strokeOpacity", p.strokeOpacity);
    v("embossOn", p.embossOn);
    v("embossStyle", p.embossStyle);
    v("embossDown", p.embossDown);
    v("depth", p.depth);
    v("size", p.size);
    v("soften", p.soften);
    v("angle", p.angle);
    v("altitude", p.altitude);
    v.color("hiColor", p.hiColor);
    v.color("shColor", p.shColor);
    v("hiOpacity", p.hiOpacity);
    v("shOpacity", p.shOpacity);
    v("visibility", p.visibility);
    v("direct", p.direct);
    v("blend", p.blend);
    v("textAlign", p.textAlign);
    v("shadowOn", p.shadowOn);
    v.color("shadowColor", p.shadowColor);
    v("shadowOpacity", p.shadowOpacity);
    v("shadowAngle", p.shadowAngle);
    v("shadowDistance", p.shadowDistance);
    v("shadowBlur", p.shadowBlur);
    v("tileOn", p.tileOn);
    v("tileSpacingX", p.tileSpacingX);
    v("tileSpacingY", p.tileSpacingY);
    v("tileAngle", p.tileAngle);
    v("tileStagger", p.tileStagger);
}

// Minimalny parser plaskiego obiektu JSON {"k": "str" | liczba | true | false, ...}
struct Parser {
    const std::string& s;
    size_t i = 0;
    explicit Parser(const std::string& str) : s(str) {}
    void ws() { while (i < s.size() && (s[i] == ' ' || s[i] == '\n' || s[i] == '\r' || s[i] == '\t')) ++i; }
    bool str(std::string& out) {
        if (i >= s.size() || s[i] != '"') return false;
        ++i;
        out.clear();
        while (i < s.size() && s[i] != '"') {
            char c = s[i++];
            if (c != '\\') { out += c; continue; }
            if (i >= s.size()) return false;
            char e = s[i++];
            switch (e) {
                case 'n': out += '\n'; break;
                case 'r': out += '\r'; break;
                case 't': out += '\t'; break;
                case 'b': out += '\b'; break;
                case 'f': out += '\f'; break;
                case 'u': {
                    if (i + 4 > s.size()) return false;
                    unsigned cp = (unsigned)std::strtoul(s.substr(i, 4).c_str(), nullptr, 16);
                    i += 4;
                    if (cp >= 0xD800 && cp < 0xDC00 && i + 6 <= s.size() && s[i] == '\\' && s[i + 1] == 'u') {
                        unsigned lo = (unsigned)std::strtoul(s.substr(i + 2, 4).c_str(), nullptr, 16);
                        i += 6;
                        cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                    }
                    std::wstring w;
                    if (cp >= 0x10000) { cp -= 0x10000; w += (wchar_t)(0xD800 + (cp >> 10)); w += (wchar_t)(0xDC00 + (cp & 0x3FF)); }
                    else w += (wchar_t)cp;
                    out += WideToUtf8(w);
                    break;
                }
                default: out += e;
            }
        }
        if (i >= s.size()) return false;
        ++i;
        return true;
    }
    bool parse(std::map<std::string, Value>& kv) {
        ws();
        if (i >= s.size() || s[i] != '{') return false;
        ++i;
        ws();
        if (i < s.size() && s[i] == '}') return true;
        while (i < s.size()) {
            ws();
            std::string k;
            if (!str(k)) return false;
            ws();
            if (i >= s.size() || s[i] != ':') return false;
            ++i;
            ws();
            Value v;
            if (i < s.size() && s[i] == '"') {
                v.isString = true;
                if (!str(v.s)) return false;
            } else if (s.compare(i, 4, "true") == 0) { v.n = 1; i += 4; }
            else if (s.compare(i, 5, "false") == 0) { v.n = 0; i += 5; }
            else if (s.compare(i, 4, "null") == 0) { i += 4; ws(); if (i < s.size() && s[i] == ',') { ++i; continue; } break; }
            else {
                char* end = nullptr;
                v.n = std::strtod(s.c_str() + i, &end);
                if (end == s.c_str() + i) return false;
                i = (size_t)(end - s.c_str());
            }
            kv[k] = v;
            ws();
            if (i < s.size() && s[i] == ',') { ++i; continue; }
            break;
        }
        ws();
        return i < s.size() && s[i] == '}';
    }
};

}  // namespace

std::string ToJson(const Params& pc) {
    Params p = pc;
    Writer w;
    Visit(p, w);
    return "{" + w.out + "\n}\n";
}

bool FromJson(const std::string& json, Params& p) {
    Reader r;
    Parser parser(json);
    if (!parser.parse(r.kv)) return false;
    Visit(p, r);
    Sanitize(p);
    // Plik spoza programu (preset, ustawienia) nie moze wskazac zasobu sieciowego ani urzadzenia:
    // otwarcie takiej sciezki wyslaloby dane logowania Windows. Obraz trzeba wskazac w oknie ponownie.
    if (p.imagePath[0] && !IsLocalDrivePath(p.imagePath)) p.imagePath[0] = 0;
    return true;
}

}  // namespace wm
