# Zetmar Podpis (Watermark) – wtyczka .8bf dla Photoshopa i Photoshop Elements

Filtr nakłada podpis tekstowy lub graficzny (z obrysem i płaskorzeźbą) na **aktywną warstwę**.
Menu: **Filtr → Zetmar → Podpis (Watermark)…**. Specyfikacja: `SPECYFIKACJA_Watermark_8bf.md`.

## Jak używać

1. Utwórz pustą, przezroczystą warstwę (Ctrl+Shift+N).
2. Filtr → Zetmar → Podpis (Watermark)…
3. Ustaw podpis, pozycję, efekty, widoczność. OK.
4. Tryb mieszania i krycie warstwy ustawiasz w panelu Warstwy (Photoshop robi to natywnie).
5. Ctrl+F powtarza filtr z ostatnimi ustawieniami (bez okna).

Na warstwie tła (bez przezroczystości) wtyczka pyta, czy zastosować podpis bezpośrednio (destrukcyjnie, z własnym trybem mieszania z zakładki Widoczność).

Obsługiwane: RGB 8 i 16 bit. Pozostałe tryby są w menu wyszarzone.

## Instalacja

```powershell
powershell -ExecutionPolicy Bypass -File install.ps1      # kopiuje do Photoshop / Elements (prosi o UAC)
powershell -ExecutionPolicy Bypass -File install.ps1 -Uninstall
```

Ręcznie: skopiuj `ZetmarWatermark_64.8bf` do
`C:\Program Files\Adobe\Adobe Photoshop <wersja>\Plug-ins\` albo
`C:\Program Files\Adobe\Photoshop Elements <wersja>\Plug-Ins\` i uruchom program ponownie.

## Budowanie

Wymagania: Visual Studio 2022 lub Build Tools z „Desktop development with C++” (CMake i Ninja są w zestawie),
Adobe Photoshop SDK (nagłówki z https://github.com/AdobeDocs/photoshop-cpp-sdk, katalog `sdk/pluginsdk`).

```
build.cmd                 # buduje wszystko (Release x64) do build\
build.cmd wm_tests        # tylko testy rdzenia
```

Ścieżkę SDK zmienia zmienna CMake `PS_SDK_DIR` (domyślnie `sdk/pluginsdk`).

Wynik: `build\ZetmarWatermark_64.8bf` (statyczny CRT, zależy tylko od bibliotek Windows).

## Struktura

| Katalog | Zawartość |
|---|---|
| `src/core` | parametry + JSON, rasteryzacja/skład podpisu, efekty, tryby mieszania, i18n, ustawienia (bez zależności od Photoshopa) |
| `src/platform` | GDI+ (tekst) i WIC (obrazy) |
| `src/plugin` | punkt wejścia filtra, odczyt/zapis pikseli hosta, podgląd z hosta |
| `src/ui` | okno Win32, motyw jasny/ciemny, komunikaty |
| `res` | `pipl.json` (PiPL: kategoria „Zetmar”), `plugin.rc` |
| `tests` | `core_tests.cpp` (testy rdzenia, zapis PNG do `tests/out`), `ui_harness.cpp` (zrzuty okna bez Photoshopa) |

## Ustawienia i presety

`%APPDATA%\ZetmarWatermark\settings.json` (ostatnie użycie, język, motyw), `presets\*.json`.
Dziennik diagnostyczny: utwórz pusty plik `%APPDATA%\ZetmarWatermark\debug.flag` – wtyczka zapisze `log.txt`.

## Znane ograniczenia wersji 1

- Filtr nie tworzy warstwy (ograniczenie SDK) – stąd pusta warstwa robiona ręcznie.
- Brak Action Managera (nagrywanie w Akcjach) – wersja 2.
- Podgląd pokazuje złożony dokument, jeśli host to udostępnia; w przeciwnym razie tylko aktywną warstwę (komunikat pod podglądem).
- Tryb „przekoloruj” dla obrazu bez przezroczystości traktuje ciemne piksele jako tusz (czarny znak na białym tle).

## SDK

Katalog `sdk/` nie jest w repozytorium (licencja Adobe). Pobierz: `git clone --depth 1 https://github.com/AdobeDocs/photoshop-cpp-sdk` i skopiuj jego `pluginsdk` do `sdk/pluginsdk`.
