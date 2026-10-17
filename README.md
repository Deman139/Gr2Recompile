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
recommended for 1080p-1440p (the game uses about 4 GB); 8 GB for 1800p and 4K (about 5.5 GB).

Tested on a Ryzen 7 5700 + RTX 2060 Super (8 GB): the game should be playable from start to
finish. The first times, and the first loads above all, can stutter while shaders compile. Once
loaded

## What's included

- Launcher in English and Spanish: dump check, display mode, window size, **render resolution
  (720p, 1080p, 1440p, 1800p, 4K)**, frame rate limit (30, 60, and experimental 144, 240 and
  unlimited), VSync, anisotropic filtering, FSR 1, keyboard, mouse and controller mapping.
- Automatic updates: the launcher announces new versions and installs them keeping your saves.
- Mouse aim (as the right stick), keyboard, and Xbox / PlayStation controllers.
- Touchpad without one (OWL drone): hold **Delete** and drag the mouse.
- In-game menu with **Insert** (R3 + L2 on a controller); **F12** takes a screenshot.

## Known issues

- **First campaign load:** the very first time you start the campaign it can take a long time
  while many shaders compile, and it may even crash. Start the game again and it should load
  fine: the shaders stay cached.
- **Stutters** the first time an effect appears (the shader compiles, then stays cached).
- **Chapter 2:** slight lighting flicker at some moments while shaders load; it goes away with time.
- **Above 60 FPS (experimental):** turn VSync off on a 60 Hz monitor, otherwise it stays at 60.
- The shader cache can take several GB ("Clear cache" in the launcher).
- Multiplayer: offline only (Botzone).

## Future plans

**High priority**
- Upscalers: DLSS, FSR 3.1 and FSR 4.
- Performance: better use of the GPU and CPU, fewer stutters and faster shader loading.

**Medium priority**
- Debug menu, cheats and patches (for example unlocking every chapter).
- Fixes for bugs reported on specific hardware.

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

Requisitos: Windows 10 (1903+) u 11 de 64 bits, GPU con Vulkan 1.3, 16 GB de RAM. Se recomiendan
**6 GB de VRAM** para 1080p-1440p (el juego usa unos 4 GB); 8 GB para 1800p y 4K (unos 5,5 GB).

Probado en Ryzen 7 5700 + RTX 2060 Super (8 GB): el juego debería poder jugarse completo, de
principio a fin. Las primeras veces, y sobre todo en las primeras cargas, puede dar tirones
mientras se compilan los shaders. Todavía no fue probado en gráficas AMD ni en procesadores Intel.

## Qué incluye

- Launcher en inglés y español: verificación del dump, modo de pantalla, tamaño de ventana,
  **resolución de render (720p, 1080p, 1440p, 1800p, 4K)**, límite de FPS (30, 60, y
  experimentales 144, 240 y sin límite), VSync, filtrado anisotrópico, FSR 1, controles de
  teclado, mouse y mando.
- Actualizaciones automáticas: el launcher avisa de versiones nuevas y las instala conservando tus partidas.
- Mouse para apuntar (como stick derecho), teclado y mandos Xbox / PlayStation.
- Touchpad sin touchpad (dron OWL): mantener **Suprimir** y arrastrar el mouse.
- Menú dentro del juego con **Insert** (R3 + L2 en el mando); **F12** saca una captura.

## Problemas conocidos

- **Primera carga de la campaña:** la primera vez que iniciás la campaña puede demorar bastante
  mientras se compilan muchos shaders, e incluso puede crashear. Volvé a entrar y debería cargar
  bien: los shaders quedan en caché.
- **Tirones** la primera vez que aparece un efecto (el shader se compila y después queda en caché).
- **Capítulo 2:** parpadeo leve de iluminación en ciertos momentos de carga de shaders; se
  soluciona con el tiempo.
- **Más de 60 FPS (experimental):** con un monitor de 60 Hz desactivá VSync, si no se queda en 60.
- La caché de shaders puede ocupar varios GB ("Borrar caché" en el launcher).
- Multijugador: solo sin conexión (Botzone).

## Planes futuros

**Prioridad alta**
- Reescalado: DLSS, FSR 3.1 y FSR 4.
- Rendimiento: mejor aprovechamiento de GPU y CPU, menos tirones y carga de shaders más rápida.

**Prioridad media**
- Menú debug, trucos y parches (por ejemplo desbloquear todos los capítulos).
- Arreglos de errores reportados en hardware específico.

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
  texture fixes, fonts, resolution patches, VRAM reduction, launcher).
- **Felipe Prado**: [gow3_pc](https://github.com/FelipePrado0/gow3_pc), the base of this port
  (its README is in [docs/gow3_pc](docs/gow3_pc/README.md)).
- **Supermedo** and **deadinside28**: [bloodborne_pc](https://github.com/deadinside28/bloodborne_pc),
  the original native port technique.
- **uncharted_pc**: the mouse input layer.
- **shadPS4 team**: the [shadPS4](https://github.com/shadps4-emu/shadPS4) video core (GPU and
  shader translation to Vulkan).

## License

GNU GPL v2 or later ([LICENSE](LICENSE)). Third-party components keep their own licenses.
