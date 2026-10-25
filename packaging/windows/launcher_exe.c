/* "Gravity Rush 2.exe" in the source tree: starts the launcher (launcher/gr2_launcher.py) with the
 * MSYS2 CLANG64 Python, without a console window, and waits until it closes, so Windows, Steam
 * and other front ends see one program running for as long as the launcher and the game are open.
 * The Python folder is the one build.sh found (GR2_PYTHON_DIR), else %MSYS2_ROOT%\clang64\bin,
 * else C:\msys64\clang64\bin. Arguments are passed on to the launcher. Built by build.sh. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wchar.h>

#ifndef GR2_PYTHON_DIR
#define GR2_PYTHON_DIR L""
#endif

static int find_python(wchar_t *bin, size_t size, wchar_t *python, size_t python_size) {
    wchar_t root[MAX_PATH];
    const wchar_t *candidates[3] = {GR2_PYTHON_DIR, NULL, L"C:\\msys64\\clang64\\bin"};
    if (GetEnvironmentVariableW(L"MSYS2_ROOT", root, MAX_PATH) && wcslen(root) + 16 < MAX_PATH) {
        wcscat(root, L"\\clang64\\bin");
        candidates[1] = root;
    }
    for (int i = 0; i < 3; ++i) {
        if (!candidates[i] || !*candidates[i]) continue;
        for (int w = 0; w < 2; ++w) { /* pythonw first: no console window */
            swprintf(python, python_size, L"%ls\\%ls", candidates[i], w ? L"python.exe" : L"pythonw.exe");
            if (GetFileAttributesW(python) != INVALID_FILE_ATTRIBUTES) {
                swprintf(bin, size, L"%ls", candidates[i]);
                return 1;
            }
        }
    }
    return 0;
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE previous, PWSTR arguments, int show) {
    (void)instance, (void)previous, (void)show;
    static wchar_t dir[32768], bin[32768], python[32768], line[32768], path[32768];
    DWORD length = GetModuleFileNameW(NULL, dir, 32768);
    if (!length || length >= 32768) return 1;
    wchar_t *slash = wcsrchr(dir, L'\\');
    if (slash) *slash = 0;

    if (!find_python(bin, 32768, python, 32768)) {
        MessageBoxW(NULL,
                    L"MSYS2 CLANG64 Python was not found (C:\\msys64\\clang64\\bin).\n\n"
                    L"Install it in the MSYS2 CLANG64 shell:\n"
                    L"pacman -S mingw-w64-clang-x86_64-python mingw-w64-clang-x86_64-tk "
                    L"mingw-w64-clang-x86_64-python-pillow\n\n"
                    L"or set MSYS2_ROOT to your MSYS2 folder.",
                    L"Gravity Rush 2", MB_OK | MB_ICONERROR);
        return 1;
    }
    /* The game and its DLLs (MSYS2 CLANG64 runtime) are found through PATH, as with the .bat. */
    DWORD used = GetEnvironmentVariableW(L"PATH", path, 32768);
    if (used < 32768) {
        static wchar_t joined[32768];
        swprintf(joined, 32768, L"%ls;%ls", bin, used ? path : L"");
        SetEnvironmentVariableW(L"PATH", joined);
    }
    swprintf(line, 32768, L"\"%ls\" \"%ls\\launcher\\gr2_launcher.py\"%ls%ls", python, dir,
             arguments && *arguments ? L" " : L"", arguments ? arguments : L"");

    STARTUPINFOW startup = {.cb = sizeof(startup)};
    PROCESS_INFORMATION process;
    if (!CreateProcessW(python, line, NULL, NULL, FALSE, 0, NULL, dir, &startup, &process)) {
        MessageBoxW(NULL, L"The launcher could not be started (launcher\\gr2_launcher.py).", L"Gravity Rush 2",
                    MB_OK | MB_ICONERROR);
        return 1;
    }
    CloseHandle(process.hThread);
    WaitForSingleObject(process.hProcess, INFINITE);
    DWORD code = 0;
    GetExitCodeProcess(process.hProcess, &code);
    CloseHandle(process.hProcess);
    return (int)code;
}
