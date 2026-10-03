// Podstawowe operacje na buforach zmiennoprzecinkowych (deterministyczne, jednowatkowe).
#pragma once
#include <cstdint>
#include <vector>

namespace wm {

// Obraz RGBA, alfa premultiplikowana, 4 floaty na piksel, zakres 0..1.
struct FImage {
    int w = 0, h = 0;
    std::vector<float> d;

    FImage() = default;
    FImage(int w_, int h_) : w(w_), h(h_), d((size_t)w_ * h_ * 4, 0.0f) {}
    bool empty() const { return w <= 0 || h <= 0; }
    float* px(int x, int y) { return &d[((size_t)y * w + x) * 4]; }
    const float* px(int x, int y) const { return &d[((size_t)y * w + x) * 4]; }
};

// Maska jednokanalowa 0..1.
struct Mask {
    int w = 0, h = 0;
    std::vector<float> d;

    Mask() = default;
    Mask(int w_, int h_) : w(w_), h(h_), d((size_t)w_ * h_, 0.0f) {}
    float& at(int x, int y) { return d[(size_t)y * w + x]; }
    float at(int x, int y) const { return d[(size_t)y * w + x]; }
};

// Lanczos3, osobno dla kazdej osi; zachowuje premultiplikowana alfe.
FImage Resize(const FImage& src, int newW, int newH);

void FlipHorizontal(FImage& img);
void FlipVertical(FImage& img);

// Obrot o dowolny kat (stopnie, zgodnie z ruchem wskazowek zegara). Wielokrotnosci 90 stopni
// sa dokladne (bez interpolacji). Wynik jest powiekszony do prostokata opisujacego.
FImage Rotate(const FImage& src, double degrees);

// Rozmycie Gaussa (zero poza krawedzia). sigma <= 0 => bez zmian.
void GaussianBlur(Mask& m, double sigma);

// Alfa -> maska.
Mask AlphaOf(const FImage& img);

// Dodaje do obrazu przezroczysty margines (px z kazdej strony).
FImage Pad(const FImage& src, int pad);

// Przycina do prostokata zawierajacego piksele o alfa > threshold. Zwraca false, jesli pusty.
// outX/outY = przesuniecie lewego-gornego rogu wyniku wzgledem src.
bool CropToContent(const FImage& src, float threshold, FImage& out, int& outX, int& outY);

// Odleglosc ze znakiem od krawedzi ksztaltu (px). Dodatnia na zewnatrz, ujemna wewnatrz;
// piksele brzegowe (0<a<1) dostaja 0.5 - a.
Mask SignedDistance(const Mask& alpha);

}  // namespace wm
