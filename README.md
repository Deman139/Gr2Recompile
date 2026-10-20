# Killzone Shadow Fall for PC

**English** | [Español](#español)

Native Windows port of **Killzone Shadow Fall** (PS4). The game's code runs directly on the CPU;
graphics are translated to Vulkan in real time. Public beta: the whole campaign is playable, and
it keeps improving with testers' reports.

> **No game files are included.** You need your own dumped copy of the game: **CUSA00190,
> version 01.81**. This project is not affiliated with Sony Interactive Entertainment or Guerrilla
> Games. Killzone is a trademark of Sony Interactive Entertainment.

## Download and play

1. Download the latest zip from [Releases](../../releases) and unzip it anywhere (outside
   "Program Files").
2. Open **Killzone Shadow Fall.exe**, choose your dump's folder (the one holding `eboot.bin`) and
   press **PLAY**.

Requirements: Windows 10 (1903+) or 11 64-bit, a Vulkan 1.3 GPU, 16 GB of RAM. **6 GB of VRAM**
recommended for 1080p-1440p; 8 GB for 1800p and 4K.

Tested on a Ryzen 7 5700 + RTX 2060 Super (8 GB): the game should be playable from start to
finish. Shader compilation barely stutters, even on a first play, and levels load fast. Not
tested yet on AMD GPUs or Intel CPUs.

## What's included

- **Resolutions above 1080p with AMD FSR 3.1 or NVIDIA DLSS** (1440p, 1800p, 4K): the game renders
  at 1080p and the upscaler takes it to your resolution, while the HUD, menus and videos are drawn
  sharp at full resolution. Far fewer visual glitches than rendering the game itself at that
  resolution, and lighter. The classic method (a game patch) is still under *Advanced*.
- Launcher in English and Spanish: dump check, display mode, window size, render resolution and
  method, frame rate limit (30, 60, and experimental 144, 240 and unlimited), VSync, anisotropic
  filtering, distant texture sharpness, FSR 1, keyboard, mouse and controller mapping.
- Automatic updates: the launcher announces new versions and installs them keeping your saves.
- Mouse aim (as the right stick), keyboard, and Xbox / PlayStation controllers.
- Touchpad without one (OWL drone): hold **Delete** and drag the mouse.
- In-game menu with **Insert** (R3 + L2 on a controller); **F12** takes a screenshot.

## Known issues

- **FSR / DLSS:** a minor visual glitch can remain in some places (far fewer than with the classic
  method). The image is clean but not as sharp as it will be once jitter is added.
- **Classic method above 1080p:** some lighting flicker and a shadow band that can follow the
  camera may appear, and the few video cutscenes look zoomed in.
- **First play:** some effects can show up a few frames late while their shader compiles.
- **Above 60 FPS (experimental):** turn VSync off on a 60 Hz monitor, otherwise it stays at 60.
- The shader cache can take several GB ("Clear cache" in the launcher).
- Multiplayer: offline only (Botzone).

## Future plans

**High priority**
- Sharper FSR and DLSS (sub-pixel jitter).
- Frame generation (FSR 3 and DLSS).
- Performance: better use of the GPU and CPU.

**Medium priority**
- FSR 4.
- Debug menu, cheats and patches (for example unlocking every chapter).
- Fixes for bugs reported on specific hardware.

## Reporting bugs

Open an [issue](../../issues) with what happened, where (chapter), your GPU, CPU and driver, and
attach that session's folder from `user\logs\` (launcher **Logs** button).

## Building from source

As in the gow3_pc base ([instructions](packaging/windows/README.md)): MSYS2 CLANG64,
`bash build.sh`, then `bash packaging/windows/package_kzsf.sh` for the zip (first,
`packaging/windows/make_kzsf_icon.py <your dump>/sce_sys/icon0.png launcher/kzsf.ico`). For DLSS,
build the bridge first with Visual Studio 2022 and the [NVIDIA DLSS SDK](https://github.com/NVIDIA/DLSS):
`DLSS_SDK_ROOT=<checkout> bash packaging/windows/build_dlss_kzsf.sh`.

---

## Español

Port nativo para Windows de **Killzone Shadow Fall** (PS4). El código del juego corre directamente
en la CPU; los gráficos se traducen a Vulkan en tiempo real. Beta pública: se puede jugar la
campaña completa, y se sigue puliendo con los reportes de los testers.

> **No incluye archivos del juego.** Necesitás tu propia copia dumpeada del juego: **CUSA00190,
> versión 01.81**. Este proyecto no está afiliado a Sony Interactive Entertainment ni a Guerrilla
> Games. Killzone es una marca de Sony Interactive Entertainment.

## Descargar y jugar

1. Bajá el zip de la última versión en [Releases](../../releases) y descomprimilo donde quieras
   (fuera de "Archivos de programa").
2. Abrí **Killzone Shadow Fall.exe**, elegí la carpeta de tu dump (la que tiene `eboot.bin`) y
   dale a **JUGAR**.

Requisitos: Windows 10 (1903+) u 11 de 64 bits, GPU con Vulkan 1.3, 16 GB de RAM. Se recomiendan
**6 GB de VRAM** para 1080p-1440p; 8 GB para 1800p y 4K.

Probado en Ryzen 7 5700 + RTX 2060 Super (8 GB): el juego debería poder jugarse completo, de
principio a fin. La compilación de shaders casi no da tirones, aun en la primera partida, y los
niveles cargan rápido. Todavía no fue probado en gráficas AMD ni en procesadores Intel.

## Qué incluye

- **Resoluciones mayores a 1080p con AMD FSR 3.1 o NVIDIA DLSS** (1440p, 1800p, 4K): el juego
  renderiza en 1080p y el reescalador lo lleva a tu resolución, mientras el HUD, los menús y los
  videos se dibujan nítidos a resolución completa. Muchos menos errores visuales que haciendo que
  el juego mismo renderice a esa resolución, y más liviano. El método clásico (parche del juego)
  sigue disponible en *Avanzado*.
- Launcher en inglés y español: verificación del dump, modo de pantalla, tamaño de ventana,
  resolución y método de render, límite de FPS (30, 60, y experimentales 144, 240 y sin límite),
  VSync, filtrado anisotrópico, nitidez de texturas lejanas, FSR 1, controles de teclado, mouse y
  mando.
- Actualizaciones automáticas: el launcher avisa de versiones nuevas y las instala conservando tus partidas.
- Mouse para apuntar (como stick derecho), teclado y mandos Xbox / PlayStation.
- Touchpad sin touchpad (dron OWL): mantener **Suprimir** y arrastrar el mouse.
- Menú dentro del juego con **Insert** (R3 + L2 en el mando); **F12** saca una captura.

## Problemas conocidos

- **FSR / DLSS:** puede quedar algún error visual menor en ciertos lugares (muchos menos que con
  el método clásico). La imagen es limpia pero todavía no tan nítida como cuando se agregue el
  jitter.
- **Método clásico por encima de 1080p:** pueden aparecer parpadeos de iluminación y una franja de
  sombra que sigue a la cámara, y las pocas cinemáticas que son video se ven con zoom.
- **Primera partida:** algunos efectos pueden aparecer unos cuadros tarde mientras se compila su shader.
- **Más de 60 FPS (experimental):** con un monitor de 60 Hz desactivá VSync, si no se queda en 60.
- La caché de shaders puede ocupar varios GB ("Borrar caché" en el launcher).
- Multijugador: solo sin conexión (Botzone).

## Planes futuros

**Prioridad alta**
- FSR y DLSS más nítidos (jitter de subpíxel).
- Generación de cuadros (FSR 3 y DLSS).
- Rendimiento: mejor aprovechamiento de GPU y CPU.

**Prioridad media**
- FSR 4.
- Menú debug, trucos y parches (por ejemplo desbloquear todos los capítulos).
- Arreglos de errores reportados en hardware específico.

## Reportar errores

Abrí un [issue](../../issues) con qué pasó, dónde (capítulo), tu GPU, CPU y driver, y adjuntá la
carpeta de esa partida en `user\logs\` (botón **Logs** del launcher).

## Compilar desde el código

Igual que la base gow3_pc ([instrucciones](packaging/windows/README.md)): MSYS2 CLANG64,
`bash build.sh`, y después `bash packaging/windows/package_kzsf.sh` para el zip (antes,
`packaging/windows/make_kzsf_icon.py <tu dump>/sce_sys/icon0.png launcher/kzsf.ico`). Para DLSS,
compilá antes el puente con Visual Studio 2022 y el [SDK de NVIDIA DLSS](https://github.com/NVIDIA/DLSS):
`DLSS_SDK_ROOT=<checkout> bash packaging/windows/build_dlss_kzsf.sh`.

---

## Credits / Créditos

- **Lesca**: Killzone Shadow Fall port (dynamic module loader, PS4 clock fix, tessellation and
  texture fixes, fonts, resolution patches, VRAM reduction, shader stutter fix, FSR/DLSS
  calibration, launcher).
- **Felipe Prado**: [gow3_pc / SpartanPort](https://github.com/FelipePrado0/SpartanPort), the base
  of this port (its README is in [docs/gow3_pc](docs/gow3_pc/README.md)).
- **Supermedo** and **deadinside28**: [bloodborne_pc](https://github.com/deadinside28/bloodborne_pc),
  the original native port technique, and [uncharted_pc](https://github.com/Supermedo/uncharted_pc)
  (mouse input layer, file read, FPU state and texture cache fixes).
- **HayatoG**: [iss_pc](https://github.com/HayatoG/iss_pc) (shader permutation and cache fixes,
  driver pipeline cache, texture LOD bias).
- **shadPS4 team**: the [shadPS4](https://github.com/shadps4-emu/shadPS4) video core (GPU and
  shader translation to Vulkan).
- **AMD FidelityFX FSR 3.1** ([FSR-Vulkan](https://github.com/FireBurn/FSR-Vulkan)) and **NVIDIA
  DLSS** ([DLSS SDK](https://github.com/NVIDIA/DLSS); NVIDIA's license is in `licenses\`).

## License

GNU GPL v2 or later ([LICENSE](LICENSE)). Third-party components keep their own licenses.
