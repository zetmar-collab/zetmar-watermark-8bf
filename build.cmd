@echo off
rem Buduje wtyczke (Release x64). Uzycie: build.cmd [cel]   np. build.cmd wm_tests
setlocal
set "VCVARS=%ProgramFiles(x86)%\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
if not exist "%VCVARS%" set "VCVARS=%ProgramFiles%\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
call "%VCVARS%" >nul || exit /b 1
set "CMAKE=%ProgramFiles(x86)%\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
if not exist "%CMAKE%" set "CMAKE=cmake"
cd /d "%~dp0"
"%CMAKE%" -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release %WM_CMAKE_ARGS% || exit /b 1
if "%~1"=="" ("%CMAKE%" --build build) else ("%CMAKE%" --build build --target %~1)
