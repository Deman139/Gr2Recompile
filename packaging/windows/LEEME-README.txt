KILLZONE SHADOW FALL - Port nativo para Windows (Lesca) - Beta 0.1.1
==================================================================

[ESPAÑOL]

Qué es
  Un port no oficial que ejecuta Killzone Shadow Fall (PS4) en Windows. El código del juego
  corre directamente en tu CPU; la parte gráfica se traduce a Vulkan en tiempo real.
  NO incluye ningún archivo del juego: necesitás tu propio dump de PS4.

Requisitos
  - Windows 10 (1903 o superior) u 11, 64 bits.
  - CPU x86-64 moderna. Probado en Ryzen 7 5700.
  - GPU con Vulkan 1.3 y 8 GB de VRAM recomendados. Probado en NVIDIA RTX 2060 Super;
    AMD e Intel todavía no están probadas.
  - 16 GB de RAM recomendados.
  - Tu dump de Killzone Shadow Fall: CUSA00190 versión 01.81, desencriptado
    (por ejemplo, con GoldHEN). Si tu dump base es 01.00, también la actualización 01.81.

Cómo jugar
  1. Descomprimí esta carpeta donde quieras (fuera de "Archivos de programa").
  2. Abrí "Killzone Shadow Fall.exe".
  3. En la pestaña Juego, Examinar... y elegí la carpeta de tu dump (la que tiene eboot.bin).
  4. JUGAR. El primer inicio tarda más: se prepara el juego y se compilan shaders.

Controles
  - Mando: Xbox, DualShock 4, DualSense y otros compatibles con SDL.
  - Teclado y mouse: WASD moverse, mouse cámara, click izquierdo dispara, click derecho apunta.
    Todo se cambia en la pestaña Controles (sensibilidad, eje Y). Alt izquierdo libera el cursor.
  - Touchpad (dron OWL) sin touchpad: mantené Suprimir y arrastrá el mouse.
  - Insert: menú del port durante el juego (también R3 + L2 en el mando).
  - F12: captura de pantalla.

Dónde quedan tus cosas (carpeta user\)
  user\ps4\savedata   partidas guardadas
  user\ps4\screenshots capturas
  user\logs           un registro por cada vez que jugás (adjuntalos si reportás un error)
  user\settings.ini   opciones
  "Borrar caché" en el launcher elimina shaders e imagen preparada; tus partidas no se tocan.

Problemas conocidos (beta)
  - Tirones la primera vez que aparece un efecto nuevo: los shaders se compilan y quedan en caché.
  - La caché de shaders puede ocupar varios GB.
  - Multijugador: solo modo sin conexión (Botzone).
  - El juego se dibuja a 1080p. En pantallas más grandes, activá "Escalado FSR 1" (Gráficos).
  - El mouse se traduce a stick derecho: fluido, pero no 1 a 1 como un shooter de PC.
  - La primera vez que se arranca un capítulo puede tardar.

Reportar errores
  Contá qué pasó, en qué capítulo o lugar, tu GPU, CPU y versión del driver, y adjuntá la
  carpeta de logs de esa partida (botón Logs en el launcher).

Créditos
  Basado en gow3_pc (FelipePrado0) y en la línea bbport / bloodborne_pc, con el núcleo gráfico
  de shadPS4 (licencia GPL-2.0). Killzone es una marca de Sony Interactive Entertainment;
  este proyecto no está afiliado ni respaldado por Sony ni por Guerrilla Games.


[ENGLISH]

What it is
  An unofficial port that runs Killzone Shadow Fall (PS4) on Windows. The game's code runs
  directly on your CPU; graphics are translated to Vulkan in real time.
  It does NOT include any game files: you need your own PS4 dump.

Requirements
  - Windows 10 (1903 or later) or 11, 64-bit.
  - A modern x86-64 CPU. Tested on a Ryzen 7 5700.
  - A Vulkan 1.3 GPU; 8 GB of VRAM recommended. Tested on an NVIDIA RTX 2060 Super;
    AMD and Intel are not tested yet.
  - 16 GB of RAM recommended.
  - Your Killzone Shadow Fall dump: CUSA00190 version 01.81, decrypted (e.g. with GoldHEN).
    If your base dump is 01.00, also the 01.81 update.

How to play
  1. Unzip this folder anywhere (outside "Program Files").
  2. Open "Killzone Shadow Fall.exe".
  3. On the Game tab, Browse... and choose your dump's folder (the one holding eboot.bin).
  4. PLAY. The first start takes longer: the game is prepared and shaders are compiled.

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

Known issues (beta)
  - Stutters the first time a new effect appears: shaders compile, then stay cached.
  - The shader cache can take several GB.
  - Multiplayer: offline mode only (Botzone).
  - The game draws at 1080p. On larger screens, turn on "FSR 1 upscaling" (Graphics).
  - The mouse is translated to the right stick: smooth, but not 1:1 like a PC shooter.
  - Starting a chapter for the first time can take a while.

Reporting bugs
  Say what happened, in which chapter or place, your GPU, CPU and driver version, and attach
  the logs folder of that session (Logs button in the launcher).

Credits
  Based on gow3_pc (FelipePrado0) and the bbport / bloodborne_pc lineage, with the shadPS4
  graphics core (GPL-2.0 license). Killzone is a trademark of Sony Interactive Entertainment;
  this project is not affiliated with or endorsed by Sony or Guerrilla Games.
