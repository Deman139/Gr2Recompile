@echo off
rem Gravity Rush 2 for a Steam "non-Steam game" shortcut: like "Gravity Rush 2.bat", but this
rem window stays open until the launcher and the game are closed. Steam keeps Steam Input (its
rem virtual Xbox gamepad, gyro to mouse, ...) only while the program it started runs, and the
rem other .bat ends at once. MSYS2_ROOT overrides C:\msys64.
setlocal
if "%MSYS2_ROOT%"=="" set "MSYS2_ROOT=C:\msys64"
set "PATH=%MSYS2_ROOT%\clang64\bin;%PATH%"
cd /d "%~dp0"
if exist "%MSYS2_ROOT%\clang64\bin\pythonw.exe" (
    start "" /wait "%MSYS2_ROOT%\clang64\bin\pythonw.exe" launcher\gr2_launcher.py
) else if exist "%MSYS2_ROOT%\clang64\bin\python.exe" (
    "%MSYS2_ROOT%\clang64\bin\python.exe" launcher\gr2_launcher.py
) else (
    echo MSYS2 CLANG64 Python not found in %MSYS2_ROOT%\clang64\bin
    pause
)
