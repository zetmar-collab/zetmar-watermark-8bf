// Parametry wtyczki "Podpis / Watermark". Struktura POD: trafia bez zmian do uchwytu
// FilterRecord::parameters (Ctrl+F) oraz jest serializowana do JSON (ustawienia, presety).
#pragma once
#include <cstdint>
#include <string>

namespace wm {

enum SigMode { kModeText = 0, kModeImage = 1 };
enum SizeUnit { kUnitPx = 0, kUnitPercentWidth = 1 };
enum StrokePos { kStrokeOutside = 0, kStrokeCenter = 1, kStrokeInside = 2 };
enum EmbossStyle { kEmbossEmboss = 0, kEmbossPillow = 1, kEmbossOuterBevel = 2, kEmbossInnerBevel = 3 };
enum BlendMode {
    kBlendNormal = 0, kBlendMultiply, kBlendScreen, kBlendOverlay, kBlendSoftLight,
    kBlendHardLight, kBlendLighten, kBlendDarken, kBlendDifference, kBlendCount
};

constexpr uint32_t kParamsMagic = 0x57424D5A;  // "ZMBW"
constexpr uint32_t kParamsVersion = 2;

struct Params {
    uint32_t magic;
    uint32_t version;

    // --- Typ podpisu
    int32_t mode;               // SigMode
    wchar_t text[512];
    wchar_t fontName[64];
    int32_t bold, italic;
    int32_t sizeUnit;           // SizeUnit
    double sizeValue;           // px (4..2000) albo % szerokosci (0.1..100)
    double tracking;            // odstep miedzy literami, 1/1000 em
    double lineSpacing;         // interlinia, % (100 = neutralna)
    uint32_t color;             // 0x00RRGGBB
    int32_t useForeColor;       // 1 = przy otwarciu bierz kolor pierwszoplanowy

    wchar_t imagePath[260];
    int32_t imgScaleUnit;       // kUnitPx => % natywnego rozmiaru, kUnitPercentWidth => % szerokosci zdjecia
    double imgScale;
    double rotation;            // -180..180
    int32_t flipH, flipV;
    int32_t recolor;            // przekoloruj obraz na `color`

    // --- Pozycja
    int32_t anchor;             // 0..8 (rzad * 3 + kolumna)
    int32_t marginUnit;         // kUnitPx albo kUnitPercentWidth (% krotszego boku)
    double marginX, marginY;
    double offsetX, offsetY;    // px
    int32_t relToSelection;

    // --- Obrys
    int32_t strokeOn;
    uint32_t strokeColor;
    double strokeWidth;         // px 0..100
    int32_t strokePos;          // StrokePos
    double strokeOpacity;       // %

    // --- Rzezba
    int32_t embossOn;
    int32_t embossStyle;        // EmbossStyle
    int32_t embossDown;         // 0 = wypukly, 1 = wklesly
    double depth;               // %
    double size;                // px
    double soften;              // px
    double angle;               // stopnie 0..360
    double altitude;            // stopnie 0..90
    uint32_t hiColor, shColor;
    double hiOpacity, shOpacity;  // %

    // --- Widocznosc / tryb awaryjny
    double visibility;          // 0..100
    int32_t direct;             // "zastosuj bezposrednio na tej warstwie"
    int32_t blend;              // BlendMode (tylko gdy direct)

    // --- Dodane w wersji 2
    int32_t textAlign;          // 0 = wg kotwicy, 1 = lewo, 2 = srodek, 3 = prawo
    int32_t shadowOn;
    uint32_t shadowColor;
    double shadowOpacity;       // %
    double shadowAngle;         // stopnie (kierunek swiatla, jak w rzezbie)
    double shadowDistance;      // px
    double shadowBlur;          // px
    int32_t tileOn;             // wzor kafelkowy na calym obrazie
    double tileSpacingX, tileSpacingY;  // odstep miedzy kafelkami, px
    double tileAngle;           // obrot wzoru, stopnie -90..90
    int32_t tileStagger;        // co drugi rzad przesuniety o pol kafelka
};

void SetDefaults(Params& p);
// Przycina wartosci do dozwolonych zakresow, naprawia magic/version i terminatory napisow.
void Sanitize(Params& p);
bool IsValid(const Params& p);
// Czy to zwykla sciezka lokalna "X:\\...": bez UNC, \\?\, \\.\ i podwojnych separatorow.
bool IsLocalDrivePath(const wchar_t* path);

// JSON (UTF-8). Parse zwraca false tylko dla niepoprawnej skladni; nieznane/brakujace klucze
// zostawiaja wartosci z `p` (wywolujacy ustawia wczesniej SetDefaults).
std::string ToJson(const Params& p);
bool FromJson(const std::string& json, Params& p);

std::string WideToUtf8(const std::wstring& w);
std::wstring Utf8ToWide(const std::string& s);

}  // namespace wm
