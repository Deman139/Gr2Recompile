@echo off
rem Starts the Gravity Rush 2 launcher from the source tree with the MSYS2 CLANG64 Python
rem (needs: pacman -S mingw-w64-clang-x86_64-tk mingw-w64-clang-x86_64-python-pillow).
rem MSYS2_ROOT overrides C:\msys64. A packaged build has its own "Gravity Rush 2.exe" instead.
setlocal
if "%MSYS2_ROOT%"=="" set "MSYS2_ROOT=C:\msys64"
set "PATH=%MSYS2_ROOT%\clang64\bin;%PATH%"
cd /d "%~dp0"
if exist "%MSYS2_ROOT%\clang64\bin\pythonw.exe" (
    start "" "%MSYS2_ROOT%\clang64\bin\pythonw.exe" launcher\gr2_launcher.py
) else if exist "%MSYS2_ROOT%\clang64\bin\python.exe" (
    "%MSYS2_ROOT%\clang64\bin\python.exe" launcher\gr2_launcher.py
) else (
    echo MSYS2 CLANG64 Python not found in %MSYS2_ROOT%\clang64\bin
    pause
)
