#include "i18n.h"

#include <windows.h>

namespace wm {

namespace {

struct Entry { const wchar_t* text[2]; };

const Entry kTable[] = {
#define X(id, pl, en) {{pl, en}},
#include "i18n_strings.inc"
#undef X
};

static_assert(sizeof(kTable) / sizeof(kTable[0]) == (size_t)Str::Count, "tabela napisow niezgodna z enum Str");

Lang g_lang = Lang::Polish;

}  // namespace

void SetLanguage(Lang l) { g_lang = l; }
Lang GetLanguage() { return g_lang; }

Lang ResolveLanguage(int settingValue) {
    if (settingValue == 1) return Lang::Polish;
    if (settingValue == 2) return Lang::English;
    wchar_t name[LOCALE_NAME_MAX_LENGTH] = {0};
    if (GetUserDefaultLocaleName(name, LOCALE_NAME_MAX_LENGTH) > 0 && name[0] == L'p' && name[1] == L'l')
        return Lang::Polish;
    return Lang::English;
}

const wchar_t* T(Str id) {
    size_t i = (size_t)id;
    if (i >= (size_t)Str::Count) return L"";
    return kTable[i].text[(int)g_lang];
}

}  // namespace wm
