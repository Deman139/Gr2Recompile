KILLZONE SHADOW FALL - Native Windows port (Lesca) - Beta 0.3.0
=================================================================

  Ports Discord (report problems, suggest features, hear about new ports first):
  https://discord.gg/UYFcV5rbDw

[ENGLISH]

What it is
  An unofficial port that runs Killzone Shadow Fall (PS4) on Windows. The game's code runs
  directly on your CPU; graphics are translated to Vulkan in real time.
  It does NOT include any game files: you need your own dumped copy of the game,
  version 01.81. If your base dump is 01.00, also the 01.81 update.

Game versions
  Developed and tested on CUSA00190 (USA, English and French). The launcher also accepts,
  untested (they may work, no guarantees): CUSA00191 (Latin America), CUSA00002 (Europe) and
  CUSA00085 (Japan; Japanese texts may look wrong or be missing). Version 01.81 is required in
  every region: without it the game will most likely not work. All of them share the same saves.

Linux and Steam Deck (Proton, not tested by us)
  No Linux build yet. Add "Killzone Shadow Fall.exe" to Steam as a non-Steam game, force a
  Proton version in its Properties > Compatibility (Proton Experimental or GE-Proton) and start
  it from Steam. A native Linux version will come once the game is fully stable.

Requirements
  - Windows 10 (1903 or later) or 11, 64-bit.
  - A Vulkan 1.3 GPU. 6 GB of VRAM recommended for 1080p-1440p (the game uses about
    4 GB); 8 GB for 1800p and 4K (about 5.5 GB).
  - 16 GB of RAM.

Tested setup
  Ryzen 7 5700 + RTX 2060 Super (8 GB): the game should be playable from start to finish.
  Shader compilation barely stutters, even on a first play, and levels load fast.
  Not tested yet on AMD GPUs or Intel CPUs.

