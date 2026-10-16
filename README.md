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

Requirements: Windows 10 (1903+) or 11 64-bit, a Vulkan 1.3 GPU, 16 GB of RAM. We don't
recommend playing with less than 8 GB of VRAM.

Tested on a Ryzen 7 5700 + RTX 2060 Super: the game should be playable from start to finish.
The first times, and the first loads above all, can stutter quite a bit while shaders compile;
once loaded the game runs well, 40-60 FPS on that GPU. Not tested yet on AMD GPUs or Intel CPUs.

## What's included

- Launcher in English and Spanish: dump check, display mode, window size, 30/60 FPS limit, VSync,
  anisotropic filtering, FSR 1, keyboard, mouse and controller mapping.
- Mouse aim (as the right stick), keyboard, and Xbox / PlayStation controllers.
- Touchpad without one (OWL drone): hold **Delete** and drag the mouse.
- In-game menu with **Insert** (R3 + L2 on a controller); **F12** takes a screenshot.

Known issues: stutters the first time an effect appears (shaders compile, then stay cached); in
chapter 2, slight lighting flicker at some moments while shaders load, which goes away with time;
a shader cache of several GB; multiplayer offline only (Botzone).

## Reporting bugs

Open an [issue](../../issues) with what happened, where (chapter), your GPU, CPU and driver, and
attach that session's folder from `user\logs\` (launcher **Logs** button).

## Building from source

As in the gow3_pc base ([instructions](packaging/windows/README.md)): MSYS2 CLANG64,
`bash build.sh`, then `bash packaging/windows/package_kzsf.sh` for the zip (first,
`packaging/windows/make_kzsf_icon.py <your dump>/sce_sys/icon0.png launcher/kzsf.ico`).

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

Requisitos: Windows 10 (1903+) u 11 de 64 bits, GPU con Vulkan 1.3, 16 GB de RAM. No
recomendamos jugarlo con menos de 8 GB de VRAM.

Probado en Ryzen 7 5700 + RTX 2060 Super: el juego debería poder jugarse completo, de principio a
fin. Las primeras veces, y sobre todo en las primeras cargas, puede dar bastantes tirones mientras
se compilan los shaders; una vez cargado corre bastante bien, 40-60 FPS en esa gráfica. Todavía no
fue probado en gráficas AMD ni en procesadores Intel.

## Qué incluye

- Launcher en inglés y español: verificación del dump, modo de pantalla, tamaño de ventana,
  límite de 30/60 FPS, VSync, filtrado anisotrópico, FSR 1, controles de teclado, mouse y mando.
- Mouse para apuntar (como stick derecho), teclado y mandos Xbox / PlayStation.
- Touchpad sin touchpad (dron OWL): mantener **Suprimir** y arrastrar el mouse.
- Menú dentro del juego con **Insert** (R3 + L2 en el mando); **F12** saca una captura.

Problemas conocidos: tirones la primera vez que aparece un efecto (compilación de shaders, luego
quedan en caché); en el capítulo 2, parpadeo leve de iluminación en ciertos momentos de carga de
shaders, que se soluciona con el tiempo; caché de shaders de varios GB; multijugador solo sin
conexión (Botzone).

## Reportar errores

Abrí un [issue](../../issues) con qué pasó, dónde (capítulo), tu GPU, CPU y driver, y adjuntá la
carpeta de esa partida en `user\logs\` (botón **Logs** del launcher).

## Compilar desde el código

Igual que la base gow3_pc ([instrucciones](packaging/windows/README.md)): MSYS2 CLANG64,
`bash build.sh`, y después `bash packaging/windows/package_kzsf.sh` para el zip (antes,
`packaging/windows/make_kzsf_icon.py <tu dump>/sce_sys/icon0.png launcher/kzsf.ico`).

---

## Credits / Créditos

- **Lesca**: Killzone Shadow Fall port (dynamic module loader, PS4 clock fix, tessellation and
  texture fixes, fonts, launcher).
- **Felipe Prado**: [gow3_pc](https://github.com/FelipePrado0/gow3_pc), the base of this port
  (its README is in [docs/gow3_pc](docs/gow3_pc/README.md)).
- **Supermedo** and **deadinside28**: [bloodborne_pc](https://github.com/deadinside28/bloodborne_pc),
  the original native port technique.
- **uncharted_pc**: the mouse input layer.
- **shadPS4 team**: the [shadPS4](https://github.com/shadps4-emu/shadPS4) video core (GPU and
  shader translation to Vulkan).

## License

GNU GPL v2 or later ([LICENSE](LICENSE)). Third-party components keep their own licenses.
