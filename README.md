# Gravity Rush 2 for PC

A Windows recompilation of **Gravity Rush 2** (PS4). The game's code runs directly on the CPU, the
PS4 system libraries are replaced, and the [shadPS4](https://github.com/shadps4-emu/shadPS4)
emulator's video core translates the graphics to Vulkan. No game files are included: you need your
own PS4 dump.

> This project is not affiliated with Sony Interactive Entertainment. Gravity Rush is a trademark
> of Sony Interactive Entertainment.

## Download and play

1. Get `GravityRush2-Setup-v<version>.exe` from [Releases](https://github.com/Deman139/Gr2Recompile/releases)
   and install it (no administrator rights; not into Program Files).
2. Copy into the install folder:
   - **`game\`**: your Gravity Rush 2 dump (CUSA04943) with update **01.11**: `eboot.bin`,
     `sce_module`, `sce_sys` and the `.psarc` files;
   - **`sys_modules\`**: the system modules from your PS4 (`.sprx`): `libSceNgs2.sprx` is required,
     `libSceJson2`, `libSceRtc`, `libSceFreeTypeOt`, `libSceFontFt`, `libSceFont` are recommended;
   - **`game\addcont\`**: your DLC (`.pkg`), if any: costumes and *The Ark of Time – Raven's Choice*.
3. Start **Gravity Rush 2** from the Start menu and press Play.

You can also point the launcher at your own folders instead of copying.

**Requirements:** Windows 10/11, a Vulkan 1.3 graphics card, 16 GB RAM.

## What's included

- Launcher in English and Russian: game folders, graphics, controls, advanced options.
- 60 FPS and up to 4K (Junmin Lee's patches), performance patches for crowded areas.
- Remappable keyboard and mouse; Xbox, DualShock 4 and DualSense controllers with gyro and light
  bar; Steam Input.
- In-game port menu (F12, R3 + L2 on a controller), screenshots (F11), FSR 1 and sharpening, DLSS on RTX.
- DLC from `game\addcont` (the same layout shadPS4 uses).
- Save backups before every launch.

## Known issues

- First launch and new areas stutter while shaders compile; the shader cache grows.
- "Prepare cached shaders" can cause graphical artifacts (off by default).
- At 60 FPS the game slows down when the PC cannot hold 60 FPS.
- The comic character outline can draw stray black lines: turn it off then.
- Saving photos in photo mode does not work yet.
- PSN features are unavailable: the game runs offline.

## Reporting bugs

Open an [issue](https://github.com/Deman139/Gr2Recompile/issues): what you did, what happened, your
graphics card, and the logs from `user\logs\<date_time>\` (`game.log`, `errors.log`; the launcher's
Logs button on the Game tab opens them).

## Building from source

See [docs/BUILDING.md](docs/BUILDING.md).

---

## Credits

- [Killzone Shadow Fall PC port](https://github.com/lesca-191204/killzone-shadowfall-pc), the base of this port,
  and the projects it builds on: [gow3_pc](https://github.com/FelipePrado0/gow3_pc),
  [bloodborne_pc](https://github.com/deadinside28/bloodborne_pc).
- The [shadPS4](https://github.com/shadps4-emu/shadPS4) team: the video core and much of the system emulation.
- Junmin Lee ([GR2fork](https://github.com/junminlee2004/GR2fork)): the 60 FPS, resolution and
  performance patches, and the reference for the game's behaviour.
- [HayatoG/gr2_pc](https://github.com/HayatoG/gr2_pc): the comic outline finding.
- [FSR-Vulkan](https://github.com/FireBurn/FSR-Vulkan), [Dear ImGui](https://github.com/ocornut/imgui),
  [LibAtrac9](https://github.com/Thealexbarney/LibAtrac9), NVIDIA DLSS.

## License

GPL-2.0-or-later, like the projects it builds on. Third-party components keep their own licenses
(NVIDIA DLSS: `NVIDIA-DLSS-LICENSE.txt` next to the game).