Resolution above 1080p (Graphics > Render resolution, method under Advanced)
  - AMD FSR 3.1 (recommended, any GPU) or NVIDIA DLSS (GeForce RTX): the game renders at 1080p
    and the upscaler takes it to 1440p, 1800p or 4K; HUD, menus and videos are drawn sharp at
    that resolution. Sharp (sub-pixel jitter) and stable (the game's own motion vectors).
  - Classic: a game patch makes the game render at that resolution (heavier, more glitches).

Frame generation (Graphics > Frame generation, experimental)
  - AMD FSR 3.1 (any GPU) or NVIDIA DLSS (GeForce RTX 40/50; other cards get FSR 3.1): a frame
    is generated between each pair of the game's frames, almost twice the FPS on screen, with
    a little more latency. HUD and text stay sharp. At 1080p it uses FSR / DLSS as
    anti-aliasing. Best above 40 FPS; on a 60 Hz monitor turn VSync off.

How to play
  1. Unzip this folder anywhere (outside "Program Files").
  2. Open "Killzone Shadow Fall.exe".
  3. On the Game tab, Browse... and choose your dump's folder (the one holding eboot.bin).
  4. PLAY.

Performance (new in 0.2.5)
  Advanced > Performance has two options, on by default: "Parallel drawing" and "Game copies
  without waiting for the GPU". Together about +40-50% FPS in heavy scenes in our tests.
  If the game closes or something looks wrong, turn them off (one at a time) and play as before;
  please tell us in an issue whether that fixed it.

Controls
  - OWL DRONE ON KEYBOARD AND MOUSE: hold Delete and move the mouse (your finger on the
    touchpad: swipe to pick the OWL's command).
  - Keyboard and mouse (shooter style, change any key on the launcher's Controls tab):
      WASD move, mouse camera, left click fire, right click aim, Space jump (X),
      C crouch (Circle), R reload/use (Square), Q weapon (Triangle), 1 L1, 3 R1,
      Left Shift sprint (L3), F melee (R3), arrow keys d-pad, Enter Options,
      Tab / Backspace touchpad click left / right. Left Alt frees the cursor.
  - Direct mouse aim (1:1): the mouse turns the camera itself, no emulated stick. Sensitivity
    about 1 is recommended; "Fast turns" (x6) raises the game's turn speed limit while the mouse
    moves. By the game's design walking turns slower than sprinting.
  - Controller: Xbox, DualShock 4, DualSense and other SDL-compatible pads (remappable buttons,
    stick dead zone, vibration, controller speaker volume).
  - Insert (R3 + L2 on a controller): the in-game menu. Most launcher options change there
    while you play (mouse, FOV, controller, display, VSync, sharpening, FSR / DLSS, frame
    generation, texture sharpness, frame rate limit, performance overlay); the few that need a
    restart have an "Apply and restart" button. "Exit game" closes the game.
  - Hold Share / View / Select about a second: quick menu (Resume / Settings / Exit game).
  - FOV: 80% to 150% in the launcher (Game tab) or live in the in-game menu.
  - F12: screenshot.

Where your things are (user\ folder)
  user\ps4\savedata    saved games
  user\ps4\screenshots screenshots
  user\logs            one log per game start (attach it when reporting a bug)
  user\settings.ini    options
  "Clear cache" in the launcher deletes shaders and the prepared image; saves are kept.

Known issues
  - Frame generation adds a little latency; DLSS Frame Generation is not tested yet on
    RTX 40/50 (reports welcome).
  - Classic method above 1080p: lighting flicker and a shadow band following the camera may
    appear, and the few video cutscenes look zoomed in (not with FSR / DLSS).
  - First play: some effects can show up a few frames late while their shader compiles.
  - Above 60 FPS (Graphics, experimental): turn VSync off on a 60 Hz monitor, otherwise it
    stays at 60.
  - Mouse: walking without sprinting turns at the game's own (slower) speed.
  - The in-game menu is in English for now.
  - The shader cache can take several GB.
  - Multiplayer: offline only (Botzone).

Coming next
  Ultrawide (21:9) support; upscaling quality modes (Quality / Balanced / Performance).
  Later: FSR 4; in-game menu in Spanish; debug menu, cheats and patches; native Linux version.

Support
  The port is and will stay free. If you want to support it (optional): the "Support with
  PayPal" button in the launcher, or https://www.paypal.com/paypalme/SantiagoLescano577

Reporting bugs
  Ports Discord: https://discord.gg/UYFcV5rbDw
  or https://github.com/lesca-191204/killzone-shadowfall-pc/issues
  Say what happened, in which chapter or place, your GPU, CPU and driver version, and attach
  the logs folder of that session (Logs button in the launcher).

Credits
  Based on gow3_pc / SpartanPort (Felipe Prado) and the bloodborne_pc lineage (deadinside28,
  Supermedo), with fixes from uncharted_pc (Supermedo) and iss_pc (HayatoG), and the shadPS4
  graphics core (GPL-2.0 license). AMD FSR 3.1 and NVIDIA DLSS (licenses in licenses\).
  Killzone is a trademark of Sony Interactive Entertainment; this project is not affiliated
  with or endorsed by Sony or Guerrilla Games.


[ESPAÑOL]

  Discord de los Ports (reportar problemas, proponer mejoras y enterarte antes de los nuevos
  ports): https://discord.gg/UYFcV5rbDw

Qué es
  Un port no oficial que ejecuta Killzone Shadow Fall (PS4) en Windows. El código del juego
  corre directamente en tu CPU; la parte gráfica se traduce a Vulkan en tiempo real.
  NO incluye ningún archivo del juego: necesitás tu propia copia dumpeada del juego,
  versión 01.81. Si tu dump base es 01.00, también la actualización 01.81.

Versiones del juego
  Desarrollado y probado en CUSA00190 (EE. UU., inglés y francés). El launcher también acepta,
  sin probar (pueden funcionar, sin garantías): CUSA00191 (Latinoamérica), CUSA00002 (Europa) y
  CUSA00085 (Japón; los textos en japonés pueden verse mal o faltar). Es obligatorio que sea la
  versión 01.81 en cualquier región: sin ella, lo más probable es que no funcione. Todas
  comparten las mismas partidas.

Linux y Steam Deck (Proton, no probado por nosotros)
  Todavía no hay versión para Linux. Agregá "Killzone Shadow Fall.exe" a Steam como juego que no
  es de Steam, forzá una versión de Proton en Propiedades > Compatibilidad (Proton Experimental
  o GE-Proton) y abrilo desde Steam. Cuando el juego esté 100% estable va a salir una versión
  nativa para Linux.

Requisitos
  - Windows 10 (1903 o superior) u 11, 64 bits.
  - GPU con Vulkan 1.3. Se recomiendan 6 GB de VRAM para 1080p-1440p (el juego usa unos
    4 GB); 8 GB para 1800p y 4K (unos 5,5 GB).
  - 16 GB de RAM.

Equipo probado
  Ryzen 7 5700 + RTX 2060 Super (8 GB): el juego debería poder jugarse completo, de principio
  a fin. La compilación de shaders casi no da tirones, aun en la primera partida, y los niveles
  cargan rápido. Todavía no fue probado en gráficas AMD ni en procesadores Intel.

Resolución mayor a 1080p (Gráficos > Resolución de render, el método en Avanzado)
  - AMD FSR 3.1 (recomendado, cualquier GPU) o NVIDIA DLSS (GeForce RTX): el juego renderiza en
    1080p y el reescalador lo lleva a 1440p, 1800p o 4K; el HUD, los menús y los videos se
    dibujan nítidos a esa resolución. Nítido (jitter de subpíxel) y estable (los vectores de
    movimiento del propio juego).
  - Clásico: un parche hace que el juego renderice a esa resolución (más exigente, más errores).

Generación de frames (Gráficos > Generación de frames, experimental)
  - AMD FSR 3.1 (cualquier GPU) o NVIDIA DLSS (GeForce RTX 40/50; en otras placas usa FSR 3.1):
    se genera un cuadro entre cada par de cuadros del juego, casi el doble de FPS en pantalla,
    con un poco más de latencia. El HUD y los textos quedan nítidos. A 1080p usa FSR / DLSS
    como antialiasing. Va mejor arriba de 40 FPS; con un monitor de 60 Hz desactivá VSync.

Cómo jugar
  1. Descomprimí esta carpeta donde quieras (fuera de "Archivos de programa").
  2. Abrí "Killzone Shadow Fall.exe".
  3. En la pestaña Juego, Examinar... y elegí la carpeta de tu dump (la que tiene eboot.bin).
  4. JUGAR.

Rendimiento (nuevo en 0.2.5)
  Avanzado > Rendimiento tiene dos opciones, activadas por defecto: "Dibujo en paralelo" y
  "Copias del juego sin esperar a la GPU". Juntas, cerca de +40-50% de FPS en escenas pesadas en
  nuestras pruebas. Si el juego se cierra o algo se ve mal, desactivalas (de a una) y jugás como
  antes; contanos en un issue si eso lo solucionó.

Controles
  - DRON OWL CON TECLADO Y MOUSE: mantené Suprimir y mové el mouse (tu dedo sobre el
    touchpad: deslizá para elegir la orden del OWL).
  - Teclado y mouse (estilo shooter, cualquier tecla se cambia en la pestaña Controles):
      WASD moverse, mouse cámara, click izquierdo disparar, click derecho apuntar,
      Espacio saltar (X), C agacharse (Círculo), R recargar/usar (Cuadrado), Q arma
      (Triángulo), 1 L1, 3 R1, Shift izquierdo correr (L3), F cuerpo a cuerpo (R3),
      flechas cruceta, Enter Options, Tab / Retroceso click del touchpad izquierdo / derecho.
      Alt izquierdo libera el cursor.
  - Mouse directo (1:1): el mouse gira la cámara directamente, sin stick emulado. Se recomienda
    sensibilidad cerca de 1; "Giros rápidos" (x6) sube el límite de giro del juego mientras
    movés el mouse. Por diseño del juego, caminando gira más lento que corriendo.
  - Mando: Xbox, DualShock 4, DualSense y otros compatibles con SDL (botones remapeables, zona
    muerta de sticks, vibración, volumen del altavoz del mando).
  - Insert (R3 + L2 en el mando): el menú dentro del juego. Casi todas las opciones del launcher
    se cambian ahí mientras jugás (mouse, FOV, mando, pantalla, VSync, nitidez, FSR / DLSS,
    generación de frames, nitidez de texturas, límite de FPS, overlay de rendimiento); las pocas
    que necesitan reinicio tienen un botón "Apply and restart". "Exit game" cierra el juego.
  - Mantener Share / View / Select un segundo: menú rápido (Continuar / Ajustes / Salir).
  - FOV: de 80% a 150% en el launcher (pestaña Juego) o en vivo en el menú del juego.
  - F12: captura de pantalla.

Dónde quedan tus cosas (carpeta user\)
  user\ps4\savedata    partidas guardadas
  user\ps4\screenshots capturas
  user\logs            un registro por cada vez que jugás (adjuntalos si reportás un error)
  user\settings.ini    opciones
  "Borrar caché" en el launcher elimina shaders e imagen preparada; tus partidas no se tocan.

Problemas conocidos
  - La generación de frames suma un poco de latencia; la de DLSS todavía no fue probada en
    RTX 40/50 (se agradecen reportes).
  - Método clásico por encima de 1080p: pueden aparecer parpadeos de iluminación y una franja
    de sombra que sigue a la cámara, y las pocas cinemáticas que son video se ven con zoom
    (no pasa con FSR / DLSS).
  - Primera partida: algunos efectos pueden aparecer unos cuadros tarde mientras se compila su shader.
  - Más de 60 FPS (Gráficos, experimental): con un monitor de 60 Hz desactivá VSync, si no se
    queda en 60.
  - Mouse: caminando sin correr, la cámara gira a la velocidad (más lenta) del propio juego.
  - Por ahora el menú dentro del juego está en inglés.
  - La caché de shaders puede ocupar varios GB.
  - Multijugador: solo sin conexión (Botzone).

Próximamente
  Soporte ultrawide (21:9); calidades de reescalado (Calidad / Equilibrado / Rendimiento).
  Más adelante: FSR 4; menú del juego en español; menú debug, trucos y parches; versión nativa
  para Linux.

Apoyar el proyecto
  El port es y seguirá siendo gratuito. Si querés apoyarlo (opcional): el botón "Apoyar con
  PayPal" del launcher, o https://www.paypal.com/paypalme/SantiagoLescano577

Reportar errores
  Discord de los Ports: https://discord.gg/UYFcV5rbDw
  o https://github.com/lesca-191204/killzone-shadowfall-pc/issues
  Contá qué pasó, en qué capítulo o lugar, tu GPU, CPU y versión del driver, y adjuntá la
  carpeta de logs de esa partida (botón Logs en el launcher).

Créditos
  Basado en gow3_pc / SpartanPort (Felipe Prado) y la línea bloodborne_pc (deadinside28,
  Supermedo), con arreglos de uncharted_pc (Supermedo) e iss_pc (HayatoG), y el núcleo gráfico
  de shadPS4 (licencia GPL-2.0). AMD FSR 3.1 y NVIDIA DLSS (licencias en licenses\).
  Killzone es una marca de Sony Interactive Entertainment; este proyecto no está afiliado ni
  respaldado por Sony ni por Guerrilla Games.
