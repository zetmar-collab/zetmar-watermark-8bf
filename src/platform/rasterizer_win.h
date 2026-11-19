// Windows: tekst przez GDI+, obrazy przez WIC.
#pragma once
#include <memory>
#include <string>
#include <vector>

#include "core/compose.h"

namespace wm {

class WinRasterizer : public Rasterizer {
public:
    WinRasterizer();
    ~WinRasterizer() override;

    bool RenderText(const TextRequest& req, FImage& out, std::wstring& err) override;
    std::shared_ptr<const FImage> LoadImageShared(const std::wstring& path, std::wstring& err) override;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// Zainstalowane czcionki (bez duplikatow, bez nazw zaczynajacych sie od '@'), posortowane.
std::vector<std::wstring> ListInstalledFonts();
// Czy rodzina ma dany styl (pogrubienie / kursywa). Nieznana rodzina => false.
bool FontHasStyle(const std::wstring& family, bool bold, bool italic);

}  // namespace wm
