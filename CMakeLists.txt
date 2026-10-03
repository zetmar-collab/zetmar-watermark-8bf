cmake_minimum_required(VERSION 3.20)
project(ZetmarWatermark LANGUAGES CXX RC)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# Statyczny CRT: wtyczka nie moze zalezec od redystrybuowalnych DLL-i obecnych/nieobecnych u uzytkownika.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")

# Sciezka do Adobe Photoshop SDK (katalog zawierajacy photoshopapi/ i samplecode/).
set(PS_SDK_DIR "${CMAKE_SOURCE_DIR}/sdk/pluginsdk" CACHE PATH "Adobe Photoshop SDK (pluginsdk)")

add_compile_definitions(UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX _CRT_SECURE_NO_WARNINGS)
add_compile_options(/utf-8 /EHsc /W3 /fp:precise /wd4828)

# ---- Rdzen (bez zaleznosci od Photoshopa): rendering, efekty, parametry
add_library(wmcore STATIC
    src/core/params.cpp
    src/core/imageops.cpp
    src/core/blend.cpp
    src/core/effects.cpp
    src/core/compose.cpp
    src/core/i18n.cpp
    src/core/settings.cpp
    src/platform/rasterizer_win.cpp
)
target_include_directories(wmcore PUBLIC src)
target_link_libraries(wmcore PUBLIC gdiplus windowscodecs ole32 shell32)

# ---- Testy
add_executable(wm_tests tests/core_tests.cpp)
target_link_libraries(wm_tests PRIVATE wmcore)

# ---- Wtyczka .8bf
option(WM_BUILD_PLUGIN "Buduj wtyczke .8bf" ON)
if(WM_BUILD_PLUGIN)
add_library(ZetmarWatermark SHARED
    src/plugin/plugin_main.cpp
    src/plugin/apply.cpp
    src/ui/dialog.cpp
    src/ui/theme.cpp
    src/ui/msgbox.cpp
    res/plugin.rc
)
set_target_properties(ZetmarWatermark PROPERTIES
    OUTPUT_NAME "ZetmarWatermark_64"
    SUFFIX ".8bf"
    PREFIX ""
)
target_include_directories(ZetmarWatermark PRIVATE
    src
    res
    ${PS_SDK_DIR}/photoshopapi/photoshop
    ${PS_SDK_DIR}/photoshopapi/pica_sp
    ${PS_SDK_DIR}/samplecode/common/includes
)
target_link_libraries(ZetmarWatermark PRIVATE wmcore comctl32 comdlg32 uxtheme dwmapi shlwapi)

# Harness do zrzutow interfejsu (bez Photoshopa)
add_executable(wm_ui_harness tests/ui_harness.cpp src/ui/dialog.cpp src/ui/theme.cpp src/ui/msgbox.cpp src/plugin/apply.cpp)
target_include_directories(wm_ui_harness PRIVATE src ${PS_SDK_DIR}/photoshopapi/photoshop ${PS_SDK_DIR}/photoshopapi/pica_sp ${PS_SDK_DIR}/samplecode/common/includes)
target_link_libraries(wm_ui_harness PRIVATE wmcore comctl32 comdlg32 uxtheme dwmapi shlwapi)
endif()
