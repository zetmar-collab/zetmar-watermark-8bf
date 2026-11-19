// Obrys i plaskorzezba liczone na ksztalcie podpisu (kanal alfa).
#pragma once
#include "imageops.h"
#include "params.h"

namespace wm {

// Wszystkie rozmiary w pikselach docelowego obrazu (parametry px juz przeskalowane).
struct StrokeSpec {
    float color[3];
    float width;
    int position;     // StrokePos
    float opacity;    // 0..1
};

struct EmbossSpec {
    int style;        // EmbossStyle
    bool down;        // wklesly
    float depth;      // 0.01..10 (1 = 100%)
    float size;       // px
    float soften;     // px
    float angleDeg;   // 0..360
    float altDeg;     // 0..90
    float hiColor[3], shColor[3];
    float hiOpacity, shOpacity;  // 0..1
};

struct ShadowSpec {
    float color[3];
    float opacity;    // 0..1
    float angleDeg;   // kierunek swiatla; cien pada w strone przeciwna
    float distance;   // px
    float blur;       // px
};

// Dodatkowy margines (px), jakiego efekty potrzebuja wokol ksztaltu.
int EffectMargin(const StrokeSpec* stroke, const EmbossSpec* emboss, const ShadowSpec* shadow = nullptr);

// `img` to podpis (premult RGBA) z juz doliczonym marginesem. Kolejnosc jak w Photoshopie:
// wypelnienie -> rzezba -> obrys (obrys na wierzchu); cien lezy pod calosciami.
void ApplyEffects(FImage& img, const StrokeSpec* stroke, const EmbossSpec* emboss, const ShadowSpec* shadow = nullptr);

}  // namespace wm
