# Gravity Rush 2 for PC (work in progress)

A Windows recompilation of **Gravity Rush 2** (PS4, CUSA04943 v01.11), built on the
[Killzone Shadow Fall PC port](https://github.com/lesca-191204/killzone-shadowfall-pc) (itself
based on [gow3_pc](https://github.com/FelipePrado0/gow3_pc) and
[bloodborne_pc](https://github.com/deadinside28/bloodborne_pc)): the game's x86-64 code runs
directly on the CPU, a C runtime replaces the PS4 system libraries, and the
[shadPS4](https://github.com/shadps4-emu/shadPS4) emulator's video core translates the graphics to
Vulkan. [GR2fork](https://github.com/junminlee2004/GR2fork) (shadPS4 for Gravity Rush 2) is the
reference for game-specific behaviour.

**Status: playable, work in progress** (60 FPS and up to 4K with the community patches; see below).
No game, DLC or firmware files are included.

## What you need

- Windows 10/11, a Vulkan 1.3 graphics card, 16 GB RAM.
- The game folder from your own PS4 (decrypted: `eboot.bin`, `sce_module`, `sce_sys`, the
  `.psarc` files), with the 01.11 update merged in.
- Decrypted firmware modules dumped from your console (the files shadPS4 takes in
  `user/sys_modules`): `libSceNgs2.sprx` (required: the game's audio engine), and
  `libSceJson2.sprx`, `libSceRtc.sprx`, `libSceFreeTypeOt.sprx`, `libSceFontFt.sprx`,
  `libSceFont.sprx`.
- Optional: your DLC (costumes, *The Ark of Time – Raven's Choice*) as `.pkg` files.

## Where the files go

Next to the launcher there are three folders, each with a `README.txt`:

| Folder | What to copy there |
|---|---|
| `game/` | The game (`eboot.bin`, `sce_module`, `sce_sys`, `.psarc`), or its whole folder (`game/CUSA04943/`) |
| `game/addcont/` | The DLC `.pkg` files: the launcher unpacks them into `addcont/CUSA04943/<label>/` and removes the packages |
| `sys_modules/` | The `.sprx` firmware modules |

A path typed or chosen in the launcher takes precedence. The DLC layout is the one shadPS4 reads,
so an emulator can share it (its add-on folder: `<game>/addcont`); already unpacked DLC can be
copied to `addcont/CUSA04943/<any folder>/` as they are. `GR2_DLC=0` starts without DLC.

## Launcher

`Gravity Rush 2.exe` (Russian and English): the game, update and modules folders, graphics options,
keyboard / mouse / controller mapping (DualShock 4 and DualSense gyro and light bar included),
shader and readback options, and shortcuts to saves, screenshots, logs and the cache. Its icon is
`launcher/gr2_launcher.ico` until the game folder is known, then the game's own.

Steam Input (Steam Controller, gyro to mouse, ...): add `Gravity Rush 2.exe` to Steam as a non-Steam
game and turn Steam Input on in its controller settings. It keeps running until the launcher closes,
so Steam keeps its virtual gamepad for the game.

### 60 FPS and higher resolutions

Launcher → Graphics → Frame rate → 60 FPS, and Game resolution → 2560 × 1440 or 3840 × 2160, use
Junmin Lee's patches for 01.11 (resolution: the game itself draws at that size; 4K adds the 128 MiB
of direct memory its note asks for, and the readback size limit grows with the pixel count), from
[GR2fork](https://github.com/junminlee2004/GR2fork) (its
[patch repository](https://github.com/junminlee2004/GoldHEN_Patch_Repository)). The launcher downloads
the patch file once into `user/patches` and selects the patches in `user/patches.json`; nothing is
bundled. At 60 FPS the game steps its time 1/60 s per frame: below 60 FPS it slows down. Any other
shadPS4/GoldHEN XML file in `user/patches` that names the game's title ID works the same way
(`patches.json`: `{"enabled": ["<file>/<patch name>"], "disabled": [...]}`).

## Building from source (MSYS2 CLANG64)

Only needed to build the port yourself; the installer needs none of it. Install
[MSYS2](https://www.msys2.org), then in the **MSYS2 CLANG64** shell install the compiler and the
libraries the port is built with, and fetch the bundled third-party sources:

```bash
pacman -S --needed git patch \
  mingw-w64-clang-x86_64-{toolchain,cmake,ninja,pkgconf,python,tk,python-pillow} \
  mingw-w64-clang-x86_64-{vulkan-headers,vulkan-loader,vulkan-memory-allocator,glslang} \
  mingw-w64-clang-x86_64-{sdl3,fmt,boost,robin-map,xxhash,ffmpeg,zydis} \
  mingw-w64-clang-x86_64-{spirv-tools,spirv-headers,spirv-cross}
git submodule update --init --recursive
```

Then build, and update later with `git pull` and the same command:

```bash
bash build.sh
```

It makes the game executable (`out/`) and **`Gravity Rush 2.exe`** in the project folder
(`packaging/windows/launcher_exe.c`), which starts the launcher with the MSYS2 CLANG64 Python
without a console window. Its icon is the game's (from `sce_sys/icon0.png` once the game folder
is known: rebuild after choosing it), else `launcher/gr2_launcher.ico`. Without the launcher:

```bash
GR2_GAME=/d/Games/CUSA04943 GR2_SYS_MODULES=/d/ps4/sys_modules bash gr2.sh
```

### Installer and portable package

`bash packaging/windows/package_gr2.sh` makes a build players can run without MSYS2 or Python:
`dist/Gravity Rush 2/` (portable), a zip of it and, with Inno Setup 6, the installer
`dist/GravityRush2-Setup-v<version>.exe` (`packaging/windows/gr2_setup.iss`). It needs a Windows
Python and Inno Setup, installed once from PowerShell:

```powershell
winget install JRSoftware.InnoSetup Python.Python.3.12
```

The installer needs no administrator rights (default folder `%LOCALAPPDATA%\Programs\Gravity Rush 2`;
Program Files is refused because the launcher writes next to itself), makes Start menu and desktop
shortcuts and an uninstaller that keeps the player's game, DLC and modules and asks before removing
saves and settings. No game files, DLC or system modules ship. DLSS ships when
`packaging/windows/build_dlss.sh` made it, with NVIDIA's license.

Notes for changing the port itself: [docs/development.md](docs/development.md).

This project is not affiliated with Sony Interactive Entertainment. Gravity Rush is a trademark
of Sony Interactive Entertainment. GPL-2.0-or-later, like the projects it builds on.
