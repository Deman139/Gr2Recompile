# Building Gravity Rush 2 for PC

Players do not need this: the installer from the Releases page has everything. This is for
building the port, the launcher and the installer from the source.

> **Кратко по-русски.** Поставь [MSYS2](https://www.msys2.org), открой **MSYS2 CLANG64** и выполни
> команды из шагов 1–2 ниже. Для инсталлера один раз выполни в PowerShell команду из шага 3, затем
> `bash packaging/windows/package_gr2.sh`. DLSS (необязательно) собирается шагом 4.

## Everything comes from GitHub

Nothing has to be on the computer beforehand except MSYS2 (and, for the installer, a Windows
Python and Inno Setup):

| What | Where it comes from |
|---|---|
| The port (runtime, loader, launcher, scripts) | this repository |
| shadPS4's video core (with this port's changes) | this repository, `gpu/shadps4/` |
| FSR-Vulkan, Dear ImGui, magic_enum, miniz, xbyak, LibAtrac9 | git submodules, fetched by `build.sh` |
| Compiler, CMake, Vulkan, SDL3, FFmpeg, Boost, SPIR-V tools, ... | MSYS2 packages (step 1) |
| NVIDIA DLSS SDK (optional) | fetched from [NVIDIA/DLSS](https://github.com/NVIDIA/DLSS) by `build_dlss.sh` |
| FSR 4 assets (optional, port menu) | fetched by `tools/fetch_fsr4_assets.sh` |

No shadPS4 or GR2fork install is needed. The game, its update, DLC and the PS4 system modules are
never part of the build: each player uses their own.

## 1. Tools and libraries (once)

In the **MSYS2 CLANG64** shell:

```bash
pacman -S --needed git patch \
  mingw-w64-clang-x86_64-{toolchain,cmake,ninja,pkgconf,python,tk,python-pillow} \
  mingw-w64-clang-x86_64-{vulkan-headers,vulkan-loader,vulkan-memory-allocator,glslang} \
  mingw-w64-clang-x86_64-{sdl3,fmt,boost,robin-map,xxhash,ffmpeg,zydis} \
  mingw-w64-clang-x86_64-{spirv-tools,spirv-headers,spirv-cross}
git clone --recursive https://github.com/Deman139/Gr2Recompile.git gr2-pc
cd gr2-pc
```

## 2. Build

```bash
bash build.sh
```

It makes the game executable (`out/gr2-probe.exe`) and **`Gravity Rush 2.exe`** in the project
folder, which opens the launcher (with the MSYS2 Python, no console window). To update later:
`git pull` and `bash build.sh` again.

The launcher finds the game in `game/`, the modules in `sys_modules/` and DLC in `game/addcont/`
(see the [README](../README.md)). Without the launcher:

```bash
GR2_GAME=/d/Games/CUSA04943 GR2_SYS_MODULES=/d/ps4/sys_modules bash gr2.sh
```

## 3. Installer and portable package

Once, in PowerShell:

```powershell
winget install JRSoftware.InnoSetup Python.Python.3.12
```

Then in MSYS2 CLANG64, after `bash build.sh`:

```bash
bash packaging/windows/package_gr2.sh
```

`dist/` gets `GravityRush2-Setup-v<version>.exe` (the installer, `packaging/windows/gr2_setup.iss`),
`GravityRush2-v<version>-windows.zip` and `Gravity Rush 2/` (portable). The launcher is frozen
with PyInstaller, so players need neither MSYS2 nor Python. The package refuses to build if any game
file, module, save or setting got into it.

The installer installs per user without administrator rights (default
`%LOCALAPPDATA%\Programs\Gravity Rush 2`; Program Files is refused because the launcher writes next
to itself), makes shortcuts, and its uninstaller keeps the player's game, DLC and modules and asks
before removing saves and settings.

## 4. DLSS (optional)

The DLSS bridge is built with MSVC. Once, in PowerShell:

```powershell
winget install Microsoft.VisualStudio.2022.BuildTools --override "--wait --passive --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended"
```

Then in MSYS2 CLANG64:

```bash
bash packaging/windows/build_dlss.sh
```

It fetches the DLSS SDK, builds `out/gr2_dlss.dll` and copies NVIDIA's `nvngx_dlss.dll` and license
next to the game; `package_gr2.sh` ships all three.

## Optional extras

- FSR 4 (port menu): `bash tools/fetch_fsr4_assets.sh` downloads its assets into `fsr4_shaders/`.
- Profile-guided build: `GR2_PGO=generate bash build.sh`, play a while with `out/gpu-pgo/gr2-probe.exe`,
  then `GR2_PGO=use bash build.sh` (later builds use `pgo/gr2.profdata` by themselves).

Notes for changing the port itself: [development.md](development.md).
