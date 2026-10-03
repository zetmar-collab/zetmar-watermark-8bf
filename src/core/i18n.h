// Napisy interfejsu: jedna tabela klucz -> PL/EN w i18n_strings.inc
// (dodanie jezyka = dodanie kolumny w tabeli i wpisu w enum Lang).
#pragma once

namespace wm {

enum class Lang { Polish = 0, English = 1 };

enum class Str {
#define X(id, pl, en) id,
#include "i18n_strings.inc"
#undef X
    Count
};

void SetLanguage(Lang l);
Lang GetLanguage();
// settingValue: 0 = auto (z Windows: pl -> polski, reszta -> angielski), 1 = polski, 2 = angielski
Lang ResolveLanguage(int settingValue);

const wchar_t* T(Str id);

}  // namespace wm
