# Killzone Shadow Fall for PC

**Español** | [English](#english)

Port nativo para Windows de **Killzone Shadow Fall** (PS4). El código del juego corre directamente
en la CPU; los gráficos se traducen a Vulkan en tiempo real. Beta pública: se puede jugar la
campaña completa, se sigue puliendo con los reportes de los testers.

> **No incluye archivos del juego.** Necesitás un dump de tu propia PS4: **CUSA00190, versión
> 01.81** (desencriptado, por ejemplo con GoldHEN). Este proyecto no está afiliado a Sony
> Interactive Entertainment ni a Guerrilla Games. Killzone es una marca de Sony Interactive
> Entertainment.

## Descargar y jugar

1. Bajá el zip de la última versión en [Releases](../../releases) y descomprimilo donde quieras
   (fuera de "Archivos de programa").
2. Abrí **Killzone Shadow Fall.exe**, elegí la carpeta de tu dump (la que tiene `eboot.bin`) y
   dale a **JUGAR**.

Requisitos: Windows 10 (1903+) u 11 de 64 bits, GPU con Vulkan 1.3 (8 GB de VRAM recomendados),
16 GB de RAM. Probado en Ryzen 7 5700 + RTX 2060 Super; AMD e Intel todavía no están probadas.

## Qué incluye

- Launcher en español e inglés: verificación del dump, modo de pantalla, tamaño de ventana,
  límite de 30/60 FPS, VSync, filtrado anisotrópico, FSR 1, controles de teclado, mouse y mando.
- Mouse para apuntar (como stick derecho), teclado y mandos Xbox / PlayStation.
- Touchpad sin touchpad (dron OWL): mantener **Suprimir** y arrastrar el mouse.
- Menú dentro del juego con **Insert** (R3 + L2 en el mando); **F12** captura de pantalla.

Problemas conocidos: tirones la primera vez que aparece un efecto (compilación de shaders, luego
quedan en caché), caché de shaders de varios GB, multijugador solo sin conexión (Botzone).

## Reportar errores

Abrí un [issue](../../issues) con qué pasó, dónde (capítulo), tu GPU, CPU y driver, y adjuntá la
carpeta de esa partida en `user\logs\` (botón **Logs** del launcher).

## Compilar desde el código

Igual que la base gow3_pc ([instrucciones](packaging/windows/README.md)): MSYS2 CLANG64,
`bash build.sh`, y `bash packaging/windows/package_kzsf.sh` para el zip (antes,
`packaging/windows/make_kzsf_icon.py <tu dump>/sce_sys/icon0.png launcher/kzsf.ico`).

---

## English

Native Windows port of **Killzone Shadow Fall** (PS4). The game's code runs directly on the CPU;
graphics are translated to Vulkan in real time. Public beta: the whole campaign is playable, and
it keeps improving with testers' reports.

> **No game files are included.** You need a dump from your own PS4: **CUSA00190, version 01.81**
> (decrypted, e.g. with GoldHEN). This project is not affiliated with Sony Interactive
> Entertainment or Guerrilla Games. Killzone is a trademark of Sony Interactive Entertainment.

**Play:** download the latest zip from [Releases](../../releases), unzip it, open
**Killzone Shadow Fall.exe**, choose your dump's folder (the one holding `eboot.bin`), press
**PLAY**. Requirements: Windows 10 (1903+) or 11 64-bit, a Vulkan 1.3 GPU (8 GB VRAM
recommended), 16 GB RAM. Tested on a Ryzen 7 5700 + RTX 2060 Super; AMD and Intel GPUs untested.

**Controls:** controller (Xbox / PlayStation), keyboard and mouse (mouse aim as the right stick).
Touchpad without one (OWL drone): hold **Delete** and drag the mouse. **Insert**: in-game menu.

**Bugs:** open an [issue](../../issues) with what happened, where, your GPU/CPU/driver, and that
session's folder from `user\logs\` (launcher **Logs** button).

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
