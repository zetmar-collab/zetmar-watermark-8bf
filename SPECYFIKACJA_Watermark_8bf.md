# Specyfikacja: wtyczka „Podpis / Watermark" (.8bf) dla Photoshop i Photoshop Elements

Dokument dla Claude Code. Autor zamówienia: Marek Zettel (Cyfrowy Przyjaciel, zetmar.net.pl).

## 1. Cel

Wtyczka filtrująca w formacie `.8bf` (Windows, x64), która nakłada na zdjęcie podpis (watermark) na osobnej, przezroczystej warstwie. Działa w Adobe Photoshop (CC, aktualne wersje) i Adobe Photoshop Elements (2020 i nowsze, 64-bit).

Dwa tryby podpisu:
1. Graficzny: obraz z pliku (PNG z kanałem alfa, także JPG/TIFF).
2. Tekstowy: czcionka, rozmiar, kolor.

## 2. Ograniczenia techniczne (ważne, przeczytać przed pisaniem kodu)

Format `.8bf` to wtyczka typu Filter. Z natury architektury Photoshop SDK wynikają trzy rzeczy, które zmieniają sposób realizacji wymagań:

1. **Filtr nie tworzy nowej warstwy.** Filtr dostaje piksele aktywnej warstwy i je modyfikuje. Nie ma w SDK dla filtrów wywołania „utwórz warstwę". Automatyzacja (`.8li`) to inny typ wtyczki, a Photoshop Elements jej nie obsługuje. Rozwiązanie w tym projekcie: użytkownik tworzy pustą, przezroczystą warstwę (Ctrl+Shift+N), a filtr rysuje podpis tylko na niej. Wtyczka wykrywa, czy aktywna warstwa jest przezroczysta/pusta, a gdy jest to warstwa tła (bez przezroczystości), pokazuje ostrzeżenie z instrukcją (patrz 6.3).
2. **Filtr nie ustawia trybu mieszania ani krycia warstwy w Photoshopie.** Te cechy (Multiply, Screen, Overlay, Soft Light, krycie warstwy itd.) ustawia użytkownik natywnie w panelu Warstwy na warstwie z podpisem i Photoshop sam je stosuje. To daje dokładnie to, o co chodzi („cechy warstwy z Photoshopa"), bez udawania ich we wtyczce. Wtyczka ma własny suwak „Widoczność 0–100%", który mnoży alfę podpisu (niezależnie od krycia warstwy).
3. **Tryb awaryjny: mieszanie wewnątrz wtyczki.** Dla przypadku, gdy ktoś chce nałożyć podpis bezpośrednio na warstwę ze zdjęciem (bez dodatkowej warstwy), wtyczka ma opcję „Zastosuj bezpośrednio na tej warstwie" z własną listą trybów mieszania (Normalny, Mnożenie, Ekran, Nakładka, Łagodne światło, Ostre światło, Rozjaśnianie, Ściemnianie, Różnica). To jest destrukcyjne i opisane w UI jako takie.

Wymagane sprawdzenie w SDK przed implementacją (nie zgadywać, odczytać z dokumentacji i przykładów `Filter`/`Dissolve`/`Outline` w SDK):
- Filter cases w PiPL: obsłużyć przypadki „Editable transparency" (z zaznaczeniem i bez), tak aby wtyczka mogła zapisywać do kanału alfa warstwy. Pola `inLayerPlanes`, `inTransparencyMask`, `outLayerPlanes`, `outTransparencyMask` w `FilterRecord`.
- Skąd wziąć kolor pierwszoplanowy: `FilterRecord.foreColor` / `backColor` (sprawdzić format wartości zależny od trybu obrazu).
- Czy dostępny jest sposób odczytu motywu interfejsu Photoshopa (jasny/ciemny) z poziomu filtra. Jeśli nie ma, użyć awaryjnie ustawienia Windows (`AppsUseLightTheme` w rejestrze).

## 3. Środowisko i budowanie

- Język: C++17.
- Kompilator: Visual Studio 2022 (lub Build Tools), platforma x64, konfiguracja Release.
- Adobe Photoshop SDK: **Marek pobiera ręcznie** z Adobe Developer (wymaga konta Adobe i akceptacji licencji; Claude Code nie może go pobrać). Ścieżka SDK ma być w jednej zmiennej projektu (`PS_SDK_DIR`).
- UI: czyste Win32 (dialog + własne rysowanie kontrolek pod motyw jasny/ciemny). Bez Qt i innych dużych zależności.
- Renderowanie tekstu i grafiki: GDI+ lub DirectWrite/Direct2D do bitmapy ARGB, potem kopiowanie do bufora warstwy. Wczytywanie obrazów: WIC (Windows Imaging Component).
- Wynik: `ZetmarWatermark_64.8bf`.
- Struktura repozytorium: `src/`, `res/` (PiPL, .rc, ikony), `docs/`, `tests/`, `CMakeLists.txt` lub `.sln`, `README.md` z instrukcją budowania.
- Zasoby PiPL: kategoria „Zetmar", nazwa „Podpis (Watermark)…". PiPL jest statyczny, więc nazwa w menu nie zmienia się wraz z językiem interfejsu.

Instalacja (do README):
- Photoshop: `C:\Program Files\Adobe\Adobe Photoshop <wersja>\Plug-ins\`
- Elements: `C:\Program Files\Adobe\Photoshop Elements <wersja>\Plug-Ins\`
- Menu: Filtr → Zetmar → Podpis (Watermark)…

## 4. Obsługiwane tryby obrazu

- RGB 8 bit na pewno, RGB 16 bit w Photoshopie.
- Tryby, których wtyczka nie obsługuje (CMYK, Lab, Grayscale, 32 bit), mają być wyłączone w PiPL (`SupportedModes`), aby Photoshop sam wyszarzył filtr.

## 5. Funkcje

### 5.1 Podpis tekstowy
- Pole tekstu (wielowiersz, Unicode, polskie znaki).
- Czcionka: lista wszystkich zainstalowanych czcionek systemowych (z podglądem w liście), styl pogrubiony/kursywa jeśli czcionka je ma.
- Rozmiar: w pikselach, 4–2000, domyślnie względny do szerokości zdjęcia (np. 5% szerokości), z przełącznikiem px / % szerokości.
- Kolor: pole koloru. **Domyślnie kolor pierwszoplanowy z Photoshopa/PSE** (przy każdym otwarciu). Przycisk „Użyj koloru pierwszoplanowego" i własny wybór (systemowy `ChooseColor`).
- Odstęp między literami i interlinia (opcjonalnie, wartości domyślne neutralne).

### 5.2 Podpis graficzny
- Wybór pliku: PNG (z alfa), JPG, TIFF. Ostatnia ścieżka zapamiętana.
- Skala 1–400% lub „% szerokości zdjęcia", proporcje zablokowane.
- Obrót -180° do 180° i odbicie poziome/pionowe.
- Opcja „Przekoloruj na wybrany kolor" (obraz traktowany jako maska, np. czarno-biały znak wodny zamieniany na kolor pierwszoplanowy).
- Ten sam obrys/płaskorzeźba co w tekście (5.4) działa na kształcie z kanału alfa obrazu.

### 5.3 Pozycja (9 punktów)
- Siatka 3×3: góra-lewo, góra-środek, góra-prawo, środek-lewo, środek, środek-prawo, dół-lewo, dół-środek, dół-prawo.
- Margines od krawędzi (px lub % krótszego boku), osobno X i Y.
- Podpis zawsze liczony względem **całego dokumentu** (rozmiar warstwy), nie względem zaznaczenia. Jeśli jest zaznaczenie, pozycja liczona względem zaznaczenia (przełącznik w UI).
- Opcjonalnie dokładne przesunięcie (offset X/Y) na bazie wybranego punktu.

### 5.4 Obrys i efekt rzeźby
Efekty liczone przez wtyczkę na kształcie podpisu (alfa), zapisywane do tej samej warstwy:
- **Obrys (stroke):** kolor, grubość 0–100 px, położenie (zewnątrz / środek / wewnątrz), krycie.
- **Płaskorzeźba / rzeźba wypukła (emboss/bevel):**
  - Styl: Płaskorzeźba (emboss), Płaskorzeźba poduszkowa (pillow emboss), Fazowanie zewnętrzne, Fazowanie wewnętrzne.
  - Kierunek: wypukły / wklęsły.
  - Głębokość, rozmiar, zmiękczenie.
  - Kąt światła 0–360° i wysokość światła.
  - Kolor i krycie światła oraz cienia (domyślnie biały/czarny).
- Obrys i rzeźba mogą być włączone razem, niezależnie od siebie (dwa osobne przełączniki).
- Implementacja rzeźby: mapa wysokości z rozmytej alfy (Gaussian/distance transform), oświetlenie z gradientu (normalne), składanie światła i cienia na obrazie podpisu. Działa na 8 i 16 bitach.

### 5.5 Widoczność
- Suwak i pole liczbowe 0–100% (krok 1). Mnoży końcową alfę podpisu (łącznie z obrysem i rzeźbą).
- To oddzielne od krycia warstwy w panelu Warstwy (patrz 2.2).

### 5.6 Tryb mieszania wewnętrzny (tylko tryb awaryjny z 2.3)
- Aktywny tylko po zaznaczeniu „Zastosuj bezpośrednio na tej warstwie".
- Lista trybów jak w 2.3. Wzory zgodne z Photoshopem (porównać wynik z Photoshopem na testowych obrazach, tolerancja ±1 na 8 bitach).

## 6. Interfejs

### 6.1 Okno
- Jedno okno modalne, rozmiar zmienny, podgląd po lewej, panel ustawień po prawej.
- Zakładki: Typ (tekst/grafika), Pozycja, Efekty (obrys/rzeźba), Widoczność, Ustawienia (język, presety).
- Podgląd na żywo: zdjęcie pomniejszone do okna podglądu z narysowanym podpisem, aktualizacja przy każdej zmianie (z krótkim opóźnieniem ~50 ms). Zoom podglądu i przesuwanie.
- Przyciski: OK, Anuluj, Resetuj.

### 6.2 Kolory interfejsu
Interpretacja dwuznaczna, obsłużyć obie:
1. **Kolor podpisu:** domyślnie z koloru pierwszoplanowego Photoshopa/PSE (5.1).
2. **Motyw okna:** jasny/ciemny dopasowany do Photoshopa, a jeśli SDK tego nie udostępnia, do ustawień Windows (patrz 2). Użytkownik może wymusić motyw w zakładce Ustawienia.

### 6.3 Komunikaty
- Aktywna warstwa to tło bez przezroczystości: komunikat z wyborem „Anuluj" / „Zastosuj bezpośrednio na tej warstwie" i podpowiedzią „Utwórz pustą warstwę: Ctrl+Shift+N, potem uruchom filtr ponownie".
- Brak pliku obrazu podpisu, pusty tekst, nieobsługiwany tryb obrazu: czytelny komunikat, bez crasha.

### 6.4 Język
- Dwa języki: polski i angielski, przełącznik w zakładce Ustawienia, wybór zapamiętany.
- Przy pierwszym uruchomieniu język z ustawień Windows (pl → polski, reszta → angielski).
- Wszystkie napisy w jednym pliku zasobów (tabela `klucz → PL/EN`), żadnych stałych tekstów w kodzie UI. Dodanie języka = dodanie kolumny.
- Obsługa Unicode (UTF-16 w UI).

## 7. Ustawienia i presety

- Ustawienia ostatniego użycia zapisywane w `%APPDATA%\ZetmarWatermark\settings.json`.
- Zapis i wczytanie presetów (np. „Podpis jasny dół-prawo", „Logo środek 30%"), plik JSON na preset.
- Wsparcie „Powtórz ostatni filtr" (Ctrl+F) w Photoshopie: parametry w uchwycie parametrów filtra (`FilterRecord.parameters`), bez pokazywania okna przy `filterSelectorParameters` wg konwencji SDK.
- Opcjonalnie (wersja 2): obsługa Action Manager, żeby filtr dało się nagrywać w Akcjach Photoshopa (w PSE nie działa, bo PSE nie ma akcji).

## 8. Wydajność i jakość

- Zdjęcia do 100 MP bez błędu pamięci. Renderowanie podpisu tylko w obszarze podpisu plus margines efektów, nie na całym obrazie.
- Obsługa przetwarzania kafelkami zgodnie z SDK (`filterSelectorStart/Continue/Finish`), jeśli host dzieli obraz.
- Antyaliasing tekstu i skalowania (bicubic/Lanczos dla obrazu).
- Premultiplied vs straight alpha: zachować poprawną alfę na krawędziach, bez jasnych/ciemnych halo.

## 9. Testy i kryteria odbioru

Testy ręczne (macierz) w Photoshopie i Elementsie:
- RGB 8 bit i 16 bit (Photoshop), dokument pionowy, poziomy, kwadratowy, mały (800 px) i duży (>10 000 px).
- Pusta warstwa, warstwa z treścią, warstwa tła, z zaznaczeniem i bez.
- Wszystkie 9 pozycji, wszystkie style rzeźby, obrys w 3 położeniach.
- Widoczność 0%, 50%, 100%.
- Zmiana trybu mieszania i krycia warstwy w panelu Warstwy po zastosowaniu filtra.
- Przełączenie PL/EN, motyw jasny/ciemny, kolor z palety pierwszoplanowej.
- Ctrl+F (powtórz filtr), Cofnij (Ctrl+Z) jednym krokiem.

Kryteria odbioru:
1. Wtyczka ładuje się w Photoshopie i PSE bez błędu i pojawia się w menu Filtr → Zetmar.
2. Podpis ląduje wyłącznie na aktywnej warstwie, reszta dokumentu bez zmian.
3. Wszystkie opcje z punktu 5 działają, widoczność dokładnie skaluje alfę.
4. Interfejs w dwóch językach, bez brakujących lub ucinanych napisów.
5. Brak crashy przy nieprawidłowych danych wejściowych (zły plik, pusty tekst, brak czcionki).
6. Wynik ma być bit-w-bit powtarzalny dla tych samych ustawień.

## 10. Zakres poza wersją 1

macOS (`.plugin`), cień (drop shadow), wzór kafelkowany na całym zdjęciu, przetwarzanie wsadowe (do tego służą Akcje w Photoshopie), licencjonowanie.

## 11. Kolejność prac dla Claude Code

1. Szkielet wtyczki wg przykładu SDK, PiPL, kompilacja do `.8bf`, test ładowania w Photoshopie i PSE (puste okno, „Hello").
2. Odczyt warstwy, zapis do kanału alfa na pustej warstwie (prosty prostokąt), pozycje 3×3.
3. Podpis tekstowy (czcionka, rozmiar, kolor z palety), widoczność.
4. Podpis graficzny (WIC), skala, obrót.
5. Obrys i płaskorzeźba.
6. Pełny interfejs z podglądem, motyw, PL/EN, presety.
7. Tryb awaryjny z mieszaniem, komunikaty, testy macierzowe, README.

Po każdym kroku Claude Code ma zbudować wtyczkę i podać Markowi instrukcję testu w Photoshopie/PSE. Claude Code nie ma dostępu do Photoshopa, więc testy w aplikacji wykonuje Marek.
