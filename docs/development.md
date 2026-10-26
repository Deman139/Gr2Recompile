# Development notes

For people changing the port itself; players need none of this (see the [README](../README.md)).

## What differs from the Killzone Shadow Fall port it started from

| Part | Change |
|---|---|
| `scripts/link_modules.py` | Links decrypted firmware modules (`--system-dir`, `--system`) and modules from the game root (`sysdata/libSceSecure.prx`); plain ELF input; empty TLS segments get no module ID |
| `run.py` | Links `libc`, `Fios2`, `JobManager`, `NpToolkit`, `libSceSecure` and the firmware modules; `GR2_NATIVE_LIBS` leaves those libraries to the native code |
| `src/runtime_fiber.c` | `libSceFiber` (the game's job system), ported from shadPS4 |
| `src/runtime_hoststack.c` | Host functions called from small guest stacks (fiber contexts) run on a per-thread host stack (`GR2_HOST_STACK_KB`, default 1024): FFmpeg, the Vulkan driver and Windows need far more stack than the PS4 libraries and overwrote the game's heap |
| `src/runtime_eventflag.c` | Kernel event flags |
| `src/runtime_gr2.c` | Other kernel/libc functions; offline network, content and sharing services; photo JPEG and Audio3d unavailable for now |
| `src/runtime_services.c` | PSN dialogs and services answering as signed out |
| `vk_rasterizer.cpp` | Draws with adjacency (the comic outline geometry shader, mistranslated: black streaks) are skipped unless `GR2_GS_OUTLINE=1`; found by [HayatoG/gr2_pc](https://github.com/HayatoG/gr2_pc) |
| `scripts/patches.py` | Games without a built-in patch file (Gravity Rush 2) get the third-party XML patches in `user/patches` that name their title ID and version |
| `tools/check_imports.py` | Offline check that every import the host must serve is implemented |

## Checking the imports

The game imports about 855 PS4 system functions; those its own and the firmware modules do not
provide (about 715) the C runtime must implement. After changing the runtime, check offline that
none is missing (`$GAME`: the game folder, `$SYS`: the system modules folder):

```bash
python scripts/prepare.py "$GAME" --out out && python scripts/link_modules.py "$GAME" --out out \
  --modules libc.prx libSceFios2.prx libSceJobManager.prx libSceNpToolkit.prx sysdata/libSceSecure.prx \
  --system-dir "$SYS" --system libSceNgs2.sprx libSceJson2.sprx libSceRtc.sprx libSceFreeTypeOt.sprx libSceFontFt.sprx libSceFont.sprx
python tools/check_imports.py out/link.json --tolerate libSceFreeType
```

