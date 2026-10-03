# Testy ręczne (macierz) – do wykonania w Photoshopie i Elements

Instalacja: `install.ps1`, potem restart programu. Menu: Filtr → Zetmar → Podpis (Watermark)…
Jeśli coś nie działa: utwórz pusty plik `%APPDATA%\ZetmarWatermark\debug.flag`, powtórz i wyślij `log.txt`.

## Krok 1 – ładowanie
- [ ] Wtyczka widoczna w menu Zetmar, bez komunikatu o błędzie przy starcie programu.
- [ ] Filtr wyszarzony w CMYK / Grayscale / 32 bit, aktywny w RGB 8 i 16 bit.

## Krok 2/3 – zapis do warstwy
- [ ] Pusta warstwa + OK: podpis tylko na tej warstwie, reszta bez zmian.
- [ ] Ctrl+Z cofa jednym krokiem.
- [ ] Warstwa tła: pojawia się pytanie „Zastosuj bezpośrednio / Anuluj”.
- [ ] Z zaznaczeniem i bez; przełącznik „pozycja względem zaznaczenia”.
- [ ] 9 pozycji siatki, marginesy px i %, przesunięcie.
- [ ] Widoczność 0 / 50 / 100 %; zmiana trybu mieszania i krycia warstwy po zastosowaniu.
- [ ] Kolor domyślny = kolor pierwszoplanowy; przycisk „Użyj koloru pierwszoplanowego”.

## Krok 4/5 – grafika i efekty
- [ ] PNG z alfa, JPG, TIFF; skala, obrót, odbicia, „przekoloruj”.
- [ ] Obrys: na zewnątrz / środek / wewnątrz. Rzeźba: 4 style × wypukły/wklęsły.

## Krok 6/7 – interfejs
- [ ] PL ↔ EN (zakładka Ustawienia), motyw jasny/ciemny/auto.
- [ ] Presety: zapis, wczytanie, usunięcie. Ctrl+F.
- [ ] Dokumenty: 800 px, >10 000 px, pionowy/poziomy/kwadratowy; 8 i 16 bit.
- [ ] Błędne dane: pusty tekst, brak pliku obrazu, nieistniejąca czcionka – komunikat, bez crasha.
