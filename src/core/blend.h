// Tryby mieszania (wzory zgodne z Photoshopem / W3C Compositing) i skladanie piksela.
#pragma once
#include "params.h"

namespace wm {

// Funkcja mieszania jednego kanalu: cb = tlo, cs = zrodlo (oba 0..1, kolor prosty).
float BlendChannel(int mode, float cb, float cs);

// Sklada piksel zrodlowy (kolor prosty cs[3] + alfa as) na tlo (kolor prosty cb[3] + alfa ab).
// Wynik: kolor prosty + alfa. Dla trybu Normal i tla o alfa=0 zwraca dokladnie kolor zrodla.
void CompositePixel(int mode, const float cb[3], float ab, const float cs[3], float as,
                    float outColor[3], float& outAlpha);

}  // namespace wm
