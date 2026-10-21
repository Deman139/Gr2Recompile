# Killzone Shadow Fall for PC

**English** | [Español](#español)

[![Support the project and future ports](https://img.shields.io/badge/PayPal-Support%20the%20project%20%26%20future%20ports-00457C?logo=paypal&logoColor=white)](https://www.paypal.com/paypalme/SantiagoLescano577) *Optional: the ports are and will stay free.*

Native Windows port of **Killzone Shadow Fall** (PS4). The game's code runs directly on the CPU;
graphics are translated to Vulkan in real time. Public beta: the whole campaign is playable, and
it keeps improving with testers' reports.

> **No game files are included.** You need your own dumped copy of the game, **version 01.81**:
> **CUSA00190** (the tested one) or another region (see [Game versions](#game-versions)). This
> project is not affiliated with Sony Interactive Entertainment or Guerrilla Games. Killzone is a
> trademark of Sony Interactive Entertainment.

## Download and play

1. Download the latest zip from [Releases](../../releases) and unzip it anywhere (outside
   "Program Files").
2. Open **Killzone Shadow Fall.exe**, choose your dump's folder (the one holding `eboot.bin`) and
   press **PLAY**.

Requirements: Windows 10 (1903+) or 11 64-bit, a Vulkan 1.3 GPU, 16 GB of RAM. **4 to 6 GB of VRAM**
recommended for 1080p-1440p; 8 GB for 1800p and 4K.

Tested on a Ryzen 7 5700 + RTX 2060 Super (8 GB): the game should be playable from start to
finish. Shader compilation barely stutters, even on a first play, and levels load fast. Not
tested yet on AMD GPUs or Intel CPUs.

## Game versions

The launcher accepts **CUSA00190, CUSA00191, CUSA00002 and CUSA00085**. The port was **developed and
tested on CUSA00190 (USA, English and French)**; the others are **untested**: players have run some
of them, but there are no guarantees.

| Release | Region | Status |
|---|---|---|
| CUSA00190 | USA | ✅ Tested (the port's reference) |
| CUSA00191 | Latin America | ⚠️ Not tested, may work |
| CUSA00002 | Europe | ⚠️ Not tested, may work (players report choosing the language in the game's options) |
| CUSA00085 | Japan | ⚠️ Not tested; Japanese texts may look wrong or be missing |

- **Version 01.81 is required** in every region (the base game or its update). Without that
  update the game will most likely not work.
- Saves are shared: every release uses the same save folder, so switching region keeps them.
- No need to rename folders or edit `param.sfo` anymore.

## Linux and Steam Deck (Proton, not tested by us)

There is no Linux build yet, but players have run the Windows version through Proton:

1. Unzip the release and add **Killzone Shadow Fall.exe** to Steam as a *non-Steam game*.
2. In its *Properties → Compatibility*, force a Proton version (Proton Experimental or GE-Proton).
3. Start it from Steam, choose your dump's folder in the launcher and press **PLAY**.

We can't test it ourselves: reports in [issues](../../issues) (distro or Steam Deck, Proton
version, GPU) are welcome. A native Linux version (AppImage) will come once the game is fully
stable on Windows.

## What's included

- **Resolutions above 1080p with AMD FSR 3.1 or NVIDIA DLSS** (1440p, 1800p, 4K): the game renders
  at 1080p and the upscaler takes it to your resolution, while the HUD, menus and videos are drawn
  sharp at full resolution. The classic method (a game patch) is still under *Advanced*.
- **Sharp, stable FSR and DLSS:** sub-pixel jitter (with the game's own temporal anti-aliasing
  replaced by the upscaler) and the game's own per-pixel motion vectors, so the weapon, moving
  objects and the camera are tracked correctly.
- **Frame generation**: **AMD FSR 3.1** on any GPU, **NVIDIA DLSS** on GeForce RTX
  40/50 (up to 4x on RTX 50). Almost twice the frames on screen; the HUD and text stay sharp. At
  1080p it uses FSR / DLSS as anti-aliasing at native resolution.
- Launcher in English and Spanish: dump check, display mode, window size, render resolution and
  method, frame rate limit (30, 60, and experimental 144, 240 and unlimited), frame generation,
  VSync, anisotropic filtering, distant texture sharpness, FSR 1, keyboard, mouse and controller
  mapping.
- Automatic updates: the launcher announces new versions and installs them keeping your saves.
- Mouse aim (as the right stick), keyboard, and Xbox / PlayStation controllers.
- Touchpad without one (OWL drone): hold **Delete** and drag the mouse.
- In-game menu with **Insert** (R3 + L2 on a controller); **F12** takes a screenshot.

## Known issues

- **First play:** some effects can show up a few frames late while their shader compiles.
- **Above 60 FPS (experimental):** turn VSync off on a 60 Hz monitor, otherwise it stays at 60.
- The shader cache can take several GB ("Clear cache" in the launcher).
- Multiplayer: offline only (Botzone).

## Future plans

**High priority**
- Performance: better use of the GPU and CPU.
- Mouse input: direct camera control instead of the emulated right stick.
- Launcher options in the in-game menu (Insert), changeable while playing.

**Medium priority**
- FSR 4.
- Ultrawide (21:9) support.
- Debug menu, cheats and patches (for example unlocking every chapter).
- Fixes for bugs reported on specific hardware.

**Once the game is fully stable**
- Native Linux version (AppImage), with Steam Deck in mind.

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

[![Apoyá el proyecto y futuros ports](https://img.shields.io/badge/PayPal-Apoy%C3%A1%20el%20proyecto%20y%20futuros%20ports-00457C?logo=paypal&logoColor=white)](https://www.paypal.com/paypalme/SantiagoLescano577) *Opcional: los ports son y seguirán siendo gratuitos.*

Port nativo para Windows de **Killzone Shadow Fall** (PS4). El código del juego corre directamente
en la CPU; los gráficos se traducen a Vulkan en tiempo real. Beta pública: se puede jugar la
campaña completa, y se sigue puliendo con los reportes de los testers.

> **No incluye archivos del juego.** Necesitás tu propia copia dumpeada del juego, **versión 01.81**:
> **CUSA00190** (la probada) u otra región (ver [Versiones del juego](#versiones-del-juego)). Este
> proyecto no está afiliado a Sony Interactive Entertainment ni a Guerrilla Games. Killzone es una
> marca de Sony Interactive Entertainment.

## Descargar y jugar

1. Bajá el zip de la última versión en [Releases](../../releases) y descomprimilo donde quieras
   (fuera de "Archivos de programa").
2. Abrí **Killzone Shadow Fall.exe**, elegí la carpeta de tu dump (la que tiene `eboot.bin`) y
   dale a **JUGAR**.

Requisitos: Windows 10 (1903+) u 11 de 64 bits, GPU con Vulkan 1.3, 16 GB de RAM. Se recomiendan
**4 a 6 GB de VRAM** para 1080p-1440p; 8 GB para 1800p y 4K.

Probado en Ryzen 7 5700 + RTX 2060 Super (8 GB): el juego debería poder jugarse completo, de
principio a fin. La compilación de shaders casi no da tirones, aun en la primera partida, y los
niveles cargan rápido. Todavía no fue probado en gráficas AMD ni en procesadores Intel.

## Versiones del juego

El launcher acepta **CUSA00190, CUSA00191, CUSA00002 y CUSA00085**. El port se **desarrolló y probó
en CUSA00190 (EE. UU., inglés y francés)**; las demás están **sin probar**: algunos jugadores las
corrieron, pero no hay garantías.

| Versión | Región | Estado |
|---|---|---|
| CUSA00190 | EE. UU. | ✅ Probada (la referencia del port) |
| CUSA00191 | Latinoamérica | ⚠️ No probada, puede funcionar |
| CUSA00002 | Europa | ⚠️ No probada, puede funcionar (hay reportes de que se elige el idioma en las opciones del juego) |
| CUSA00085 | Japón | ⚠️ No probada; los textos en japonés pueden verse mal o faltar |

- **Es obligatorio que sea la versión 01.81** en cualquier región (el juego base o su
  actualización). Sin esa actualización, lo más probable es que no funcione.
- Las partidas se comparten: todas las versiones usan la misma carpeta de guardado, así que
  cambiar de región las conserva.
- Ya no hace falta renombrar carpetas ni editar `param.sfo`.

## Linux y Steam Deck (Proton, no probado por nosotros)

Todavía no hay versión para Linux, pero hay jugadores que corrieron la de Windows con Proton:

1. Descomprimí la release y agregá **Killzone Shadow Fall.exe** a Steam como *juego que no es de Steam*.
2. En *Propiedades → Compatibilidad*, forzá una versión de Proton (Proton Experimental o GE-Proton).
3. Abrilo desde Steam, elegí la carpeta de tu dump en el launcher y dale a **JUGAR**.

No podemos probarlo nosotros: se agradecen reportes en [issues](../../issues) (distro o Steam
Deck, versión de Proton, GPU). Cuando el juego esté 100% estable en Windows, va a salir una versión
nativa para Linux (AppImage).

## Qué incluye

- **Resoluciones mayores a 1080p con AMD FSR 3.1 o NVIDIA DLSS** (1440p, 1800p, 4K): el juego
  renderiza en 1080p y el reescalador lo lleva a tu resolución, mientras el HUD, los menús y los
  videos se dibujan nítidos a resolución completa. El método clásico (parche del juego) sigue
  disponible en *Avanzado*.
- **FSR y DLSS nítidos y estables:** jitter de subpíxel (el reescalador reemplaza al antialiasing
  temporal propio del juego) y los vectores de movimiento por píxel del propio juego, así el arma,
  los objetos que se mueven y la cámara se siguen bien.
- **Generación de frames**: **AMD FSR 3.1** en cualquier GPU, **NVIDIA DLSS** en
  GeForce RTX 40/50 (hasta 4x en RTX 50). Casi el doble de cuadros en pantalla; el HUD y los textos
  quedan nítidos. A 1080p usa FSR / DLSS como antialiasing a resolución nativa.
- Launcher en inglés y español: verificación del dump, modo de pantalla, tamaño de ventana,
  resolución y método de render, límite de FPS (30, 60, y experimentales 144, 240 y sin límite),
  generación de frames, VSync, filtrado anisotrópico, nitidez de texturas lejanas, FSR 1, controles
  de teclado, mouse y mando.
- Actualizaciones automáticas: el launcher avisa de versiones nuevas y las instala conservando tus partidas.
- Mouse para apuntar (como stick derecho), teclado y mandos Xbox / PlayStation.
- Touchpad sin touchpad (dron OWL): mantener **Suprimir** y arrastrar el mouse.
- Menú dentro del juego con **Insert** (R3 + L2 en el mando); **F12** saca una captura.

## Problemas conocidos

- **Primera partida:** algunos efectos pueden aparecer unos cuadros tarde mientras se compila su shader.
- **Más de 60 FPS (experimental):** con un monitor de 60 Hz desactivá VSync, si no se queda en 60.
- La caché de shaders puede ocupar varios GB ("Borrar caché" en el launcher).
- Multijugador: solo sin conexión (Botzone).

## Planes futuros

**Prioridad alta**
- Rendimiento: mejor aprovechamiento de GPU y CPU.
- Input del mouse: control directo de la cámara en lugar del stick derecho emulado.
- Opciones del launcher en el menú dentro del juego (Insert), cambiables mientras jugás.

**Prioridad media**
- FSR 4.
- Soporte ultrawide (21:9).
- Menú debug, trucos y parches (por ejemplo desbloquear todos los capítulos).
- Arreglos de errores reportados en hardware específico.

**Cuando el juego esté 100% estable**
- Versión nativa para Linux (AppImage), pensando en Steam Deck.

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
  calibration, jitter and motion vectors, frame generation, launcher).
- **Felipe Prado**: [gow3_pc / SpartanPort](https://github.com/FelipePrado0/SpartanPort), the base
  of this port (its README is in [docs/gow3_pc](docs/gow3_pc/README.md)).
- **Supermedo** and **deadinside28**: [bloodborne_pc](https://github.com/deadinside28/bloodborne_pc),
  the original native port technique, and [uncharted_pc](https://github.com/Supermedo/uncharted_pc)
  (mouse input layer, file read, FPU state and texture cache fixes, frame generation).
- **HayatoG**: [iss_pc](https://github.com/HayatoG/iss_pc) (shader permutation and cache fixes,
  driver pipeline cache, texture LOD bias).
- **shadPS4 team**: the [shadPS4](https://github.com/shadps4-emu/shadPS4) video core (GPU and
  shader translation to Vulkan).
- **AMD FidelityFX FSR 3.1** ([FSR-Vulkan](https://github.com/FireBurn/FSR-Vulkan)) and **NVIDIA
  DLSS** ([DLSS SDK](https://github.com/NVIDIA/DLSS); NVIDIA's license is in `licenses\`).

## License

GNU GPL v2 or later ([LICENSE](LICENSE)). Third-party components keep their own licenses.
