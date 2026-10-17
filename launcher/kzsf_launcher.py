#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Killzone Shadow Fall (PC port) launcher for Windows (Tkinter).

One window: the game folder (CUSA00190, version 01.81, from the player's own dump), basic
graphics options (settings.ini, shared with the in-game menu), keyboard and controller mapping
(GOW3_KEY_MAP / GOW3_PAD_MAP), and shortcuts to saves, logs and the cache.

Everything the port writes lives in user/ beside the launcher:
  user/launcher.json   launcher choices (game folder, language, mappings)
  user/settings.ini    game options (GOW3_CONFIG; the in-game menu writes it too)
  user/out/            the prepared game image (GOW3_DATA_DIR/out; rebuilt when missing)
  user/ps4/            the console's user folder: savedata, screenshots, shader cache
  user/logs/<time>/    one folder per game start

Frozen with PyInstaller (packaging/windows/package_kzsf.sh), the same executable also runs
run.py (`--run`) and the preparation scripts (`--script`), so players need no Python.
The artwork (banner, icon) comes from the player's game folder: nothing of the game ships.
"""
import json
import os
from pathlib import Path
import runpy
import shutil
import subprocess
import sys
import threading
import time
import webbrowser
import zipfile

import kzsf_update

WINDOWS = sys.platform == 'win32'
if WINDOWS:
    import ctypes
    # AMD hybrid laptops: this layer hides the discrete GPU from Vulkan and the game stays black.
    os.environ.setdefault('VK_LOADER_LAYERS_DISABLE', 'VK_LAYER_AMD_switchable_graphics')
FROZEN = getattr(sys, 'frozen', False)
PORT_DIR = Path(sys.executable).resolve().parent if FROZEN else Path(__file__).resolve().parent.parent
sys.path.insert(0, str(PORT_DIR / 'scripts'))

VERSION = '0.2.0'
TITLE_ID = 'CUSA00190'
GAME_VERSION = '01.81'
USER_DIR = Path(os.environ.get('KZSF_USER_DIR', PORT_DIR / 'user'))
LAUNCHER_FILE = USER_DIR / 'launcher.json'
SETTINGS_FILE = USER_DIR / 'settings.ini'
PS4_DIR = USER_DIR / 'ps4'
LOGS_DIR = USER_DIR / 'logs'
UPDATE_DIR = USER_DIR / 'update'
KEEP_LOGS = 10
NO_WINDOW = getattr(subprocess, 'CREATE_NO_WINDOW', 0)

# Killzone Shadow Fall's menus: near black, white text, cyan highlight.
BG = '#0a0e11'
PANEL = '#11181d'
FIELD = '#18222a'
LINE = '#26343d'
TEXT = '#e9eff3'
MUTED = '#8796a0'
ACCENT = '#35c8e4'
ACCENT_DARK = '#1d8ea6'
OK = '#5ad48a'
WARN = '#e8b13a'
ERROR = '#e46060'
MARK = '#2f8bff'  # "Lesca": marks this build as ours
UI_FONT = 'Segoe UI'


# ---------------------------------------------------------------------------------------------
# Texts (Spanish and English).

TEXTS = {
    'es': {
        'tab_game': 'Juego', 'tab_graphics': 'Gráficos', 'tab_controls': 'Controles', 'tab_advanced': 'Avanzado',
        'play': 'JUGAR', 'playing': 'EN JUEGO', 'preparing': 'Iniciando…',
        'game_folder': 'Carpeta del juego',
        'game_folder_help': f'La carpeta de tu dump de PS4 ({TITLE_ID}, versión {GAME_VERSION}): la que contiene eboot.bin.',
        'update_folder': 'Carpeta de la actualización',
        'update_folder_help': f'Tu dump base no es la versión {GAME_VERSION}: elegí la carpeta de la actualización {GAME_VERSION} (CUSA00190-patch).',
        'browse': 'Examinar…', 'choose_game': 'Elegí la carpeta del juego (la que tiene eboot.bin)',
        'choose_update': 'Elegí la carpeta de la actualización',
        'no_folder': 'Todavía no elegiste la carpeta del juego.',
        'no_eboot': 'En esa carpeta no hay eboot.bin.',
        'no_sfo': 'Falta sce_sys/param.sfo: el dump está incompleto.',
        'wrong_game': 'Esa carpeta es de otro juego ({id}). Este port es para Killzone Shadow Fall ({want}).',
        'need_update': 'Versión {ver}: falta la actualización {want}.',
        'bad_update': 'La carpeta de actualización no es {want} versión {ver_want} (es {id} {ver}).',
        'ready': 'Listo: {title} · {id} · versión {ver}',
        'ready_update': 'Listo: {title} · {id} · versión {ver} (con actualización)',
        'tools': 'Carpetas y mantenimiento',
        'open_saves': 'Partidas guardadas', 'open_shots': 'Capturas', 'open_logs': 'Logs',
        'clear_cache': 'Borrar caché',
        'cache_size': 'Caché: {size}. Se vuelve a crear al jugar (el primer inicio y las zonas nuevas tardan más).',
        'clear_confirm': 'Se borra la caché de shaders y la imagen preparada del juego ({size}).\n'
                         'Tus partidas guardadas no se tocan. ¿Continuar?',
        'cleared': 'Caché borrada.',
        'busy_running': 'Cerrá el juego primero.',
        'display_mode': 'Modo de pantalla', 'mode_windowed': 'Ventana', 'mode_borderless': 'Ventana sin bordes',
        'mode_fullscreen': 'Pantalla completa',
        'window_size': 'Tamaño de ventana',
        'window_size_help': 'Solo en modo ventana. La imagen del juego se escala a la ventana.',
        'render_res': 'Resolución de render', 'res_native': '1080p (como en PS4)',
        'render_res_help': 'Resolución interna del juego. Más alta = más nítida pero más GPU y VRAM. '
                           'Probado con 8 GB de VRAM hasta 4K (~5 GB). Se aplica al iniciar el juego.',
        'fps_limit': 'Límite de FPS', 'vsync': 'VSync (sincronización vertical)',
        'fps_unlimited': 'Sin límite (experimental)', 'fps_experimental': 'experimental',
        'fps_limit_help': '144 / 240 / sin límite son experimentales. Con un monitor de 60 Hz desactivá '
                          'VSync o quedará en 60. Se aplica al iniciar el juego.',
        'vsync_help': 'Evita el tearing. Desactivado: un poco menos de latencia.',
        'anisotropy': 'Filtrado anisotrópico', 'aniso_game': 'El del juego',
        'sharpen': 'Nitidez (RCAS)', 'show_fps': 'Mostrar FPS en pantalla',
        'fsr1': 'Escalado FSR 1',
        'fsr1_help': 'Para pantallas de más de 1080p (1440p, 4K): imagen más nítida que el estirado normal. '
                     'A 1080p no cambia nada.',
        'graphics_note': 'Durante el juego, Insert abre el menú del port (también con el mando: R3 + L2).',
        'keyboard': 'Teclado', 'controller': 'Mando',
        'press_key': 'Pulsá una tecla…', 'reset_defaults': 'Valores por defecto',
        'controls_help': 'Click en una tecla y apretá otra tecla, un botón del mouse o la rueda (Esc cancela). Movimiento: WASD. Cámara: flechas.\n'
                         'Touchpad (dron OWL): mantené Suprimir y arrastrá el mouse. Tab / Retroceso: click izquierdo / '
                         'derecho del touchpad.',
        'controller_help': 'Botón del mando que hace de cada botón de PS4. Los gatillos y sticks no se cambian.',
        'vibration': 'Vibración',
        'mouse': 'Mouse', 'mouse_look': 'Apuntar con el mouse',
        'mouse_look_help': 'El mouse mueve la cámara; click izquierdo dispara (R2), derecho apunta (L2). '
                           'Alt izquierdo libera el cursor.',
        'mouse_sens': 'Sensibilidad', 'mouse_invert': 'Invertir eje Y',
        'advanced_help': 'Solo si algo se ve mal o el juego no funciona bien. Si tenés dudas, dejalo como está.',
        'async_shaders': 'Compilar shaders nuevos en segundo plano',
        'async_help': 'Menos tirones en zonas nuevas; un objeto puede aparecer un instante después.',
        'stale_readback': 'Lecturas de GPU sin esperar',
        'stale_help': 'Más FPS. Desactivalo si la iluminación u objetos se ven mal.',
        'deferred_readback': 'Lecturas de GPU diferidas',
        'deferred_help': 'Menos esperas. Desactivalo si texturas o sombras se ven mal.',
        'frames_queued': 'Frames en cola', 'queued_1': '1 (menor latencia)', 'queued_2': '2', 'queued_0': 'Sin límite',
        'reset_all': 'Restaurar todas las opciones',
        'update_available': 'Nueva versión {version} disponible', 'update_now': 'Actualizar ahora',
        'update_notes': 'Ver novedades', 'update_later': 'Más tarde',
        'update_confirm': 'Se va a descargar la versión {version} ({size}) y el launcher se va a reiniciar.\n'
                          'Tus partidas, opciones y caché (carpeta user) no se tocan.\n\n¿Actualizar ahora?',
        'update_downloading': 'Descargando actualización… {percent} %',
        'update_unpacking': 'Preparando actualización…',
        'update_failed': 'No se pudo actualizar: {error}\n\nPodés descargarla a mano desde GitHub.',
        'update_none': 'Ya tenés la última versión ({version}).',
        'update_check_failed': 'No se pudo buscar actualizaciones: {error}',
        'update_close_game': 'Cerrá el juego antes de actualizar.',
        'update_previous_failed': 'La última actualización no se pudo aplicar. Probá de nuevo o descargala desde GitHub.',
        'check_updates': 'Buscar actualizaciones al iniciar', 'check_updates_now': 'Buscar actualizaciones',
        'updates': 'Actualizaciones',
        'reset_all_confirm': '¿Volver todas las opciones gráficas, avanzadas y de controles a los valores por defecto?',
        'language': 'Idioma',
        'game_ended': 'El juego se cerró con un error (código {code}).\n\n¿Abrir la carpeta de logs para ver qué pasó?',
        'cant_start': 'No se pudo iniciar el juego:\n{error}',
        'fix_folder': 'Primero elegí una carpeta del juego válida (pestaña Juego).',
        'footer_hint': 'Port nativo para Windows · usa tu propio dump',
    },
    'en': {
        'tab_game': 'Game', 'tab_graphics': 'Graphics', 'tab_controls': 'Controls', 'tab_advanced': 'Advanced',
        'play': 'PLAY', 'playing': 'IN GAME', 'preparing': 'Starting…',
        'game_folder': 'Game folder',
        'game_folder_help': f'The folder of your PS4 dump ({TITLE_ID}, version {GAME_VERSION}): the one holding eboot.bin.',
        'update_folder': 'Update folder',
        'update_folder_help': f'Your base dump is not version {GAME_VERSION}: choose the folder of update {GAME_VERSION} (CUSA00190-patch).',
        'browse': 'Browse…', 'choose_game': 'Choose the game folder (the one with eboot.bin)',
        'choose_update': 'Choose the update folder',
        'no_folder': 'No game folder chosen yet.',
        'no_eboot': 'There is no eboot.bin in that folder.',
        'no_sfo': 'sce_sys/param.sfo is missing: the dump is incomplete.',
        'wrong_game': 'That folder holds another game ({id}). This port is for Killzone Shadow Fall ({want}).',
        'need_update': 'Version {ver}: update {want} is missing.',
        'bad_update': 'The update folder is not {want} version {ver_want} (it is {id} {ver}).',
        'ready': 'Ready: {title} · {id} · version {ver}',
        'ready_update': 'Ready: {title} · {id} · version {ver} (with update)',
        'tools': 'Folders and maintenance',
        'open_saves': 'Saved games', 'open_shots': 'Screenshots', 'open_logs': 'Logs',
        'clear_cache': 'Clear cache',
        'cache_size': 'Cache: {size}. It is rebuilt while playing (the first start and new areas take longer).',
        'clear_confirm': 'The shader cache and the prepared game image ({size}) will be deleted.\n'
                         'Your saved games are kept. Continue?',
        'cleared': 'Cache cleared.',
        'busy_running': 'Close the game first.',
        'display_mode': 'Display mode', 'mode_windowed': 'Windowed', 'mode_borderless': 'Borderless window',
        'mode_fullscreen': 'Fullscreen',
        'window_size': 'Window size',
        'window_size_help': 'Windowed mode only. The game image is scaled to the window.',
        'render_res': 'Render resolution', 'res_native': '1080p (as on PS4)',
        'render_res_help': "The game's internal resolution. "
                           'Higher = sharper but more GPU and VRAM. '
                           'Tested with 8 GB of VRAM up to 4K (~5 GB). Applied when the game starts.',
        'fps_limit': 'Frame rate limit', 'vsync': 'VSync (vertical sync)',
        'fps_unlimited': 'Unlimited (experimental)', 'fps_experimental': 'experimental',
        'fps_limit_help': '144 / 240 / unlimited are experimental. On a 60 Hz monitor turn VSync off '
                          'or it stays at 60. Applied when the game starts.',
        'vsync_help': 'Prevents tearing. Off: slightly lower latency.',
        'anisotropy': 'Anisotropic filtering', 'aniso_game': 'Game default',
        'sharpen': 'Sharpening (RCAS)', 'show_fps': 'Show FPS on screen',
        'fsr1': 'FSR 1 upscaling',
        'fsr1_help': 'For screens above 1080p (1440p, 4K): sharper than plain stretching. No change at 1080p.',
        'graphics_note': 'In game, Insert opens the port menu (on a controller: R3 + L2).',
        'keyboard': 'Keyboard', 'controller': 'Controller',
        'press_key': 'Press a key…', 'reset_defaults': 'Defaults',
        'controls_help': 'Click a key, then press a key, a mouse button or the wheel (Esc cancels). Move: WASD. Camera: arrow keys.\n'
                         'Touchpad (OWL drone): hold Delete and drag the mouse. Tab / Backspace: left / right '
                         'touchpad click.',
        'controller_help': 'The controller button that acts as each PS4 button. Triggers and sticks are fixed.',
        'vibration': 'Vibration',
        'mouse': 'Mouse', 'mouse_look': 'Aim with the mouse',
        'mouse_look_help': 'The mouse moves the camera; left click fires (R2), right click aims (L2). '
                           'Left Alt frees the cursor.',
        'mouse_sens': 'Sensitivity', 'mouse_invert': 'Invert Y axis',
        'advanced_help': 'Only if something looks wrong or the game misbehaves. When in doubt, leave it as it is.',
        'async_shaders': 'Compile new shaders in the background',
        'async_help': 'Fewer stutters in new areas; an object may appear a moment later.',
        'stale_readback': 'GPU readbacks without waiting',
        'stale_help': 'More FPS. Turn it off if lighting or objects look wrong.',
        'deferred_readback': 'Deferred GPU readbacks',
        'deferred_help': 'Fewer waits. Turn it off if textures or shadows look wrong.',
        'frames_queued': 'Frames queued', 'queued_1': '1 (lowest latency)', 'queued_2': '2', 'queued_0': 'Unlimited',
        'reset_all': 'Restore all options',
        'update_available': 'New version {version} available', 'update_now': 'Update now',
        'update_notes': "What's new", 'update_later': 'Later',
        'update_confirm': 'Version {version} ({size}) will be downloaded and the launcher will restart.\n'
                          'Your saves, settings and cache (user folder) are kept.\n\nUpdate now?',
        'update_downloading': 'Downloading update… {percent} %',
        'update_unpacking': 'Preparing update…',
        'update_failed': 'The update failed: {error}\n\nYou can download it by hand from GitHub.',
        'update_none': 'You have the latest version ({version}).',
        'update_check_failed': 'Could not check for updates: {error}',
        'update_close_game': 'Close the game before updating.',
        'update_previous_failed': 'The last update could not be applied. Try again or download it from GitHub.',
        'check_updates': 'Check for updates at startup', 'check_updates_now': 'Check for updates',
        'updates': 'Updates',
        'reset_all_confirm': 'Set every graphics, advanced and control option back to its default?',
        'language': 'Language',
        'game_ended': 'The game closed with an error (code {code}).\n\nOpen the logs folder to see what happened?',
        'cant_start': 'The game could not start:\n{error}',
        'fix_folder': 'Choose a valid game folder first (Game tab).',
        'footer_hint': 'Native Windows port · uses your own dump',
    },
}
LANG = 'es'


def tr(key, **values):
    text = TEXTS.get(LANG, TEXTS['en']).get(key) or TEXTS['en'].get(key, key)
    return text.format(**values) if values else text


def windows_language():
    if WINDOWS:
        try:
            return 'es' if (ctypes.windll.kernel32.GetUserDefaultUILanguage() & 0x3ff) == 0x0a else 'en'
        except (AttributeError, OSError):
            pass
    return 'es' if os.environ.get('LANG', '').startswith('es') else 'en'


# ---------------------------------------------------------------------------------------------
# Command line roles of the frozen executable.

def attach_stdio():
    """A windowed executable starts without sys.stdout; inherited handles (log files) still exist."""
    if not WINDOWS:
        return
    import msvcrt
    for name, std in (('stdout', -11), ('stderr', -12)):
        if getattr(sys, name) is not None:
            continue
        stream = None
        handle = ctypes.windll.kernel32.GetStdHandle(std)
        if handle and handle != ctypes.c_void_p(-1).value:
            try:
                stream = open(msvcrt.open_osfhandle(handle, os.O_WRONLY), 'w', encoding='utf-8',
                              errors='replace', buffering=1)
            except OSError:
                stream = None
        setattr(sys, name, stream or open(os.devnull, 'w'))


def run_role(argv):
    """--run [args]: run.py; --script <file> [args]: a preparation script. Returns an exit code."""
    attach_stdio()
    for stream in (sys.stdout, sys.stderr):
        try:
            stream.reconfigure(encoding='utf-8', line_buffering=True)
        except (AttributeError, ValueError):
            pass
    if argv[0] == '--script':
        path, sys.argv = argv[1], argv[1:]
    else:
        path = str(PORT_DIR / 'run.py')
        sys.argv = [path, *argv[1:]]
    try:
        runpy.run_path(path, run_name='__main__')
    except SystemExit as stop:
        return stop.code if isinstance(stop.code, int) else (0 if stop.code is None else 1)
    return 0


def run_command():
    return [sys.executable, '--run'] if FROZEN else [sys.executable, str(PORT_DIR / 'run.py')]


# ---------------------------------------------------------------------------------------------
# Game folder.

def read_sfo(path):
    from prepare import read_sfo as read
    return read(path)


def check_game(game_dir, update_dir):
    """(ok, message, needs_update): whether these folders hold Killzone Shadow Fall 01.81."""
    if not game_dir:
        return False, tr('no_folder'), False
    game = Path(game_dir)
    if not (game / 'eboot.bin').is_file():
        return False, tr('no_eboot'), False
    sfo = read_sfo(game / 'sce_sys' / 'param.sfo')
    if not sfo:
        return False, tr('no_sfo'), False
    title_id = str(sfo.get('TITLE_ID', '?'))
    if title_id != TITLE_ID:
        return False, tr('wrong_game', id=title_id, want=TITLE_ID), False
    title = clean_title(sfo.get('TITLE', 'Killzone Shadow Fall'))
    version = str(sfo.get('APP_VER', '?'))
    if version == GAME_VERSION:
        return True, tr('ready', title=title, id=title_id, ver=version), False
    if not update_dir:
        return False, tr('need_update', ver=version, want=GAME_VERSION), True
    update = read_sfo(Path(update_dir) / 'sce_sys' / 'param.sfo') or {}
    up_id, up_ver = str(update.get('TITLE_ID', '?')), str(update.get('APP_VER', '?'))
    if up_id != TITLE_ID or up_ver != GAME_VERSION or not (Path(update_dir) / 'eboot.bin').is_file():
        return False, tr('bad_update', want=TITLE_ID, ver_want=GAME_VERSION, id=up_id, ver=up_ver), True
    return True, tr('ready_update', title=title, id=title_id, ver=up_ver), True


def clean_title(title):
    title = ''.join(ch for ch in str(title) if ch.isprintable() and ch not in '™®�')
    return ' '.join(word.capitalize() for word in title.split()) or 'Killzone Shadow Fall'


# ---------------------------------------------------------------------------------------------
# Settings.

GRAPHICS_DEFAULTS = {
    'display_mode': 'windowed', 'render_resolution': 'native', 'fps_limit': '60', 'vsync': '1', 'anisotropy': '16',
    'rcas': '0', 'fsr1': '0',
    'show_fps': '0', 'vibration': '100', 'async_shaders': '1', 'stale_readback': '1',
    'deferred_readback': '1', 'frames_queued': '1',
}
LAUNCHER_DEFAULTS = {'game_dir': '', 'update_dir': '', 'language': '', 'window_size': '1920x1080',
                     'key_map': {}, 'pad_map': {}, 'mouse_look': True, 'mouse_sens': 1.0,
                     'mouse_invert': False, 'check_updates': True}
WINDOW_SIZES = ('1280x720', '1600x900', '1920x1080', '2560x1440', '3840x2160')
DISPLAY_MODES = ('windowed', 'borderless', 'fullscreen')
FPS_LIMITS = ('30', '60', '144', '240', '0')
# Above 60 the game is not vblank paced: vblank runs at 480 Hz and the present thread caps the rate
# (GOW3_FPS_LIMIT). The engine measures frame time with the guest TSC, so game speed is unchanged.
EXPERIMENTAL_FPS = ('144', '240', '0')
RENDER_RESOLUTIONS = ('720p', 'native', '1440p', '1800p', '4K')  # patches/Killzone_Shadow_Fall.xml
ANISOTROPY = ('0', '2', '4', '8', '16')
FRAMES_QUEUED = ('1', '2', '0')

# PS4 button, label, default key (SDL scancode name) and default controller button (SDL name).
BUTTONS = (
    ('cross', '✕', 'Space', 'south'), ('circle', '○', 'Left Shift', 'east'),
    ('square', '□', 'E', 'west'), ('triangle', '△', 'Q', 'north'),
    ('l1', 'L1', '1', 'leftshoulder'), ('r1', 'R1', '3', 'rightshoulder'),
    ('l2', 'L2', 'R', ''), ('r2', 'R2', 'F', ''),
    ('l3', 'L3', 'Z', 'leftstick'), ('r3', 'R3', 'C', 'rightstick'),
    ('options', 'OPTIONS', 'Return', 'start'),
    ('touchpad_left', 'Touch ←', 'Tab', ''), ('touchpad_right', 'Touch →', 'Backspace', 'back'),
    ('up', '↑', 'I', 'dpup'), ('down', '↓', 'K', 'dpdown'), ('left', '←', 'J', 'dpleft'), ('right', '→', 'L', 'dpright'),
)
# Controller buttons a PS4 button can take: SDL name and the label shown (Xbox / PlayStation).
PAD_BUTTONS = (
    ('', '—'), ('south', 'A / ✕'), ('east', 'B / ○'), ('west', 'X / □'), ('north', 'Y / △'),
    ('leftshoulder', 'LB / L1'), ('rightshoulder', 'RB / R1'), ('leftstick', 'LS / L3'),
    ('rightstick', 'RS / R3'), ('start', 'Start / Options'), ('back', 'Back / Share'),
    ('dpup', 'D-pad ↑'), ('dpdown', 'D-pad ↓'), ('dpleft', 'D-pad ←'), ('dpright', 'D-pad →'),
    ('misc1', 'Misc / Mic'), ('right_paddle1', 'Paddle R1'), ('left_paddle1', 'Paddle L1'),
    ('right_paddle2', 'Paddle R2'), ('left_paddle2', 'Paddle L2'),
)
# Keys the game itself or the port already uses: not offered for buttons.
TK_MOUSE = {1: 'Mouse Left', 2: 'Mouse Middle', 3: 'Mouse Right', 4: 'Mouse 4', 5: 'Mouse 5'}
RESERVED_KEYS = {'W', 'A', 'S', 'D', 'Up', 'Down', 'Left', 'Right', 'Insert', 'Delete', 'F9', 'F11', 'F12', 'Escape'}
# Tk keysyms that differ from SDL scancode names.
TK_TO_SDL = {
    'space': 'Space', 'Shift_L': 'Left Shift', 'Shift_R': 'Right Shift', 'Control_L': 'Left Ctrl',
    'Control_R': 'Right Ctrl', 'Alt_L': 'Left Alt', 'Alt_R': 'Right Alt', 'Return': 'Return', 'Tab': 'Tab',
    'BackSpace': 'Backspace', 'Caps_Lock': 'CapsLock', 'comma': ',', 'period': '.', 'slash': '/',
    'semicolon': ';', 'apostrophe': "'", 'bracketleft': '[', 'bracketright': ']', 'minus': '-',
    'equal': '=', 'grave': '`', 'backslash': '\\', 'Home': 'Home', 'End': 'End', 'Prior': 'PageUp',
    'Next': 'PageDown', 'Up': 'Up', 'Down': 'Down', 'Left': 'Left', 'Right': 'Right', 'Insert': 'Insert',
    'Delete': 'Delete', 'Escape': 'Escape', 'KP_Enter': 'Keypad Enter', 'KP_Add': 'Keypad +',
    'KP_Subtract': 'Keypad -', 'KP_Multiply': 'Keypad *', 'KP_Divide': 'Keypad /', 'KP_Decimal': 'Keypad .',
}
# Tk keysyms on the number pad with Num Lock off.
for _digit, _name in enumerate(('KP_Insert', 'KP_End', 'KP_Down', 'KP_Next', 'KP_Left', 'KP_Begin',
                                'KP_Right', 'KP_Home', 'KP_Up', 'KP_Prior')):
    TK_TO_SDL[_name] = f'Keypad {_digit}'
    TK_TO_SDL[f'KP_{_digit}'] = f'Keypad {_digit}'


def sdl_key_name(keysym):
    """The SDL scancode name of a Tk keysym, or None when the port cannot read that key."""
    if keysym in TK_TO_SDL:
        return TK_TO_SDL[keysym]
    if len(keysym) == 1 and keysym.isalnum():
        return keysym.upper()
    if keysym.startswith('F') and keysym[1:].isdigit() and 1 <= int(keysym[1:]) <= 12:
        return keysym
    return None


def load_launcher():
    try:
        data = json.loads(LAUNCHER_FILE.read_text(encoding='utf-8'))
    except (OSError, ValueError):
        data = {}
    settings = {**LAUNCHER_DEFAULTS, **(data if isinstance(data, dict) else {})}
    for key in ('key_map', 'pad_map'):
        if not isinstance(settings[key], dict):
            settings[key] = {}
    return settings


def save_launcher(settings):
    USER_DIR.mkdir(parents=True, exist_ok=True)
    LAUNCHER_FILE.write_text(json.dumps(settings, indent=2, ensure_ascii=False) + '\n', encoding='utf-8')


def load_graphics():
    from graphics_options import read_ini
    ini = read_ini(SETTINGS_FILE)
    values = {key: ini.get(key, value) for key, value in GRAPHICS_DEFAULTS.items()}
    # Values the launcher does not offer (the in-game menu has more) fall back to the closest one.
    if values['display_mode'] not in DISPLAY_MODES:
        values['display_mode'] = 'windowed'
    if values['render_resolution'] not in RENDER_RESOLUTIONS:
        values['render_resolution'] = 'native'
    if values['fps_limit'] not in FPS_LIMITS:
        values['fps_limit'] = '60'
    if values['anisotropy'] not in ANISOTROPY:
        values['anisotropy'] = '16'
    if values['frames_queued'] not in FRAMES_QUEUED:
        values['frames_queued'] = '1'
    return values


def save_graphics(values):
    from graphics_options import write_keys
    USER_DIR.mkdir(parents=True, exist_ok=True)
    if not SETTINGS_FILE.exists():
        SETTINGS_FILE.write_text('# Killzone Shadow Fall (PC port) settings: launcher and in-game menu (Insert)\n',
                                 encoding='utf-8')
    write_keys(SETTINGS_FILE, values)


def map_spec(mapping, column):
    """GOW3_KEY_MAP / GOW3_PAD_MAP for the buttons that differ from the defaults."""
    defaults = {name: (key, pad) for name, _, key, pad in BUTTONS}
    parts = []
    for name, value in mapping.items():
        if name in defaults and value != defaults[name][column]:
            parts.append(f'{name}={value}')
    return ','.join(parts)


# ---------------------------------------------------------------------------------------------
# Folders.

def cache_paths():
    return [USER_DIR / 'out', PS4_DIR / 'cache']


def folder_size(path):
    total = 0
    for root, _, files in os.walk(path):
        for name in files:
            try:
                total += os.path.getsize(os.path.join(root, name))
            except OSError:
                pass
    return total


def human_size(size):
    for unit in ('B', 'KB', 'MB', 'GB'):
        if size < 1024 or unit == 'GB':
            return f'{size:.0f} {unit}' if unit in ('B', 'KB') else f'{size:.1f} {unit}'
        size /= 1024
    return f'{size:.1f} GB'


def open_folder(path):
    path = Path(path)
    path.mkdir(parents=True, exist_ok=True)
    if WINDOWS:
        os.startfile(str(path))  # noqa: S606
    else:
        subprocess.Popen(['xdg-open', str(path)])


def prune_logs():
    try:
        runs = sorted((p for p in LOGS_DIR.iterdir() if p.is_dir()), key=lambda p: p.name)
    except OSError:
        return
    for old in runs[:-KEEP_LOGS]:
        shutil.rmtree(old, ignore_errors=True)


# ---------------------------------------------------------------------------------------------
# Window.

class Launcher:
    def __init__(self, root, tk, ttk, filedialog, messagebox):
        self.root, self.tk, self.ttk = root, tk, ttk
        self.filedialog, self.messagebox = filedialog, messagebox
        self.settings = load_launcher()
        self.graphics = load_graphics()
        self.process = None
        self.capturing = None
        self.images = {}
        self.scale = max(1.0, root.winfo_fpixels('1i') / 96.0)
        root.title('Killzone Shadow Fall')
        root.configure(bg=BG)
        root.minsize(self.px(900), self.px(740))
        root.geometry(f'{self.px(980)}x{self.px(780)}')
        self.style()
        self.build()
        root.protocol('WM_DELETE_WINDOW', self.close)
        self.update_info = None
        if self.settings.get('check_updates', True):
            self.check_updates(quiet=True)
        if kzsf_update.failed_update(UPDATE_DIR):
            root.after(500, lambda: self.messagebox.showwarning('Killzone Shadow Fall', tr('update_previous_failed'),
                                                                 parent=root))

    def px(self, value):
        return int(value * self.scale)

    def font(self, size, weight='normal', family=UI_FONT):
        return (family, size, weight)

    def style(self):
        s = self.ttk.Style(self.root)
        s.theme_use('clam')
        s.configure('.', background=BG, foreground=TEXT, fieldbackground=FIELD, bordercolor=LINE,
                    lightcolor=LINE, darkcolor=LINE, troughcolor=FIELD, focuscolor=ACCENT, font=self.font(10))
        s.configure('TCombobox', arrowcolor=TEXT, padding=self.px(4), selectbackground=FIELD,
                    selectforeground=TEXT)
        s.map('TCombobox', fieldbackground=[('readonly', FIELD)], foreground=[('readonly', TEXT)],
              bordercolor=[('focus', ACCENT)], arrowcolor=[('active', ACCENT)])
        s.configure('TCheckbutton', background=PANEL, foreground=TEXT, indicatorbackground=FIELD,
                    indicatorforeground=ACCENT, padding=self.px(2))
        s.map('TCheckbutton', background=[('active', PANEL)], indicatorbackground=[('selected', FIELD)],
              indicatorforeground=[('selected', ACCENT)])
        s.configure('Horizontal.TScale', background=PANEL, troughcolor=FIELD, sliderthickness=self.px(14))
        self.root.option_add('*TCombobox*Listbox.background', FIELD)
        self.root.option_add('*TCombobox*Listbox.foreground', TEXT)
        self.root.option_add('*TCombobox*Listbox.selectBackground', ACCENT_DARK)
        self.root.option_add('*TCombobox*Listbox.selectForeground', TEXT)
        self.root.option_add('*TCombobox*Listbox.font', self.font(10))

    # -- layout -----------------------------------------------------------------------------

    def build(self):
        tk = self.tk
        for child in self.root.winfo_children():
            child.destroy()
        self.banner = tk.Canvas(self.root, height=self.px(170), bg=BG, highlightthickness=0, bd=0)
        self.banner.pack(fill='x')
        self.banner.bind('<Configure>', lambda e: self.draw_banner())

        self.tabbar = tk.Frame(self.root, bg=BG)
        self.tabbar.pack(fill='x', padx=self.px(28))
        footer = tk.Frame(self.root, bg=BG)
        footer.pack(side='bottom', fill='x', padx=self.px(28), pady=(0, self.px(18)))
        self.body = tk.Frame(self.root, bg=PANEL, highlightthickness=1, highlightbackground=LINE)
        self.body.pack(fill='both', expand=True, padx=self.px(28), pady=(0, self.px(12)))
        tk.Label(footer, text='Lesca', fg=MARK, bg=BG, font=self.font(9, 'bold')).pack(side='left')
        tk.Label(footer, text=f'  v{VERSION}  ·  {tr("footer_hint")}', fg=MUTED, bg=BG,
                 font=self.font(9)).pack(side='left')
        self.play_button = tk.Label(footer, text=tr('play'), fg=BG, bg=ACCENT, font=self.font(13, 'bold'),
                                    padx=self.px(46), pady=self.px(10), cursor='hand2')
        self.play_button.pack(side='right')
        self.play_button.bind('<Button-1>', lambda e: self.play())
        self.play_button.bind('<Enter>', lambda e: self.play_hover(True))
        self.play_button.bind('<Leave>', lambda e: self.play_hover(False))
        self.status = tk.Label(footer, text='', fg=MUTED, bg=BG, font=self.font(9))
        self.status.pack(side='right', padx=self.px(14))

        self.tabs = [('game', tr('tab_game'), self.game_tab), ('graphics', tr('tab_graphics'), self.graphics_tab),
                     ('controls', tr('tab_controls'), self.controls_tab),
                     ('advanced', tr('tab_advanced'), self.advanced_tab)]
        self.tab_labels = {}
        for key, title, _ in self.tabs:
            label = tk.Label(self.tabbar, text=title.upper(), bg=BG, fg=MUTED, font=self.font(10, 'bold'),
                             padx=self.px(14), pady=self.px(9), cursor='hand2')
            label.pack(side='left')
            label.bind('<Button-1>', lambda e, k=key: self.show_tab(k))
            self.tab_labels[key] = label
        lang = tk.Frame(self.tabbar, bg=BG)
        lang.pack(side='right')
        for code in ('en', 'es'):
            label = tk.Label(lang, text=code.upper(), bg=BG, fg=ACCENT if LANG == code else MUTED,
                             font=self.font(9, 'bold'), padx=self.px(6), cursor='hand2')
            label.pack(side='right')
            label.bind('<Button-1>', lambda e, c=code: self.set_language(c))
        tk.Label(lang, text=tr('language') + ':', bg=BG, fg=MUTED, font=self.font(9)).pack(side='right')
        self.update_bar = None
        if getattr(self, 'update_info', None):
            self.show_update_bar()
        self.show_tab(getattr(self, 'current_tab', os.environ.get('KZSF_LAUNCHER_TAB', 'game')))
        self.refresh_game()
        self.update_play_state()

    def play_hover(self, inside):
        if self.process is None and self.game_ok:
            self.play_button.configure(bg=TEXT if inside else ACCENT)

    def show_tab(self, key):
        self.current_tab = key
        for name, label in self.tab_labels.items():
            label.configure(fg=TEXT if name == key else MUTED,
                            bg=PANEL if name == key else BG)
        for child in self.body.winfo_children():
            child.destroy()
        frame = self.tk.Frame(self.body, bg=PANEL)
        frame.pack(fill='both', expand=True, padx=self.px(24), pady=self.px(18))
        dict((k, f) for k, _, f in self.tabs)[key](frame)

    def set_language(self, code):
        global LANG
        if code == LANG:
            return
        LANG = code
        self.settings['language'] = code
        save_launcher(self.settings)
        self.build()

    # -- banner -----------------------------------------------------------------------------

    def load_art(self):
        """The game's own background (sce_sys/pic0.png) for the banner, darkened on the left."""
        game = self.settings.get('game_dir')
        if not game:
            return None
        path = Path(game) / 'sce_sys' / 'pic0.png'
        key = (str(path), self.banner.winfo_width(), self.banner.winfo_height())
        if self.images.get('banner_key') == key:
            return self.images.get('banner')
        try:
            from PIL import Image, ImageTk
            width, height = key[1], key[2]
            with Image.open(path) as source:
                image = source.convert('RGB')
            scale = width / image.width
            image = image.resize((width, max(height, int(image.height * scale))), Image.LANCZOS)
            top = int((image.height - height) * 0.42)
            image = image.crop((0, top, width, top + height))
            shade = Image.new('L', (width, 1))
            shade.putdata([int(235 - 150 * min(1.0, x / (width * 0.75))) for x in range(width)])
            shade = shade.resize((width, height))
            bottom = Image.new('L', (1, height))
            bottom.putdata([int(120 * max(0.0, (y - height * 0.55) / (height * 0.45))) for y in range(height)])
            bottom = bottom.resize((width, height))
            from PIL import ImageChops
            mask = ImageChops.lighter(shade, bottom)
            image = Image.composite(Image.new('RGB', (width, height), BG), image, mask)
            photo = ImageTk.PhotoImage(image)
        except Exception:  # noqa: BLE001  (no Pillow, no picture: the plain banner remains)
            photo = None
        self.images['banner_key'], self.images['banner'] = key, photo
        return photo

    def draw_banner(self):
        c = self.banner
        c.delete('all')
        width, height = c.winfo_width(), c.winfo_height()
        art = self.load_art()
        if art:
            c.create_image(0, 0, image=art, anchor='nw')
        else:
            for i in range(0, height, 2):
                shade = int(10 + 14 * i / height)
                c.create_rectangle(0, i, width, i + 2, fill=f'#{shade // 2:02x}{shade:02x}{shade + 4:02x}', width=0)
        x = self.px(30)
        c.create_text(x, self.px(52), text='KILLZONE', anchor='w', fill=TEXT, font=self.font(36, 'bold', 'Segoe UI Black'))
        c.create_text(x + self.px(3), self.px(96), text='S H A D O W   F A L L', anchor='w', fill=TEXT,
                      font=self.font(19, 'normal', 'Segoe UI Light'))
        c.create_rectangle(x, self.px(120), x + self.px(64), self.px(123), fill=ACCENT, width=0)
        c.create_text(x, self.px(140), text='PC', anchor='w', fill=ACCENT, font=self.font(10, 'bold'))
        c.create_line(0, height - 1, width, height - 1, fill=LINE)

    # -- helpers ----------------------------------------------------------------------------

    def heading(self, parent, text, pady=(0, 6)):
        self.tk.Label(parent, text=text.upper(), bg=PANEL, fg=ACCENT, font=self.font(9, 'bold')).pack(
            anchor='w', pady=(self.px(pady[0]), self.px(pady[1])))

    def note(self, parent, text, color=MUTED, wrap=820):
        label = self.tk.Label(parent, text=text, bg=PANEL, fg=color, font=self.font(9), justify='left',
                              wraplength=self.px(wrap))
        label.pack(anchor='w', pady=(self.px(2), 0))
        return label

    def button(self, parent, text, command, primary=False):
        b = self.tk.Label(parent, text=text, bg=ACCENT if primary else FIELD, fg=BG if primary else TEXT,
                          font=self.font(9, 'bold' if primary else 'normal'), padx=self.px(12), pady=self.px(6),
                          cursor='hand2', highlightthickness=1, highlightbackground=LINE)
        b.bind('<Button-1>', lambda e: command())
        b.bind('<Enter>', lambda e: b.configure(highlightbackground=ACCENT))
        b.bind('<Leave>', lambda e: b.configure(highlightbackground=LINE))
        return b

    def row(self, parent, label):
        frame = self.tk.Frame(parent, bg=PANEL)
        frame.pack(fill='x', pady=self.px(5))
        self.tk.Label(frame, text=label, bg=PANEL, fg=TEXT, font=self.font(10), width=26, anchor='w').pack(side='left')
        return frame

    def choice(self, parent, label, values, labels, current, on_change, help_text=None):
        frame = self.row(parent, label)
        combo = self.ttk.Combobox(frame, values=labels, state='readonly', width=26)
        combo.current(values.index(current) if current in values else 0)
        combo.pack(side='left')
        combo.bind('<<ComboboxSelected>>', lambda e: on_change(values[combo.current()]))
        if help_text:
            self.tk.Label(frame, text=help_text, bg=PANEL, fg=MUTED, font=self.font(9), justify='left',
                          wraplength=self.px(360)).pack(side='left', padx=self.px(12))
        return combo

    def check(self, parent, label, key, help_text=None):
        var = self.tk.BooleanVar(value=self.graphics[key] == '1')

        def changed():
            self.graphics[key] = '1' if var.get() else '0'
            save_graphics({key: self.graphics[key]})
        frame = self.tk.Frame(parent, bg=PANEL)
        frame.pack(fill='x', pady=self.px(4))
        self.ttk.Checkbutton(frame, text=label, variable=var, command=changed).pack(side='left')
        if help_text:
            self.tk.Label(frame, text=help_text, bg=PANEL, fg=MUTED, font=self.font(9)).pack(side='left',
                                                                                         padx=self.px(10))
        self.images.setdefault('vars', []).append(var)
        return var

    def set_graphic(self, key, value):
        self.graphics[key] = value
        save_graphics({key: value})

    # -- game tab ---------------------------------------------------------------------------

    def game_tab(self, parent):
        tk = self.tk
        self.heading(parent, tr('game_folder'))
        row = tk.Frame(parent, bg=PANEL)
        row.pack(fill='x')
        self.game_entry = tk.Label(row, text=self.settings['game_dir'] or '—', bg=FIELD, fg=TEXT, anchor='w',
                                   font=self.font(10), padx=self.px(10), pady=self.px(7))
        self.game_entry.pack(side='left', fill='x', expand=True)
        self.button(row, tr('browse'), self.choose_game).pack(side='left', padx=(self.px(8), 0))
        self.note(parent, tr('game_folder_help'))

        self.update_frame = tk.Frame(parent, bg=PANEL)
        self.update_frame.pack(fill='x')
        self.game_status = tk.Label(parent, text='', bg=PANEL, font=self.font(10, 'bold'), anchor='w',
                                    justify='left', wraplength=self.px(820))
        self.game_status.pack(fill='x', pady=(self.px(10), 0))

        self.heading(parent, tr('tools'), pady=(22, 8))
        tools = tk.Frame(parent, bg=PANEL)
        tools.pack(fill='x')
        for text, command in ((tr('open_saves'), lambda: open_folder(PS4_DIR / 'savedata')),
                              (tr('open_shots'), lambda: open_folder(PS4_DIR / 'screenshots')),
                              (tr('open_logs'), lambda: open_folder(LOGS_DIR)),
                              (tr('clear_cache'), self.clear_cache)):
            self.button(tools, text, command).pack(side='left', padx=(0, self.px(8)))
        self.cache_note = self.note(parent, '')
        self.refresh_cache_size()
        self.refresh_game()

    def refresh_cache_size(self):
        def measure():
            size = sum(folder_size(p) for p in cache_paths())
            self.root.after(0, lambda: self.cache_note.winfo_exists() and
                            self.cache_note.configure(text=tr('cache_size', size=human_size(size))))
        threading.Thread(target=measure, daemon=True).start()

    def refresh_game(self):
        ok, message, needs_update = check_game(self.settings['game_dir'], self.settings['update_dir'])
        self.game_ok = ok
        if getattr(self, 'game_status', None) is not None and self.game_status.winfo_exists():
            self.game_status.configure(text=('✓  ' if ok else '✕  ') + message, fg=OK if ok else
                                       (WARN if needs_update or not self.settings['game_dir'] else ERROR))
            for child in self.update_frame.winfo_children():
                child.destroy()
            if needs_update:
                self.heading(self.update_frame, tr('update_folder'), pady=(16, 6))
                row = self.tk.Frame(self.update_frame, bg=PANEL)
                row.pack(fill='x')
                self.tk.Label(row, text=self.settings['update_dir'] or '—', bg=FIELD, fg=TEXT, anchor='w',
                              font=self.font(10), padx=self.px(10), pady=self.px(7)).pack(side='left', fill='x',
                                                                                         expand=True)
                self.button(row, tr('browse'), self.choose_update).pack(side='left', padx=(self.px(8), 0))
                self.note(self.update_frame, tr('update_folder_help'))
        self.update_play_state()

    def choose_game(self):
        folder = self.filedialog.askdirectory(title=tr('choose_game'), parent=self.root,
                                              initialdir=self.settings['game_dir'] or None)
        if not folder:
            return
        folder = Path(folder)
        # The eboot.bin may be one level down (a folder holding CUSA00190/).
        if not (folder / 'eboot.bin').is_file() and (folder / TITLE_ID / 'eboot.bin').is_file():
            folder = folder / TITLE_ID
        self.settings['game_dir'] = str(folder)
        save_launcher(self.settings)
        self.images.pop('banner_key', None)
        self.set_window_icon()
        self.draw_banner()
        self.game_entry.configure(text=str(folder))
        self.refresh_game()

    def choose_update(self):
        folder = self.filedialog.askdirectory(title=tr('choose_update'), parent=self.root,
                                              initialdir=self.settings['update_dir'] or None)
        if folder:
            self.settings['update_dir'] = str(Path(folder))
            save_launcher(self.settings)
            self.refresh_game()

    def clear_cache(self):
        if self.process is not None:
            self.messagebox.showinfo('Killzone Shadow Fall', tr('busy_running'), parent=self.root)
            return
        size = human_size(sum(folder_size(p) for p in cache_paths()))
        if not self.messagebox.askyesno('Killzone Shadow Fall', tr('clear_confirm', size=size), parent=self.root):
            return
        for path in cache_paths():
            shutil.rmtree(path, ignore_errors=True)
        self.refresh_cache_size()
        self.status.configure(text=tr('cleared'), fg=OK)

    # -- graphics tab -----------------------------------------------------------------------

    def graphics_tab(self, parent):
        self.choice(parent, tr('display_mode'), DISPLAY_MODES,
                    [tr('mode_windowed'), tr('mode_borderless'), tr('mode_fullscreen')],
                    self.graphics['display_mode'], lambda v: self.set_graphic('display_mode', v))
        size = self.settings.get('window_size') if self.settings.get('window_size') in WINDOW_SIZES else '1920x1080'
        self.choice(parent, tr('window_size'), WINDOW_SIZES, [s.replace('x', ' × ') for s in WINDOW_SIZES], size,
                    self.set_window_size, tr('window_size_help'))
        self.choice(parent, tr('render_res'), RENDER_RESOLUTIONS,
                    ['720p (1280 × 720)', tr('res_native'), '1440p (2560 × 1440)', '1800p (3200 × 1800)',
                     '4K (3840 × 2160)'], self.graphics['render_resolution'],
                    lambda v: self.set_graphic('render_resolution', v), tr('render_res_help'))
        self.choice(parent, tr('fps_limit'), FPS_LIMITS,
                    ['30 FPS', '60 FPS', f"144 FPS ({tr('fps_experimental')})",
                     f"240 FPS ({tr('fps_experimental')})", tr('fps_unlimited')],
                    self.graphics['fps_limit'], lambda v: self.set_graphic('fps_limit', v), tr('fps_limit_help'))
        self.choice(parent, tr('anisotropy'), ANISOTROPY, [tr('aniso_game'), '2x', '4x', '8x', '16x'],
                    self.graphics['anisotropy'], lambda v: self.set_graphic('anisotropy', v))
        self.tk.Frame(parent, bg=PANEL, height=self.px(8)).pack()
        self.check(parent, tr('vsync'), 'vsync', tr('vsync_help'))
        self.check(parent, tr('fsr1'), 'fsr1', tr('fsr1_help'))
        self.check(parent, tr('sharpen'), 'rcas')
        self.check(parent, tr('show_fps'), 'show_fps')
        self.tk.Frame(parent, bg=LINE, height=1).pack(fill='x', pady=self.px(14))
        self.note(parent, tr('graphics_note'))

    def set_window_size(self, value):
        self.settings['window_size'] = value
        save_launcher(self.settings)

    # -- controls tab -----------------------------------------------------------------------

    def controls_tab(self, parent):
        tk = self.tk
        self.note(parent, tr('controls_help'), wrap=880)
        grid = tk.Frame(parent, bg=PANEL)
        grid.pack(fill='x', pady=(self.px(8), 0))
        self.key_labels = {}
        keys = self.settings['key_map']
        pads = self.settings['pad_map']
        values = [v for v, _ in PAD_BUTTONS]
        rows = (len(BUTTONS) + 1) // 2
        # Two groups side by side, each: PS4 button | keyboard key | controller button.
        for group in range(2):
            col = group * 4
            pad_x = (self.px(36), 0) if group else (0, 0)
            for c, text in enumerate(('PS4', tr('keyboard').upper(), tr('controller').upper())):
                tk.Label(grid, text=text, bg=PANEL, fg=ACCENT, font=self.font(8, 'bold')).grid(
                    row=0, column=col + c, sticky='w', padx=pad_x if c == 0 else (self.px(6), 0))
        for index, (name, label, key, pad) in enumerate(BUTTONS):
            r, col = index % rows + 1, (index // rows) * 4
            pad_x = (self.px(36), 0) if col else (0, 0)
            tk.Label(grid, text=label, bg=PANEL, fg=TEXT, font=self.font(9), width=10, anchor='w').grid(
                row=r, column=col, sticky='w', padx=pad_x)
            k = tk.Label(grid, text=keys.get(name, key), bg=FIELD, fg=TEXT, font=self.font(9), width=12,
                         cursor='hand2', highlightthickness=1, highlightbackground=LINE)
            k.grid(row=r, column=col + 1, sticky='w', padx=(self.px(6), 0), pady=self.px(1))
            k.bind('<Button-1>', lambda e, n=name: self.capture_key(n))
            self.key_labels[name] = k
            if name in ('l2', 'r2'):  # the triggers stay the triggers
                tk.Label(grid, text='LT / L2' if name == 'l2' else 'RT / R2', bg=PANEL, fg=MUTED,
                         font=self.font(9), anchor='w').grid(row=r, column=col + 2, sticky='w', padx=(self.px(10), 0))
                continue
            current = pads.get(name, pad)
            combo = self.ttk.Combobox(grid, values=[t for _, t in PAD_BUTTONS], state='readonly', width=14,
                                      font=self.font(9))
            combo.current(values.index(current) if current in values else 0)
            combo.grid(row=r, column=col + 2, sticky='w', padx=(self.px(6), 0), pady=self.px(1))
            combo.bind('<<ComboboxSelected>>', lambda e, n=name, c=combo: self.set_pad(n, values[c.current()]))
        self.mouse_section(parent)
        bottom = tk.Frame(parent, bg=PANEL)
        bottom.pack(fill='x', pady=(self.px(8), 0))
        tk.Label(bottom, text=tr('vibration'), bg=PANEL, fg=TEXT, font=self.font(10)).pack(side='left')
        var = tk.IntVar(value=int(self.graphics['vibration']) if self.graphics['vibration'].isdigit() else 100)
        value_label = tk.Label(bottom, text=f'{var.get()}%', bg=PANEL, fg=MUTED, font=self.font(9), width=5)

        def moved(value):
            value = str(int(float(value)))
            value_label.configure(text=f'{value}%')
            if value != self.graphics['vibration']:
                self.set_graphic('vibration', value)
        self.ttk.Scale(bottom, from_=0, to=100, variable=var, command=moved,
                       length=self.px(200)).pack(side='left', padx=self.px(10))
        value_label.pack(side='left')
        self.images['vibration_var'] = var
        self.button(bottom, tr('reset_defaults'), self.reset_controls).pack(side='right')
        self.note(parent, tr('controller_help'))

    def mouse_section(self, parent):
        tk = self.tk
        row = tk.Frame(parent, bg=PANEL)
        row.pack(fill='x', pady=(self.px(10), 0))
        look = tk.BooleanVar(value=bool(self.settings.get('mouse_look', True)))
        invert = tk.BooleanVar(value=bool(self.settings.get('mouse_invert', False)))

        def changed():
            self.settings['mouse_look'] = look.get()
            self.settings['mouse_invert'] = invert.get()
            save_launcher(self.settings)
        self.ttk.Checkbutton(row, text=tr('mouse_look'), variable=look, command=changed).pack(side='left')
        tk.Label(row, text=tr('mouse_sens'), bg=PANEL, fg=TEXT, font=self.font(10)).pack(side='left',
                                                                                       padx=(self.px(24), 0))
        sens = tk.DoubleVar(value=float(self.settings.get('mouse_sens', 1.0)))
        sens_label = tk.Label(row, text=f'{sens.get():.1f}', bg=PANEL, fg=MUTED, font=self.font(9), width=4)

        def moved(value):
            value = round(float(value) * 10) / 10
            sens_label.configure(text=f'{value:.1f}')
            if value != self.settings.get('mouse_sens'):
                self.settings['mouse_sens'] = value
                save_launcher(self.settings)
        self.ttk.Scale(row, from_=0.2, to=4.0, variable=sens, command=moved, length=self.px(160)).pack(
            side='left', padx=self.px(8))
        sens_label.pack(side='left')
        self.ttk.Checkbutton(row, text=tr('mouse_invert'), variable=invert, command=changed).pack(
            side='left', padx=(self.px(18), 0))
        self.images['mouse_vars'] = (look, invert, sens)
        self.note(parent, tr('mouse_look_help'))

    def capture_key(self, name):
        if self.capturing:
            self.cancel_capture()
        self.capturing = name
        label = self.key_labels[name]
        label.configure(text=tr('press_key'), fg=ACCENT, highlightbackground=ACCENT)
        self.root.bind('<KeyPress>', self.key_pressed)
        # Bound once this click is over, or it would be taken as the new binding.
        self.root.after(150, self.bind_mouse_capture)
        self.root.focus_force()

    def bind_mouse_capture(self):
        if self.capturing:
            self.root.bind('<ButtonPress>', lambda e: self.assign_key(TK_MOUSE.get(e.num)))
            self.root.bind('<MouseWheel>', lambda e: self.assign_key('Wheel Up' if e.delta > 0 else 'Wheel Down'))

    def cancel_capture(self):
        name = self.capturing
        self.capturing = None
        self.root.unbind('<KeyPress>')
        self.root.unbind('<ButtonPress>')
        self.root.unbind('<MouseWheel>')
        if name and name in self.key_labels and self.key_labels[name].winfo_exists():
            default = next(key for n, _, key, _ in BUTTONS if n == name)
            self.key_labels[name].configure(text=self.settings['key_map'].get(name, default), fg=TEXT,
                                            highlightbackground=LINE)

    def key_pressed(self, event):
        name = self.capturing
        if not name:
            return
        if event.keysym == 'Escape':
            self.cancel_capture()
            return
        self.assign_key(sdl_key_name(event.keysym))

    def assign_key(self, key):
        name = self.capturing
        if not name:
            return
        if not key or key in RESERVED_KEYS:
            self.root.bell()
            return
        # A key serves one button: whichever had it gets this button's old key.
        keys = self.settings['key_map']
        defaults = {n: k for n, _, k, _ in BUTTONS}
        old = keys.get(name, defaults[name])
        for other in defaults:
            if other != name and keys.get(other, defaults[other]) == key:
                keys[other] = old
                self.key_labels[other].configure(text=old)
        keys[name] = key
        save_launcher(self.settings)
        self.cancel_capture()

    def set_pad(self, name, value):
        self.settings['pad_map'][name] = value
        save_launcher(self.settings)

    def reset_controls(self):
        self.settings.update(key_map={}, pad_map={}, mouse_look=True, mouse_sens=1.0, mouse_invert=False)
        save_launcher(self.settings)
        self.set_graphic('vibration', '100')
        self.show_tab('controls')

    # -- advanced tab -----------------------------------------------------------------------

    def advanced_tab(self, parent):
        self.note(parent, tr('advanced_help'), color=WARN)
        self.tk.Frame(parent, bg=PANEL, height=self.px(8)).pack()
        self.check(parent, tr('async_shaders'), 'async_shaders')
        self.note(parent, '      ' + tr('async_help'))
        self.check(parent, tr('stale_readback'), 'stale_readback')
        self.note(parent, '      ' + tr('stale_help'))
        self.check(parent, tr('deferred_readback'), 'deferred_readback')
        self.note(parent, '      ' + tr('deferred_help'))
        self.tk.Frame(parent, bg=PANEL, height=self.px(8)).pack()
        self.choice(parent, tr('frames_queued'), FRAMES_QUEUED, [tr('queued_1'), tr('queued_2'), tr('queued_0')],
                    self.graphics['frames_queued'], lambda v: self.set_graphic('frames_queued', v))
        self.tk.Frame(parent, bg=LINE, height=1).pack(fill='x', pady=self.px(14))
        self.heading(parent, tr('updates'))
        var = self.tk.BooleanVar(value=bool(self.settings.get('check_updates', True)))

        def changed():
            self.settings['check_updates'] = var.get()
            save_launcher(self.settings)
        row = self.tk.Frame(parent, bg=PANEL)
        row.pack(fill='x', pady=self.px(4))
        self.ttk.Checkbutton(row, text=tr('check_updates'), variable=var, command=changed).pack(side='left')
        self.button(row, tr('check_updates_now'), lambda: self.check_updates(quiet=False)).pack(
            side='left', padx=self.px(16))
        self.images.setdefault('vars', []).append(var)
        self.tk.Frame(parent, bg=LINE, height=1).pack(fill='x', pady=self.px(14))
        self.button(parent, tr('reset_all'), self.reset_all).pack(anchor='w')

    def reset_all(self):
        if not self.messagebox.askyesno('Killzone Shadow Fall', tr('reset_all_confirm'), parent=self.root):
            return
        self.graphics = dict(GRAPHICS_DEFAULTS)
        save_graphics(self.graphics)
        self.settings.update(window_size='1920x1080', key_map={}, pad_map={}, mouse_look=True, mouse_sens=1.0,
                             mouse_invert=False)
        save_launcher(self.settings)
        self.show_tab('advanced')

    # -- updates ----------------------------------------------------------------------------

    def check_updates(self, quiet):
        """Asks GitHub for a newer release in the background; quiet: no message when none or offline."""
        def work():
            try:
                # KZSF_UPDATE_AS_VERSION: test the update flow from an older version number.
                release, error = kzsf_update.check(os.environ.get('KZSF_UPDATE_AS_VERSION') or VERSION), None
            except (OSError, ValueError) as e:
                release, error = None, e
            self.root.after(0, lambda: self.update_checked(release, error, quiet))
        threading.Thread(target=work, daemon=True).start()

    def update_checked(self, release, error, quiet):
        if release:
            self.update_info = release
            self.show_update_bar()
        elif not quiet:
            text = tr('update_check_failed', error=error) if error else tr('update_none', version=VERSION)
            self.messagebox.showinfo('Killzone Shadow Fall', text, parent=self.root)

    def show_update_bar(self):
        tk, release = self.tk, self.update_info
        if self.update_bar is not None and self.update_bar.winfo_exists():
            self.update_bar.destroy()
        bar = tk.Frame(self.root, bg=FIELD, highlightthickness=1, highlightbackground=ACCENT)
        bar.pack(side='bottom', fill='x', padx=self.px(28), pady=(0, self.px(10)), before=self.body)
        tk.Label(bar, text=tr('update_available', version=release['version']), bg=FIELD, fg=ACCENT,
                 font=self.font(10, 'bold')).pack(side='left', padx=self.px(12), pady=self.px(8))
        later = self.button(bar, tr('update_later'), lambda: bar.destroy())
        later.pack(side='right', padx=(self.px(6), self.px(10)), pady=self.px(6))
        self.button(bar, tr('update_notes'), lambda: webbrowser.open(release['page'])).pack(side='right', padx=self.px(6))
        self.button(bar, tr('update_now'), self.install_update, primary=True).pack(side='right', padx=self.px(6))
        self.update_bar = bar

    def install_update(self):
        release = self.update_info
        if self.process is not None:
            self.messagebox.showwarning('Killzone Shadow Fall', tr('update_close_game'), parent=self.root)
            return
        if not FROZEN:  # running from source: git pulls the code, the page has the zip
            webbrowser.open(release['page'])
            return
        if not self.messagebox.askyesno('Killzone Shadow Fall', tr('update_confirm', version=release['version'],
                                                                     size=human_size(release['size'])),
                                        parent=self.root):
            return
        self.play_button.configure(cursor='arrow')
        self.updating = True  # PLAY waits while the update downloads

        def progress(done, total):
            percent = int(done * 100 / total) if total else 0
            self.root.after(0, lambda: self.status.configure(text=tr('update_downloading', percent=percent),
                                                             fg=ACCENT))

        def work():
            try:
                archive = kzsf_update.download(release, UPDATE_DIR, progress)
                self.root.after(0, lambda: self.status.configure(text=tr('update_unpacking'), fg=ACCENT))
                unpacked = UPDATE_DIR / 'new'
                source = kzsf_update.extract(archive, unpacked, Path(sys.executable).name)
                script = kzsf_update.install_script(source, PORT_DIR, os.getpid(), Path(sys.executable).name,
                                                    UPDATE_DIR, cleanup=(unpacked, archive))
                kzsf_update.run_script(script)
            except (OSError, ValueError, zipfile.BadZipFile) as e:
                self.root.after(0, lambda e=e: self.update_failed(e))
                return
            self.root.after(0, self.root.destroy)
        threading.Thread(target=work, daemon=True).start()

    def update_failed(self, error):
        self.updating = False
        self.status.configure(text='', fg=MUTED)
        self.update_play_state()
        self.messagebox.showerror('Killzone Shadow Fall', tr('update_failed', error=error), parent=self.root)

    # -- playing ----------------------------------------------------------------------------

    def update_play_state(self):
        if not hasattr(self, 'play_button'):
            return
        if self.process is not None:
            self.play_button.configure(text=tr('playing'), bg=FIELD, fg=MUTED, cursor='arrow')
        elif not getattr(self, 'game_ok', False):
            self.play_button.configure(text=tr('play'), bg=FIELD, fg=MUTED, cursor='arrow')
        else:
            self.play_button.configure(text=tr('play'), bg=ACCENT, fg=BG, cursor='hand2')

    def play(self):
        if self.process is not None or getattr(self, 'updating', False):
            return
        ok, message, needs_update = check_game(self.settings['game_dir'], self.settings['update_dir'])
        if not ok:
            self.messagebox.showwarning('Killzone Shadow Fall', f'{message}\n\n{tr("fix_folder")}', parent=self.root)
            self.show_tab('game')
            return
        save_graphics(self.graphics)
        run_dir = LOGS_DIR / time.strftime('%Y-%m-%d_%H-%M-%S')
        run_dir.mkdir(parents=True, exist_ok=True)
        prune_logs()
        env = dict(os.environ)
        env.update({
            'GOW3_GAME_DIR': self.settings['game_dir'],
            'GOW3_DATA_DIR': str(USER_DIR),
            'GOW3_USER_DIR': str(PS4_DIR),
            'GOW3_CONFIG': str(SETTINGS_FILE),
            'GOW3_WINDOW_SIZE': self.settings.get('window_size') or '1920x1080',
            'GOW3_KEY_MAP': map_spec(self.settings['key_map'], 0),
            'GOW3_PAD_MAP': map_spec(self.settings['pad_map'], 1),
            'PYTHONIOENCODING': 'utf-8',
            'GOW3_MOUSE_LOOK': '1' if self.settings.get('mouse_look', True) else '0',
            'GOW3_MOUSE_SENS': str(self.settings.get('mouse_sens', 1.0)),
            'GOW3_MOUSE_INVERT_Y': '1' if self.settings.get('mouse_invert') else '0',
            'GOW3_DISCORD': '0',  # its Discord presence is God of War III's
        })
        if self.graphics['fps_limit'] in EXPERIMENTAL_FPS:
            env['GOW3_VBLANK_HZ'] = '0'
            env['GOW3_FPS_LIMIT'] = self.graphics['fps_limit']
        else:
            env.pop('GOW3_VBLANK_HZ', None)
            env.pop('GOW3_FPS_LIMIT', None)
        if needs_update and self.settings['update_dir']:
            env['GOW3_PATCH_DIR'] = self.settings['update_dir']
        else:
            env.pop('GOW3_PATCH_DIR', None)
        try:
            out = open(run_dir / 'game.log', 'w', encoding='utf-8')
            err = open(run_dir / 'errors.log', 'w', encoding='utf-8')
            self.process = subprocess.Popen(run_command(), cwd=str(PORT_DIR), env=env, stdin=subprocess.DEVNULL,
                                            stdout=out, stderr=err, creationflags=NO_WINDOW)
        except OSError as error:
            self.process = None
            self.messagebox.showerror('Killzone Shadow Fall', tr('cant_start', error=error), parent=self.root)
            return
        self.status.configure(text=tr('preparing'), fg=MUTED)
        self.update_play_state()
        threading.Thread(target=self.wait_game, args=(self.process, out, err, run_dir), daemon=True).start()

    def wait_game(self, process, out, err, run_dir):
        code = process.wait()
        out.close()
        err.close()
        self.root.after(0, lambda: self.game_ended(code, run_dir))

    def game_ended(self, code, run_dir):
        self.process = None
        self.status.configure(text='', fg=MUTED)
        self.update_play_state()
        if getattr(self, 'cache_note', None) is not None and self.cache_note.winfo_exists():
            self.refresh_cache_size()
        # 0: closed normally; 1 with an empty error log: the window was closed by the player.
        if code not in (0, None) and self.messagebox.askyesno('Killzone Shadow Fall', tr('game_ended', code=code),
                                                               parent=self.root):
            open_folder(run_dir)

    def set_window_icon(self):
        """The game's icon (sce_sys/icon0.png) on the launcher window, when Pillow can read it."""
        game = self.settings.get('game_dir')
        if not game:
            return
        try:
            from PIL import Image, ImageTk
            with Image.open(Path(game) / 'sce_sys' / 'icon0.png') as source:
                icon = ImageTk.PhotoImage(source.convert('RGB').resize((64, 64), Image.LANCZOS))
            self.root.iconphoto(True, icon)
            self.images['icon'] = icon
        except Exception:  # noqa: BLE001
            pass

    def close(self):
        self.root.destroy()


def main():
    global LANG
    args = sys.argv[1:]
    if args and args[0] in ('--run', '--script'):
        sys.exit(run_role(args))
    settings = load_launcher()
    LANG = settings.get('language') or windows_language()
    if WINDOWS:
        try:
            ctypes.windll.shcore.SetProcessDpiAwareness(1)
        except (AttributeError, OSError):
            pass
        try:  # its own taskbar button and icon, not Python's
            ctypes.windll.shell32.SetCurrentProcessExplicitAppUserModelID('Lesca.KillzoneShadowFall')
        except (AttributeError, OSError):
            pass
    import tkinter as tk
    from tkinter import filedialog, messagebox, ttk
    root = tk.Tk()
    icon = PORT_DIR / 'launcher' / 'kzsf.ico'
    if icon.is_file():
        try:
            root.iconbitmap(default=str(icon))
        except tk.TclError:
            pass
    launcher = Launcher(root, tk, ttk, filedialog, messagebox)
    launcher.set_window_icon()
    root.mainloop()


if __name__ == '__main__':
    main()
