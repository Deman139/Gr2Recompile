KILLZONE SHADOW FALL - Native Windows port (Lesca) - Beta 0.2.3
=================================================================

[ENGLISH]

What it is
  An unofficial port that runs Killzone Shadow Fall (PS4) on Windows. The game's code runs
  directly on your CPU; graphics are translated to Vulkan in real time.
  It does NOT include any game files: you need your own dumped copy of the game,
  CUSA00190 version 01.81. If your base dump is 01.00, also the 01.81 update.

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

Controls
  - Controller: Xbox, DualShock 4, DualSense and other SDL-compatible pads.
  - Keyboard and mouse: WASD to move, mouse for the camera, left click fires, right click aims.
    Change everything on the Controls tab (sensitivity, Y axis). Left Alt frees the cursor.
  - Touchpad (OWL drone) without a touchpad: hold Delete and drag the mouse.
  - Insert: the port's menu while playing (R3 + L2 on a controller).
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
  - The mouse is translated to the right stick: smooth, but not 1:1 like a PC shooter.
  - The shader cache can take several GB.
  - Multiplayer: offline only (Botzone).

Future plans
  High priority:   performance (better use of the GPU and CPU); mouse input (direct camera
                   control); launcher options in the in-game menu (Insert).
  Medium priority: FSR 4; ultrawide (21:9); debug menu, cheats and patches (e.g. unlock
                   every chapter); fixes for bugs reported on specific hardware.

Support
  The port is and will stay free. If you want to support it (optional): the "Support with
  PayPal" button in the launcher, or https://www.paypal.com/paypalme/SantiagoLescano577

Reporting bugs
  https://github.com/lesca-191204/killzone-shadowfall-pc/issues
  Say what happened, in which chapter or place, your GPU, CPU and driver version, and attach
  the logs folder of that session (Logs button in the launcher).

Credits
  Based on gow3_pc / SpartanPort (Felipe Prado) and the bloodborne_pc lineage (deadinside28,
  Supermedo), with fixes from uncharted_pc (Supermedo) and iss_pc (HayatoG), and the shadPS4
  graphics core (GPL-2.0 license). AMD FSR 3.1 and NVIDIA DLSS (licenses in licenses\).
  Killzone is a trademark of Sony Interactive Entertainment; this project is not affiliated
  with or endorsed by Sony or Guerrilla Games.


[ESPAÑOL]

Qué es
  Un port no oficial que ejecuta Killzone Shadow Fall (PS4) en Windows. El código del juego
  corre directamente en tu CPU; la parte gráfica se traduce a Vulkan en tiempo real.
  NO incluye ningún archivo del juego: necesitás tu propia copia dumpeada del juego,
  CUSA00190 versión 01.81. Si tu dump base es 01.00, también la actualización 01.81.

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

Controles
  - Mando: Xbox, DualShock 4, DualSense y otros compatibles con SDL.
  - Teclado y mouse: WASD moverse, mouse cámara, click izquierdo dispara, click derecho apunta.
    Todo se cambia en la pestaña Controles (sensibilidad, eje Y). Alt izquierdo libera el cursor.
  - Touchpad (dron OWL) sin touchpad: mantené Suprimir y arrastrá el mouse.
  - Insert: menú del port durante el juego (también R3 + L2 en el mando).
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
  - El mouse se traduce a stick derecho: fluido, pero no 1 a 1 como un shooter de PC.
  - La caché de shaders puede ocupar varios GB.
  - Multijugador: solo sin conexión (Botzone).

Planes futuros
  Prioridad alta:  rendimiento (mejor uso de GPU y CPU); input del mouse (control directo de
                   la cámara); opciones del launcher en el menú del juego (Insert).
  Prioridad media: FSR 4; ultrawide (21:9); menú debug, trucos y parches (p. ej. desbloquear
                   todos los capítulos); arreglos de errores reportados en hardware específico.

Apoyar el proyecto
  El port es y seguirá siendo gratuito. Si querés apoyarlo (opcional): el botón "Apoyar con
  PayPal" del launcher, o https://www.paypal.com/paypalme/SantiagoLescano577

Reportar errores
  https://github.com/lesca-191204/killzone-shadowfall-pc/issues
  Contá qué pasó, en qué capítulo o lugar, tu GPU, CPU y versión del driver, y adjuntá la
  carpeta de logs de esa partida (botón Logs en el launcher).

Créditos
  Basado en gow3_pc / SpartanPort (Felipe Prado) y la línea bloodborne_pc (deadinside28,
  Supermedo), con arreglos de uncharted_pc (Supermedo) e iss_pc (HayatoG), y el núcleo gráfico
  de shadPS4 (licencia GPL-2.0). AMD FSR 3.1 y NVIDIA DLSS (licencias en licenses\).
  Killzone es una marca de Sony Interactive Entertainment; este proyecto no está afiliado ni
  respaldado por Sony ni por Guerrilla Games.
