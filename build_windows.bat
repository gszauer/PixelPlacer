@echo off
REM PixelPlacer Windows Build Script
REM Usage: build_windows.bat [debug]
REM
REM === BUILD INSTRUCTIONS ===
REM
REM Option 1: Visual Studio (MSVC)
REM   1. Install Visual Studio 2019 or later with "Desktop development with C++"
REM   2. Open "x64 Native Tools Command Prompt for VS 2022" (or VS 2019)
REM   3. Navigate to this directory
REM   4. Run: build_windows.bat
REM   5. For debug build: build_windows.bat debug
REM
REM Option 2: MinGW-w64
REM   1. Install MSYS2 from https://www.msys2.org/
REM   2. In MSYS2 terminal run: pacman -S mingw-w64-x86_64-gcc
REM   3. Add C:\msys64\mingw64\bin to your PATH environment variable
REM   4. Open Command Prompt
REM   5. Navigate to this directory
REM   6. Run: build_windows.bat
REM
REM === END INSTRUCTIONS ===

setlocal

cd /d "%~dp0"

REM Clean previous build
if exist pixelplacer.exe del pixelplacer.exe
if exist pixelplacer_debug.exe del pixelplacer_debug.exe
if exist *.obj del *.obj
if exist *.pdb del *.pdb

REM Check for Visual Studio compiler
where cl >nul 2>&1
if %ERRORLEVEL%==0 (
    echo Building with MSVC...
    goto :msvc_build
)

REM Check for MinGW compiler
where g++ >nul 2>&1
if %ERRORLEVEL%==0 (
    echo Building with MinGW...
    goto :mingw_build
)

echo ERROR: No C++ compiler found.
echo.
echo Please install one of the following:
echo.
echo 1. Visual Studio:
echo    - Install Visual Studio 2019 or later
echo    - Include "Desktop development with C++" workload
echo    - Run this script from "x64 Native Tools Command Prompt for VS"
echo.
echo 2. MinGW-w64:
echo    - Install MSYS2 from https://www.msys2.org/
echo    - Run: pacman -S mingw-w64-x86_64-gcc
echo    - Add C:\msys64\mingw64\bin to PATH
echo.
exit /b 1

:msvc_build
if "%1"=="debug" (
    echo Building debug version...
    cl /std:c++17 /EHsc /nologo /Zi /Od /DUNITY_BUILD /D_WIN32 ^
        /W3 /wd4244 /wd4267 ^
        code\main.cpp ^
        user32.lib gdi32.lib shell32.lib comdlg32.lib ole32.lib winmm.lib ^
        /Fe:pixelplacer_debug.exe /Fd:pixelplacer_debug.pdb
    if %ERRORLEVEL%==0 (
        echo Built: pixelplacer_debug.exe
    ) else (
        echo Build failed!
        exit /b 1
    )
) else (
    echo Building release version...
    cl /std:c++17 /EHsc /nologo /O2 /DNDEBUG /DUNITY_BUILD /D_WIN32 ^
        /W3 /wd4244 /wd4267 ^
        code\main.cpp ^
        user32.lib gdi32.lib shell32.lib comdlg32.lib ole32.lib winmm.lib ^
        /Fe:pixelplacer.exe
    if %ERRORLEVEL%==0 (
        echo Built: pixelplacer.exe
    ) else (
        echo Build failed!
        exit /b 1
    )
)
goto :done

:mingw_build
if "%1"=="debug" (
    echo Building debug version...
    g++ -std=c++17 -g -O0 -DUNITY_BUILD -D_WIN32 -Wall -Wextra ^
        -Wno-unused-parameter ^
        code/main.cpp ^
        -lgdi32 -luser32 -lshell32 -lcomdlg32 -lole32 -lwinmm ^
        -mwindows ^
        -o pixelplacer_debug.exe
    if %ERRORLEVEL%==0 (
        echo Built: pixelplacer_debug.exe
    ) else (
        echo Build failed!
        exit /b 1
    )
) else (
    echo Building release version...
    g++ -std=c++17 -O3 -DNDEBUG -DUNITY_BUILD -D_WIN32 ^
        -Wno-unused-parameter ^
        code/main.cpp ^
        -lgdi32 -luser32 -lshell32 -lcomdlg32 -lole32 -lwinmm ^
        -mwindows ^
        -o pixelplacer.exe
    if %ERRORLEVEL%==0 (
        echo Built: pixelplacer.exe
    ) else (
        echo Build failed!
        exit /b 1
    )
)

:done
echo Done!
