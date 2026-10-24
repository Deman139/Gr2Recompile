# Gravity Rush 2 for PC (work in progress)

A native Windows port of **Gravity Rush 2** (PS4, CUSA04943 v01.11), built on the
[Killzone Shadow Fall PC port](https://github.com/lesca-191204/killzone-shadowfall-pc) (itself
based on [gow3_pc](https://github.com/FelipePrado0/gow3_pc) and
[bloodborne_pc](https://github.com/deadinside28/bloodborne_pc)): the game's x86-64 code runs
directly on the CPU, a C runtime replaces the PS4 system libraries, and the
[shadPS4](https://github.com/shadps4-emu/shadPS4) video core translates the graphics to Vulkan.
[GR2fork](https://github.com/junminlee2004/GR2fork) (shadPS4 for Gravity Rush 2) is the reference
for game-specific behaviour.

**Status: not playable yet.** The game image links and every imported function resolves; the
first runs are next. No game or firmware files are included.

## What you need

- The game folder from your own PS4 (decrypted: `eboot.bin`, `sce_module`, `sce_sys`, the
  `.psarc` files), with the 01.11 update merged in.
- Decrypted firmware modules dumped from your console (the files shadPS4 takes in
  `user/sys_modules`): `libSceNgs2.sprx` (required: the game's audio engine), and
  `libSceJson2.sprx`, `libSceRtc.sprx`, `libSceFreeTypeOt.sprx`, `libSceFontFt.sprx`,
  `libSceFont.sprx`.
- Windows 10/11, a Vulkan 1.3 GPU, 16 GB RAM; MSYS2 CLANG64 to build
  ([packaging/windows/README.md](packaging/windows/README.md)).

## Build and run (MSYS2 CLANG64)

```bash
bash build.sh
GR2_GAME=/d/Games/CUSA04943 \
GR2_SYS_MODULES=/d/ps4/sys_modules bash gr2.sh
```

The console output is also saved in `user/logs/gr2-<date>.log`.

## Launcher

`launcher/gr2_launcher.py` (Russian and English): the game folder (with an optional 01.11 update
folder), the system modules folder (found by itself in a shadPS4 install beside the game), port
graphics options, keyboard / mouse / controller mapping, readback and shader options, and
shortcuts to saves, screenshots, logs and the cache. The banner shows the dump's `sce_sys/pic1.png`,
or a picture of your choice (Game tab → Banner picture: a file, or a link the launcher downloads once
into `user/banner`). From the source tree, in MSYS2 CLANG64:

```bash
pacman -S --needed mingw-w64-clang-x86_64-tk mingw-w64-clang-x86_64-python-pillow
python launcher/gr2_launcher.py
```

A package players can run without MSYS2 or Python (`dist/Gravity Rush 2/Gravity Rush 2.exe`):
`bash packaging/windows/package_gr2.sh` (needs a python.org Python; the icon is made once from
your dump, as the script says).

### 60 FPS

Launcher → Graphics → Frame rate → 60 FPS uses Junmin Lee's 60 FPS patch for 01.11 from
[GR2fork](https://github.com/junminlee2004/GR2fork) (its
[patch repository](https://github.com/junminlee2004/GoldHEN_Patch_Repository)). The launcher downloads
the patch file once into `user/patches` and selects the patch in `user/patches.json`; nothing is
bundled. The game then steps its time 1/60 s per frame: below 60 FPS it slows down. Any other
shadPS4/GoldHEN XML file in `user/patches` that names the game's title ID works the same way
(`patches.json`: `{"enabled": ["<file>/<patch name>"], "disabled": [...]}`).

## What differs from the Killzone port

| Part | Change |
|---|---|
| `scripts/link_modules.py` | Links decrypted firmware modules (`--system-dir`, `--system`) and modules from the game root (`sysdata/libSceSecure.prx`); plain ELF input; empty TLS segments get no module ID |
| `run.py` | Links `libc`, `Fios2`, `JobManager`, `NpToolkit`, `libSceSecure` and the firmware modules; `GOW3_NATIVE_LIBS` leaves those libraries to the native code |
| `src/runtime_fiber.c` | `libSceFiber` (the game's job system), ported from shadPS4 |
| `src/runtime_hoststack.c` | Host functions called from small guest stacks (fiber contexts) run on a per-thread host stack (`GOW3_HOST_STACK_KB`, default 1024): FFmpeg, the Vulkan driver and Windows need far more stack than the PS4 libraries and overwrote the game's heap |
| `src/runtime_eventflag.c` | Kernel event flags |
| `src/runtime_gr2.c` | Other kernel/libc functions; offline network, content and sharing services; photo JPEG and Audio3d unavailable for now |
| `src/runtime_services.c` | PSN dialogs and services answering as signed out |
| `vk_rasterizer.cpp` | Draws with adjacency (the comic outline geometry shader, mistranslated: black streaks) are skipped unless `GOW3_GS_OUTLINE=1`; found by [HayatoG/gr2_pc](https://github.com/HayatoG/gr2_pc) |
| `scripts/patches.py` | Games without a built-in patch file (Gravity Rush 2) get the third-party XML patches in `user/patches` that name their title ID and version |
| `tools/check_imports.py` | Offline check that every import the host must serve is implemented |

Check the imports after changing the runtime:

```bash
python scripts/prepare.py "$GAME" --out out && python scripts/link_modules.py "$GAME" --out out \
  --modules libc.prx libSceFios2.prx libSceJobManager.prx libSceNpToolkit.prx sysdata/libSceSecure.prx \
  --system-dir "$SYS" --system libSceNgs2.sprx libSceJson2.sprx libSceRtc.sprx libSceFreeTypeOt.sprx libSceFontFt.sprx libSceFont.sprx
python tools/check_imports.py out/link.json --tolerate libSceFreeType
```

This project is not affiliated with Sony Interactive Entertainment. Gravity Rush is a trademark
of Sony Interactive Entertainment. GPL-2.0-or-later, like the projects it builds on.
