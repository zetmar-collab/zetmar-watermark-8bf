// Skladanie podpisu: rasteryzacja (tekst / obraz) -> pozycja -> efekty -> widocznosc.
#pragma once
#include <string>

#include "imageops.h"
#include "params.h"

namespace wm {

struct TextRequest {
    std::wstring text;
    std::wstring font;
    bool bold = false, italic = false;
    double sizePx = 20;       // wysokosc em w pikselach
    double tracking = 0;      // 1/1000 em
    double lineSpacing = 100; // %
    int align = 0;            // 0 lewo, 1 srodek, 2 prawo
};

// Implementacje platformowe (GDI+/WIC) albo atrapy w testach.
class Rasterizer {
public:
    virtual ~Rasterizer() = default;
    // Zwraca obraz, w ktorym liczy sie tylko alfa (kolor ignorowany). false => blad (err opisuje).
    virtual bool RenderText(const TextRequest& req, FImage& out, std::wstring& err) = 0;
    // Wczytuje obraz do premultiplikowanego RGBA.
    virtual bool LoadImageFile(const std::wstring& path, FImage& out, std::wstring& err) = 0;
};

struct IRect {
    int left = 0, top = 0, right = 0, bottom = 0;
    int width() const { return right - left; }
    int height() const { return bottom - top; }
};

struct DocContext {
    int docW = 0, docH = 0;  // rozmiar warstwy / obrazu
    IRect ref;                // prostokat odniesienia dla pozycji (caly obraz albo zaznaczenie)
    double scale = 1.0;      // mnoznik wartosci w px (podglad < 1)
};

enum class Status {
    Ok,
    EmptyText,
    ImageNotSet,
    ImageLoadFailed,
    TooLarge,
    NothingToDraw,
    RenderFailed,
};

struct Rendered {
    int x0 = 0, y0 = 0;  // lewy-gorny naroznik w ukladzie wspolrzednych obrazu
    FImage img;          // premult RGBA, widocznosc juz uwzgledniona
};

struct RenderResult {
    Status status = Status::Ok;
    std::wstring detail;  // np. komunikat systemowy
    Rendered sig;
};

constexpr long long kMaxSignaturePixels = 64LL * 1000 * 1000;

// Pozycja lewego-gornego rogu prostokata (bw x bh) wg siatki 3x3, marginesow i offsetu.
void PlaceBox(const Params& p, const DocContext& ctx, int bw, int bh, int& x, int& y);

RenderResult RenderSignature(const Params& p, const DocContext& ctx, Rasterizer& ras);

}  // namespace wm
