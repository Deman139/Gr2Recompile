KILLZONE SHADOW FALL - Native Windows port (Lesca) - Beta 0.1.1
=================================================================

[ENGLISH]

What it is
  An unofficial port that runs Killzone Shadow Fall (PS4) on Windows. The game's code runs
  directly on your CPU; graphics are translated to Vulkan in real time.
  It does NOT include any game files: you need your own dumped copy of the game,
  CUSA00190 version 01.81. If your base dump is 01.00, also the 01.81 update.

Requirements
  - Windows 10 (1903 or later) or 11, 64-bit.
  - A Vulkan 1.3 GPU. We don't recommend playing with less than 8 GB of VRAM.
  - 16 GB of RAM.

Tested setup
  Ryzen 7 5700 + RTX 2060 Super: the game should be playable from start to finish.
  The first times, and the first loads above all, can stutter quite a bit while shaders
  compile; once loaded the game runs well, 40-60 FPS on that GPU.
  Not tested yet on AMD GPUs or Intel CPUs.

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
  - First campaign load: the very first time you start the campaign it can take a long time
    while many shaders compile, and it may even crash. Start the game again and it should
    load fine: the shaders stay cached.
  - Stutters the first time an effect appears (the shader compiles, then stays cached).
  - Chapter 2: slight lighting flicker at some moments while shaders load; it goes away
    with time.
  - No resolution scaling yet: the game draws internally at a fixed 1080p, and its render
    targets, shadows and post-processing effects are all sized for it. Rendering higher means
    patching every place in the game's code that assumes 1080p; doing it partially breaks
    effects. Meanwhile, on screens above 1080p, turn on "FSR 1 upscaling" (Graphics).
  - The mouse is translated to the right stick: smooth, but not 1:1 like a PC shooter.
  - The shader cache can take several GB.
  - Multiplayer: offline only (Botzone).

Future plans
  High priority:   resolution scaling up to 4K; GPU usage and shader loading optimizations.
  Medium priority: unlocked frame rate above 60 FPS (120, 144, 240...).
  Later:           DLSS, FSR 3.1 and FSR 4 upscalers; any patches or cheats needed.

Reporting bugs
  https://github.com/lesca-191204/killzone-shadowfall-pc/issues
  Say what happened, in which chapter or place, your GPU, CPU and driver version, and attach
  the logs folder of that session (Logs button in the launcher).

Credits
  Based on gow3_pc (Felipe Prado) and the bloodborne_pc lineage (deadinside28, Supermedo),
  the uncharted_pc mouse layer and the shadPS4 graphics core (GPL-2.0 license).
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
  - GPU con Vulkan 1.3. No recomendamos jugarlo con menos de 8 GB de VRAM.
  - 16 GB de RAM.

Equipo probado
  Ryzen 7 5700 + RTX 2060 Super: el juego debería poder jugarse completo, de principio a fin.
  Las primeras veces, y sobre todo en las primeras cargas, puede dar bastantes tirones
  mientras se compilan los shaders; una vez cargado corre bastante bien, 40-60 FPS en esa
  gráfica. Todavía no fue probado en gráficas AMD ni en procesadores Intel.

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
  - Primera carga de la campaña: la primera vez que iniciás la campaña puede demorar bastante
    mientras se compilan muchos shaders, e incluso puede crashear. Volvé a entrar y debería
    cargar bien: los shaders quedan en caché.
  - Tirones la primera vez que aparece un efecto (el shader se compila y después queda en caché).
  - Capítulo 2: parpadeo leve de iluminación en ciertos momentos de carga de shaders; se
    soluciona con el tiempo.
  - Todavía no se puede escalar la resolución: el juego dibuja internamente a 1080p fijo, y sus
    buffers de render, sombras y post-procesado están hechos para ese tamaño. Renderizar a más
    resolución obliga a parchear todos los lugares del código del juego que asumen 1080p;
    hacerlo a medias rompe efectos. Mientras tanto, en pantallas de más de 1080p, activá
    "Escalado FSR 1" (Gráficos).
  - El mouse se traduce a stick derecho: fluido, pero no 1 a 1 como un shooter de PC.
  - La caché de shaders puede ocupar varios GB.
  - Multijugador: solo sin conexión (Botzone).

Planes futuros
  Prioridad alta:  escala de resolución hasta 4K; optimizaciones de uso de GPU y carga de shaders.
  Prioridad media: desbloqueo de FPS por encima de 60 (120, 144, 240...).
  Más adelante:    reescalado DLSS, FSR 3.1 y FSR 4; parches o trucos que hagan falta.

Reportar errores
  https://github.com/lesca-191204/killzone-shadowfall-pc/issues
  Contá qué pasó, en qué capítulo o lugar, tu GPU, CPU y versión del driver, y adjuntá la
  carpeta de logs de esa partida (botón Logs en el launcher).

Créditos
  Basado en gow3_pc (Felipe Prado) y la línea bloodborne_pc (deadinside28, Supermedo), la capa
  de mouse de uncharted_pc y el núcleo gráfico de shadPS4 (licencia GPL-2.0).
  Killzone es una marca de Sony Interactive Entertainment; este proyecto no está afiliado ni
  respaldado por Sony ni por Guerrilla Games.
