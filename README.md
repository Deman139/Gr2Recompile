# Killzone Shadow Fall for PC

**English** | [Español](#español)

[![Join the Ports Discord](https://img.shields.io/badge/Discord-Join%20the%20Ports%20Discord-5865F2?logo=discord&logoColor=white)](https://discord.gg/UYFcV5rbDw)
[![Support the project and future ports](https://img.shields.io/badge/PayPal-Support%20the%20project%20%26%20future%20ports-00457C?logo=paypal&logoColor=white)](https://www.paypal.com/paypalme/SantiagoLescano577)
[![Latest release](https://img.shields.io/github/v/release/lesca-191204/killzone-shadowfall-pc?label=Download&color=2ea44f)](../../releases/latest)

> 💬 **We have a Discord!** The community server is the place to **report problems**, tell us
> **what you'd like to see next**, and **hear first about new ports** and how to support them.
> 👉 **[discord.gg/UYFcV5rbDw](https://discord.gg/UYFcV5rbDw)**
>
> ❤️ The ports are and will stay **free**. If you want to help, a PayPal donation is welcome but
> completely optional.

Native Windows port of **Killzone Shadow Fall** (PS4). The game's code runs directly on your CPU;
graphics are translated to Vulkan in real time. **Public beta:** the whole campaign is playable,
and it keeps improving with every report.

> ⚠️ **No game files are included.** You need your own dumped copy of the game, **version 01.81**:
> **CUSA00190** (the tested one) or another region (see [Game versions](#-game-versions)). This
> project is not affiliated with Sony Interactive Entertainment or Guerrilla Games. Killzone is a
> trademark of Sony Interactive Entertainment.

<!-- Screenshot: the launcher (Game tab) -->

## 📥 Download and play

1. Download the latest zip from **[Releases](../../releases/latest)** and unzip it anywhere
   (outside "Program Files").
2. Open **Killzone Shadow Fall.exe**, choose your dump's folder (the one holding `eboot.bin`).
3. Press **PLAY**. The launcher updates itself when a new version comes out, keeping your saves.

| | Minimum / recommended |
|---|---|
| 🪟 System | Windows 10 (1903+) or 11, 64-bit |
| 🎮 GPU | Vulkan 1.3 · **4-6 GB VRAM** for 1080p-1440p, **8 GB** for 1800p and 4K |
| 🧠 RAM | 16 GB |
| ✅ Tested on | Ryzen 7 5700 + RTX 2060 Super (8 GB): playable from start to finish |

Not tested yet on AMD GPUs or Intel CPUs: reports are very welcome.

## 🎮 Controls

> 🦉 **The OWL drone on keyboard and mouse:** hold **Delete** and move the mouse. That is your
> finger on the PS4 touchpad: swipe to choose the OWL's command, just like on the controller.
> The game also reminds you for a few seconds when it starts.

**Keyboard and mouse** (shooter-style defaults; change any key in the launcher → *Controls*):

| Action | Key | | Action | Key |
|---|---|---|---|---|
| Move | **W A S D** | | Camera | **Mouse** (direct, 1:1) |
| Fire (R2) | **Left click** | | Aim (L2) | **Right click** |
| Jump / ✕ | **Space** | | ○ (crouch) | **C** |
| □ (reload / use) | **R** | | △ (weapon) | **Q** |
| L1 / R1 | **1 / 3** | | Sprint (L3) | **Left Shift** |
| Melee (R3) | **F** | | D-pad | **Arrow keys** |
| Options (pause) | **Enter** | | Touchpad click ← / → | **Tab / Backspace** |
| 🦉 OWL (touchpad) | **Hold Delete + mouse** | | Free the cursor | **Left Alt** |

**Controller:** Xbox, DualShock 4, DualSense and any SDL-compatible pad, with remappable buttons,
stick dead zone, vibration and controller speaker volume.

**Shortcuts while playing:**

| Shortcut | What it does |
|---|---|
| **Insert** · **R3 + L2** | Opens the in-game menu (all the settings below) |
| **Hold Share / View / Select (~1 s)** | Quick menu: **Resume · Settings · Exit game** (controller only) |
| **Esc** · **Circle / B** | Closes the menu |
| **L1 / R1** · **Q / E** | Switch menu tabs |
| **F12** | Screenshot (`user\ps4\screenshots`) |
| **Alt + F4** | Closes the game |

## ✨ Features

### 🎯 Direct mouse aim (1:1)

The mouse turns the camera **directly**: no emulated stick, no dead zone, no response curve and no
speed cap. In practice it plays almost exactly like a PC shooter.

- **Sensitivity:** about **1** is recommended (higher values turn very fast).
- **Fast turns (x1 to x12, x6 by default):** while the mouse moves, the game's turn speed limit is
  raised so a quick 90° or 180° flick isn't held back. A controller keeps the game's own feel.
- **Invert Y axis.**
- ℹ️ *By the game's design, the camera turns at a different speed when you walk than when you
  sprint, so walking without sprinting can still feel a little different. Sprinting and aiming
  behave like a true 1:1 mouse.*
- Change all of it in the launcher (*Controls*) or **live** in the in-game menu (*Controls* tab).

### 🔭 Field of view (FOV)

A **FOV slider from 80% to 150%** of the original (100% = as on PS4). Set it in the launcher
(*Game* tab) or change it **live while playing** in the in-game menu (*Controls* tab): the view
widens at once. Aiming zoom scales in the same proportion.

<!-- Screenshot: FOV 100% vs 130% -->

### ⚙️ In-game menu (overlay)

Press **Insert** (or **R3 + L2**) at any time. Almost every launcher option is now here, and **most
apply at once**, without closing the game. Settings are saved automatically and the launcher shows
the same values.

<!-- Screenshot: in-game menu, Controls tab -->

| Tab | Option | When it applies |
|---|---|---|
| 🎮 **Controls** | Mouse sensitivity, fast turns, invert Y | ⚡ Live |
| | Field of view (FOV) | ⚡ Live |
| | Vibration, stick dead zone, controller speaker | ⚡ Live |
| | Hold Share for the quick menu, tips at startup | ⚡ Live |
| 🖼️ **Image** | Display mode (window / borderless / fullscreen), VSync | ⚡ Live |
| | FSR 1 upscaling, sharpening (RCAS) and its strength | ⚡ Live |
| | Upscaler **FSR 3.1 / DLSS** *(when the game started with one)* | ⚡ Live |
| | **Frame generation** Off / FSR 3.1 / DLSS | ⚡ Live · 🔄 restart to turn it on from Off |
| | Texture sharpness (LOD bias) | ⚡ Live |
| | Anisotropic filtering | 🔄 Restart |
| 🚀 **Performance** | Frame rate limit, frames queued, background shader compiling | ⚡ Live |
| | GPU readback options (the 0.2.5 performance options) | 🔄 Restart |
| 📊 **Overlay** | FPS, frame time, upscaler, GPU, CPU, RAM, VRAM · size, corner, opacity | ⚡ Live |

- 🔄 **Apply and restart:** options marked *restart* show a button that closes the game and the
  launcher starts it again by itself with the new settings (unsaved progress since your last
  checkpoint is lost).
- 🚪 **Exit game:** a button in the menu and in the quick menu closes the game cleanly; no keyboard
  needed when you play from the couch or through Steam.
- ℹ️ The in-game menu is in English for now.

### 🚀 Performance

- **0.3.0:** the threads that feed the GPU and stream the levels run at a higher Windows priority,
  and on CPUs with **8 threads or fewer** the GPU command thread too: smoother frame pacing on
  weaker CPUs when other programs compete for them.
- **0.2.5:** *Advanced → Performance* keeps the GPU busy instead of waiting (about **+40-50% FPS**
  in heavy scenes in our tests). **If you get a crash or a graphics error, turn those two options
  off (one at a time)** and tell us.

### 🖥️ Resolution, upscaling and frame generation

- **Above 1080p with AMD FSR 3.1 or NVIDIA DLSS** (1440p, 1800p, 4K): the game renders at 1080p and
  the upscaler takes it to your resolution; HUD, menus and videos are drawn sharp at full size.
- **Sharp and stable:** sub-pixel jitter and the game's own per-pixel motion vectors, so the weapon,
  moving objects and the camera are tracked correctly.
- **Frame generation:** **AMD FSR 3.1** on any GPU, **NVIDIA DLSS** on GeForce RTX 40/50 (up to 4x
  on RTX 50). Almost twice the frames on screen with sharp HUD and text.
- Classic method (a game resolution patch) still under *Advanced*.

### 🧰 Launcher

English and Spanish · dump check · automatic updates · display mode, window size, resolution,
upscaler, frame generation, frame rate limit (30, 60, experimental 144 / 240 / unlimited), VSync,
anisotropic filtering, texture sharpness, FSR 1 · keyboard, mouse and controller mapping · FOV ·
quick links to saves, screenshots and logs · **Ports Discord** button.

## 🗂️ Game versions

| Release | Region | Status |
|---|---|---|
| CUSA00190 | USA | ✅ Tested (the port's reference) |
| CUSA00191 | Latin America | ⚠️ Not tested, may work |
| CUSA00002 | Europe | ⚠️ Not tested, may work (choose the language in the game's options) |
| CUSA00085 | Japan | ⚠️ Not tested; Japanese texts may look wrong or be missing |

- **Version 01.81 is required** in every region (base game or its update).
- Saves are shared between regions: switching keeps them. No folder renaming or `param.sfo` edits.

## 🐧 Linux and Steam Deck (Proton, not tested by us)

1. Add **Killzone Shadow Fall.exe** to Steam as a *non-Steam game*.
2. *Properties → Compatibility*: force Proton Experimental or GE-Proton.
3. Start it from Steam, choose your dump's folder and press **PLAY**.

Reports (distro or Steam Deck, Proton version, GPU) are welcome on Discord or in
[issues](../../issues). A native Linux version (AppImage) will come once the game is fully stable.

## ⚠️ Known issues

- **Performance options (0.2.5):** they give about 40-50% more FPS, but if the game closes or
  something looks wrong, turn off *Advanced → Performance* and try again before reporting.
- **Mouse:** walking without sprinting turns at the game's own (slower) speed; see
  [Direct mouse aim](#-direct-mouse-aim-11).
- **First play:** some effects can show up a few frames late while their shader compiles.
- **Above 60 FPS (experimental):** turn VSync off on a 60 Hz monitor.
- The shader cache can take several GB ("Clear cache" in the launcher).
- Multiplayer: offline only (Botzone).

## 🗺️ Roadmap

**🔜 Coming next**
- 🖥️ **Ultrawide (21:9)** support: wider view (Hor+) with the HUD kept in place.
- 🎚️ **Upscaling quality modes** (Quality / Balanced / Performance) for weaker PCs.

**Later**
- FSR 4 · in-game menu in Spanish · debug menu, cheats and patches (e.g. unlocking every chapter).
- Native Linux version (AppImage), with Steam Deck in mind, once the game is fully stable.

## 🐞 Reporting bugs

Tell us on **[Discord](https://discord.gg/UYFcV5rbDw)** or open an [issue](../../issues) with what
happened, where (chapter), your GPU, CPU and driver, and attach that session's folder from
`user\logs\` (launcher **Logs** button). **Crash or graphics error?** First try with
*Advanced → Performance* turned off and say whether it still happens.

## 🛠️ Building from source

As in the gow3_pc base ([instructions](packaging/windows/README.md)): MSYS2 CLANG64,
`bash build.sh`, then `bash packaging/windows/package_kzsf.sh` for the zip (first,
`packaging/windows/make_kzsf_icon.py <your dump>/sce_sys/icon0.png launcher/kzsf.ico`). For DLSS,
build the bridge first with Visual Studio 2022 and the [NVIDIA DLSS SDK](https://github.com/NVIDIA/DLSS):
`DLSS_SDK_ROOT=<checkout> bash packaging/windows/build_dlss_kzsf.sh`.

---

## Español

[![Unite al Discord de los Ports](https://img.shields.io/badge/Discord-Unite%20al%20Discord%20de%20los%20Ports-5865F2?logo=discord&logoColor=white)](https://discord.gg/UYFcV5rbDw)
[![Apoyá el proyecto y futuros ports](https://img.shields.io/badge/PayPal-Apoy%C3%A1%20el%20proyecto%20y%20futuros%20ports-00457C?logo=paypal&logoColor=white)](https://www.paypal.com/paypalme/SantiagoLescano577)
[![Última versión](https://img.shields.io/github/v/release/lesca-191204/killzone-shadowfall-pc?label=Descargar&color=2ea44f)](../../releases/latest)

> 💬 **¡Tenemos Discord!** El servidor de la comunidad es el lugar para **reportar problemas**,
> contarnos **qué te gustaría que agreguemos** y **enterarte antes que nadie de los nuevos ports**
> y de cómo apoyarlos.
> 👉 **[discord.gg/UYFcV5rbDw](https://discord.gg/UYFcV5rbDw)**
>
> ❤️ Los ports son y seguirán siendo **gratuitos**. Si querés ayudar, una donación por PayPal se
> agradece, pero es totalmente opcional.

Port nativo para Windows de **Killzone Shadow Fall** (PS4). El código del juego corre directamente
en tu CPU; los gráficos se traducen a Vulkan en tiempo real. **Beta pública:** se puede jugar la
campaña completa, y mejora con cada reporte.

> ⚠️ **No incluye archivos del juego.** Necesitás tu propia copia dumpeada del juego, **versión
> 01.81**: **CUSA00190** (la probada) u otra región (ver [Versiones del juego](#️-versiones-del-juego)).
> Este proyecto no está afiliado a Sony Interactive Entertainment ni a Guerrilla Games. Killzone es
> una marca de Sony Interactive Entertainment.

## 📥 Descargar y jugar

1. Bajá el zip de la última versión en **[Releases](../../releases/latest)** y descomprimilo donde
   quieras (fuera de "Archivos de programa").
2. Abrí **Killzone Shadow Fall.exe** y elegí la carpeta de tu dump (la que tiene `eboot.bin`).
3. Dale a **JUGAR**. El launcher se actualiza solo cuando sale una versión nueva, conservando tus
   partidas.

| | Mínimo / recomendado |
|---|---|
| 🪟 Sistema | Windows 10 (1903+) u 11 de 64 bits |
| 🎮 GPU | Vulkan 1.3 · **4-6 GB de VRAM** para 1080p-1440p, **8 GB** para 1800p y 4K |
| 🧠 RAM | 16 GB |
| ✅ Probado en | Ryzen 7 5700 + RTX 2060 Super (8 GB): se juega completo de principio a fin |

Todavía no fue probado en gráficas AMD ni en procesadores Intel: los reportes se agradecen mucho.

## 🎮 Controles

> 🦉 **El dron OWL con teclado y mouse:** mantené **Suprimir (Delete)** y mové el mouse. Es tu dedo
> sobre el touchpad de PS4: deslizá para elegir la orden del OWL, igual que con el mando. El juego
> también te lo recuerda unos segundos al empezar.

**Teclado y mouse** (por defecto estilo shooter; cambiá cualquier tecla en el launcher → *Controles*):

| Acción | Tecla | | Acción | Tecla |
|---|---|---|---|---|
| Moverse | **W A S D** | | Cámara | **Mouse** (directo, 1:1) |
| Disparar (R2) | **Click izquierdo** | | Apuntar (L2) | **Click derecho** |
| Saltar / ✕ | **Espacio** | | ○ (agacharse) | **C** |
| □ (recargar / usar) | **R** | | △ (arma) | **Q** |
| L1 / R1 | **1 / 3** | | Correr (L3) | **Shift izquierdo** |
| Cuerpo a cuerpo (R3) | **F** | | Cruceta | **Flechas** |
| Options (pausa) | **Enter** | | Click del touchpad ← / → | **Tab / Retroceso** |
| 🦉 OWL (touchpad) | **Mantener Suprimir + mouse** | | Liberar el cursor | **Alt izquierdo** |

**Mando:** Xbox, DualShock 4, DualSense y cualquier mando compatible con SDL, con botones
remapeables, zona muerta de sticks, vibración y volumen del altavoz del mando.

**Atajos mientras jugás:**

| Atajo | Qué hace |
|---|---|
| **Insert** · **R3 + L2** | Abre el menú dentro del juego (todas las opciones de abajo) |
| **Mantener Share / View / Select (~1 s)** | Menú rápido: **Continuar · Ajustes · Salir del juego** (solo mando) |
| **Esc** · **Círculo / B** | Cierra el menú |
| **L1 / R1** · **Q / E** | Cambia de pestaña en el menú |
| **F12** | Captura de pantalla (`user\ps4\screenshots`) |
| **Alt + F4** | Cierra el juego |

## ✨ Funciones

### 🎯 Mouse directo (1:1)

El mouse gira la cámara **directamente**: sin stick emulado, sin zona muerta, sin curva de
respuesta y sin tope de velocidad. En la práctica se juega casi igual que un shooter de PC.

- **Sensibilidad:** se recomienda alrededor de **1** (valores altos giran muy rápido).
- **Giros rápidos (x1 a x12, x6 por defecto):** mientras movés el mouse, se sube el límite de
  velocidad de giro del juego para que un 90° o 180° rápido no se frene. Con mando se mantiene la
  sensación original del juego.
- **Invertir eje Y.**
- ℹ️ *Por diseño del juego, la cámara gira a otra velocidad cuando caminás que cuando corrés, así
  que caminar sin esprintar todavía puede sentirse un poco distinto. Corriendo y apuntando se
  comporta como un mouse 1:1 de verdad.*
- Todo se cambia en el launcher (*Controles*) o **en vivo** en el menú dentro del juego (pestaña
  *Controls*).

### 🔭 Campo de visión (FOV)

Un **slider de FOV de 80% a 150%** del original (100% = como en PS4). Se ajusta en el launcher
(pestaña *Juego*) o **en vivo mientras jugás** en el menú dentro del juego (pestaña *Controls*): la
vista se abre al instante. El zoom al apuntar se escala en la misma proporción.

### ⚙️ Menú dentro del juego (overlay)

Apretá **Insert** (o **R3 + L2**) cuando quieras. Ahí está casi todo lo del launcher, y **la mayoría
se aplica al instante**, sin cerrar el juego. Se guarda solo, y el launcher muestra los mismos valores.

| Pestaña | Opción | Cuándo se aplica |
|---|---|---|
| 🎮 **Controls** | Sensibilidad del mouse, giros rápidos, invertir Y | ⚡ En vivo |
| | Campo de visión (FOV) | ⚡ En vivo |
| | Vibración, zona muerta de sticks, altavoz del mando | ⚡ En vivo |
| | Mantener Share para el menú rápido, consejos al iniciar | ⚡ En vivo |
| 🖼️ **Image** | Modo de pantalla (ventana / sin bordes / completa), VSync | ⚡ En vivo |
| | Reescalado FSR 1, nitidez (RCAS) y su intensidad | ⚡ En vivo |
| | Reescalador **FSR 3.1 / DLSS** *(si el juego arrancó con uno)* | ⚡ En vivo |
| | **Generación de frames** Apagada / FSR 3.1 / DLSS | ⚡ En vivo · 🔄 reinicio para encenderla desde apagada |
| | Nitidez de texturas (LOD bias) | ⚡ En vivo |
| | Filtrado anisotrópico | 🔄 Reinicio |
| 🚀 **Performance** | Límite de FPS, cuadros en cola, compilación de shaders en segundo plano | ⚡ En vivo |
| | Opciones de lectura de GPU (las de rendimiento de la 0.2.5) | 🔄 Reinicio |
| 📊 **Overlay** | FPS, frametime, reescalador, GPU, CPU, RAM, VRAM · tamaño, esquina, opacidad | ⚡ En vivo |

- 🔄 **Aplicar y reiniciar:** las opciones de *reinicio* muestran un botón que cierra el juego, y el
  launcher lo vuelve a abrir solo con lo nuevo (se pierde lo no guardado desde el último punto de
  control).
- 🚪 **Salir del juego:** un botón en el menú y en el menú rápido cierra el juego bien; no hace falta
  teclado si jugás desde el sillón o por Steam.
- ℹ️ Por ahora el menú dentro del juego está en inglés.

### 🚀 Rendimiento

- **0.3.0:** los hilos que alimentan a la GPU y cargan los niveles corren con más prioridad en
  Windows, y en procesadores de **8 hilos o menos** también el hilo de comandos de la GPU: un
  ritmo de cuadros más parejo en CPUs más débiles cuando otros programas compiten por ellas.
- **0.2.5:** *Avanzado → Rendimiento* hace que la GPU trabaje en vez de esperar (cerca de **+40-50%
  de FPS** en escenas pesadas en nuestras pruebas). **Si tenés un crash o un error gráfico,
  desactivá esas dos opciones (de a una)** y contanos.

### 🖥️ Resolución, reescalado y generación de frames

- **Más de 1080p con AMD FSR 3.1 o NVIDIA DLSS** (1440p, 1800p, 4K): el juego renderiza en 1080p y
  el reescalador lo lleva a tu resolución; HUD, menús y videos se dibujan nítidos a tamaño completo.
- **Nítido y estable:** jitter de subpíxel y los vectores de movimiento por píxel del propio juego,
  así el arma, los objetos que se mueven y la cámara se siguen bien.
- **Generación de frames:** **AMD FSR 3.1** en cualquier GPU, **NVIDIA DLSS** en GeForce RTX 40/50
  (hasta 4x en RTX 50). Casi el doble de cuadros con HUD y textos nítidos.
- El método clásico (parche de resolución del juego) sigue en *Avanzado*.

### 🧰 Launcher

Inglés y español · verificación del dump · actualizaciones automáticas · modo de pantalla, tamaño
de ventana, resolución, reescalador, generación de frames, límite de FPS (30, 60, experimentales
144 / 240 / sin límite), VSync, filtrado anisotrópico, nitidez de texturas, FSR 1 · controles de
teclado, mouse y mando · FOV · accesos a partidas, capturas y logs · botón **Ports Discord**.

## 🗂️ Versiones del juego

| Versión | Región | Estado |
|---|---|---|
| CUSA00190 | EE. UU. | ✅ Probada (la referencia del port) |
| CUSA00191 | Latinoamérica | ⚠️ No probada, puede funcionar |
| CUSA00002 | Europa | ⚠️ No probada, puede funcionar (el idioma se elige en las opciones del juego) |
| CUSA00085 | Japón | ⚠️ No probada; los textos en japonés pueden verse mal o faltar |

- **Es obligatoria la versión 01.81** en cualquier región (juego base o su actualización).
- Las partidas se comparten entre regiones. No hace falta renombrar carpetas ni editar `param.sfo`.

## 🐧 Linux y Steam Deck (Proton, no probado por nosotros)

1. Agregá **Killzone Shadow Fall.exe** a Steam como *juego que no es de Steam*.
2. *Propiedades → Compatibilidad*: forzá Proton Experimental o GE-Proton.
3. Abrilo desde Steam, elegí la carpeta de tu dump y dale a **JUGAR**.

Se agradecen reportes (distro o Steam Deck, versión de Proton, GPU) en el Discord o en
[issues](../../issues). Cuando el juego esté 100% estable va a salir una versión nativa para
Linux (AppImage).

## ⚠️ Problemas conocidos

- **Opciones de rendimiento (0.2.5):** dan cerca de 40-50% más de FPS, pero si el juego se cierra
  o algo se ve mal, desactivá *Avanzado → Rendimiento* y probá de nuevo antes de reportar.
- **Mouse:** caminando sin correr, la cámara gira a la velocidad (más lenta) del propio juego; ver
  [Mouse directo](#-mouse-directo-11).
- **Primera partida:** algunos efectos pueden aparecer unos cuadros tarde mientras se compila su shader.
- **Más de 60 FPS (experimental):** con un monitor de 60 Hz desactivá VSync.
- La caché de shaders puede ocupar varios GB ("Borrar caché" en el launcher).
- Multijugador: solo sin conexión (Botzone).

## 🗺️ Próximamente

**🔜 Lo que viene**
- 🖥️ **Soporte ultrawide (21:9):** vista más ancha (Hor+) con el HUD en su lugar.
- 🎚️ **Calidades de reescalado** (Calidad / Equilibrado / Rendimiento) para PCs más modestas.

**Más adelante**
- FSR 4 · menú dentro del juego en español · menú debug, trucos y parches (por ejemplo desbloquear
  todos los capítulos).
- Versión nativa para Linux (AppImage), pensando en Steam Deck, cuando el juego esté 100% estable.

## 🐞 Reportar errores

Contanos en el **[Discord](https://discord.gg/UYFcV5rbDw)** o abrí un [issue](../../issues) con qué
pasó, dónde (capítulo), tu GPU, CPU y driver, y adjuntá la carpeta de esa partida en `user\logs\`
(botón **Logs** del launcher). **¿Crash o error gráfico?** Primero probá con *Avanzado → Rendimiento*
desactivado y decí si sigue pasando.

## 🛠️ Compilar desde el código

Igual que la base gow3_pc ([instrucciones](packaging/windows/README.md)): MSYS2 CLANG64,
`bash build.sh`, y después `bash packaging/windows/package_kzsf.sh` para el zip (antes,
`packaging/windows/make_kzsf_icon.py <tu dump>/sce_sys/icon0.png launcher/kzsf.ico`). Para DLSS,
compilá antes el puente con Visual Studio 2022 y el [SDK de NVIDIA DLSS](https://github.com/NVIDIA/DLSS):
`DLSS_SDK_ROOT=<checkout> bash packaging/windows/build_dlss_kzsf.sh`.

---

## 🙏 Credits / Créditos

- **Lesca**: Killzone Shadow Fall port (dynamic module loader, PS4 clock fix, tessellation and
  texture fixes, fonts, resolution patches, VRAM reduction, shader stutter fix, FSR/DLSS
  calibration, jitter and motion vectors, frame generation, direct mouse aim, live FOV, in-game
  menu and quick menu, launcher).
- **Felipe Prado**: [gow3_pc / SpartanPort](https://github.com/FelipePrado0/SpartanPort), the base
  of this port (its README is in [docs/gow3_pc](docs/gow3_pc/README.md)).
- **Supermedo** and **deadinside28**: [bloodborne_pc](https://github.com/deadinside28/bloodborne_pc),
  the original native port technique, and [uncharted_pc](https://github.com/Supermedo/uncharted_pc)
  (mouse input layer, file read, FPU state and texture cache fixes, frame generation).
- **HayatoG**: [iss_pc](https://github.com/HayatoG/iss_pc) (shader permutation and cache fixes,
  driver pipeline cache, texture LOD bias).
- **shadPS4 team**: the [shadPS4](https://github.com/shadps4-emu/shadPS4) video core (GPU and
  shader translation to Vulkan).
- **clearfeather**: startup crash fix for base 01.00 + update installs (file symlink stat, PR #31),
  controller/audio suggestions and the controller quick menu idea (#49).
- **AMD FidelityFX FSR 3.1** ([FSR-Vulkan](https://github.com/FireBurn/FSR-Vulkan)) and **NVIDIA
  DLSS** ([DLSS SDK](https://github.com/NVIDIA/DLSS); NVIDIA's license is in `licenses\`).

## 📄 License

GNU GPL v2 or later ([LICENSE](LICENSE)). Third-party components keep their own licenses.
