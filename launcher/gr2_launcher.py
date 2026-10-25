#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Gravity Rush 2 (PC port) launcher for Windows (Tkinter), after the Killzone Shadow Fall port's.

One window: the game folder (CUSA04943 version 01.11 from the player's own dump, or the base game
plus the 01.11 update folder), the PS4 system modules the port links (decrypted .sprx files from
the player's console), port graphics options (settings.ini, shared with the in-game menu),
keyboard, mouse and controller mapping (GR2_KEY_MAP / GR2_PAD_MAP) and shortcuts to saves,
screenshots, logs and the cache.

Everything the port writes lives in user/ beside the launcher:
  user/launcher.json   launcher choices (folders, language, mappings)
  user/settings.ini    port options (GR2_CONFIG; the in-game menu writes it too)
  user/out/            the prepared game image (rebuilt at every start)
  user/ps4/            the console's user folder: savedata, screenshots, shader cache
  user/logs/<time>/    one folder per game start (game.log, errors.log)

Frozen with PyInstaller (packaging/windows/package_gr2.sh), the same executable also runs run.py
(`--run`) and the preparation scripts (`--script`), so players need no Python. The artwork
(banner) ships as launcher/banner.jpg; the window icon comes from the player's game folder.
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

WINDOWS = sys.platform == 'win32'
if WINDOWS:
    import ctypes
    # AMD hybrid laptops: this layer hides the discrete GPU from Vulkan and the game stays black.
    os.environ.setdefault('VK_LOADER_LAYERS_DISABLE', 'VK_LAYER_AMD_switchable_graphics')
FROZEN = getattr(sys, 'frozen', False)
PORT_DIR = Path(sys.executable).resolve().parent if FROZEN else Path(__file__).resolve().parent.parent
sys.path.insert(0, str(PORT_DIR / 'scripts'))

VERSION = '0.1.0'
APP_NAME = 'Gravity Rush 2'
TITLE_ID = 'CUSA04943'  # Europe: the release the port is built and tested on
GAME_VERSION = '01.11'
REPO_URL = 'https://github.com/Deman139/Gr2Recompile'
USER_DIR = Path(os.environ.get('GR2_PORT_USER_DIR', PORT_DIR / 'user'))
LAUNCHER_FILE = USER_DIR / 'launcher.json'
SETTINGS_FILE = USER_DIR / 'settings.ini'
PS4_DIR = USER_DIR / 'ps4'
LOGS_DIR = USER_DIR / 'logs'
KEEP_LOGS = 10
NO_WINDOW = getattr(subprocess, 'CREATE_NO_WINDOW', 0)
RESTART_CODE = 75  # the game's exit code for "Apply and restart" (src/probe.c runtime_restart)

# Firmware modules the port links (run.py SYSTEM_MODULES): libSceNgs2 is the game's audio engine.
REQUIRED_MODULES = ('libSceNgs2.sprx',)
OPTIONAL_MODULES = ('libSceJson2.sprx', 'libSceRtc.sprx', 'libSceFreeTypeOt.sprx', 'libSceFontFt.sprx',
                    'libSceFont.sprx')

# Gravity Rush 2's sky: deep night blue, warm gold (Kat's hair), violet gravity.
BG = '#0b0d18'
PANEL = '#12152a'
FIELD = '#1b1f3a'
LINE = '#2b3058'
TEXT = '#eef0fa'
MUTED = '#8e93b8'
ACCENT = '#f2b544'
ACCENT_DARK = '#b98420'
VIOLET = '#9a7cf4'
OK = '#5ad48a'
WARN = '#e8b13a'
ERROR = '#e46060'
UI_FONT = 'Segoe UI'


# ---------------------------------------------------------------------------------------------
# Texts (Russian and English).

TEXTS = {
    'ru': {
        'tab_game': 'Игра', 'tab_graphics': 'Графика', 'tab_controls': 'Управление', 'tab_advanced': 'Дополнительно',
        'play': 'ИГРАТЬ', 'playing': 'В ИГРЕ', 'preparing': 'Запуск…',
        'game_folder': 'Папка игры',
        'game_folder_help': f'Папка твоего дампа PS4 (eboot.bin, sce_module, sce_sys и файлы .psarc). '
                            f'Нужна версия {GAME_VERSION}: обновление, влитое в папку игры, или отдельной папкой ниже. '
                            f'Путь можно вставить в поле (Enter — применить) или выбрать кнопкой. Если поле пустое, '
                            f'берётся папка game рядом с лаунчером: просто скопируй файлы игры туда.',
        'update_folder': 'Папка обновления',
        'update_folder_help': f'Твой дамп не версии {GAME_VERSION}: выбери папку обновления {GAME_VERSION}.',
        'modules_folder': 'Системные модули PS4',
        'modules_help': 'Расшифрованные модули прошивки, сдампленные с твоей консоли (те же файлы, что shadPS4 '
                        'берёт из user/sys_modules). Обязателен libSceNgs2.sprx (звуковой движок игры); '
                        'libSceJson2, libSceRtc, libSceFreeTypeOt, libSceFontFt и libSceFont используются, если есть. '
                        'Если поле пустое, берётся папка sys_modules рядом с лаунчером: просто скопируй файлы .sprx туда.',
        'open_builtin': 'Открыть папку {name}',
        'dlc_found': 'DLC ({n}): {names}',
        'using_builtin': ' (папка {name} рядом с лаунчером)',
        'browse': 'Обзор…', 'choose_game': 'Выбери папку игры (с eboot.bin)',
        'choose_update': 'Выбери папку обновления', 'choose_modules': 'Выбери папку с модулями (.sprx)',
        'no_folder': 'Папка игры ещё не выбрана.',
        'no_eboot': 'В этой папке нет eboot.bin.',
        'no_sfo': 'Нет sce_sys/param.sfo: дамп неполный.',
        'wrong_game': 'Это папка другой игры ({id}). Этот порт для Gravity Rush 2 ({want}).',
        'other_region': 'Издание {id}: не проверялось (порт сделан и проверен на {want}), может работать.',
        'need_update': 'Версия {ver}: нужно обновление {want}.',
        'bad_update': 'Папка обновления не подходит: нужна версия {ver_want}, а там {id} {ver}.',
        'ready': 'Готово: {title} · {id} · версия {ver}',
        'ready_update': 'Готово: {title} · {id} · версия {ver} (с обновлением)',
        'modules_none': 'Папка модулей ещё не выбрана.',
        'modules_missing': 'Нет обязательного модуля: {names}.',
        'modules_ok': 'Модули: {found} из {total}.',
        'modules_partial': ' Нет: {names} (порт использует свои версии).',
        'tools': 'Папки и обслуживание',
        'open_saves': 'Сохранения', 'open_shots': 'Скриншоты', 'open_logs': 'Логи', 'clear_cache': 'Очистить кэш',
        'cache_size': 'Кэш: {size}. Создаётся заново во время игры (первый запуск и новые места дольше).',
        'clear_confirm': 'Будут удалены кэш шейдеров и подготовленный образ игры ({size}).\n'
                         'Сохранения не трогаются. Продолжить?',
        'cleared': 'Кэш очищен.', 'busy_running': 'Сначала закрой игру.',
        'display_mode': 'Режим экрана', 'mode_windowed': 'В окне', 'mode_borderless': 'Окно без рамки',
        'mode_fullscreen': 'Полный экран',
        'window_size': 'Размер окна',
        'frame_rate': 'Частота кадров', 'fps_30': '30 FPS (как на PS4)',
        'fps_60': '60 FPS (патч GR2fork, экспериментально)',
        'fps_help': 'Патч 60 FPS Junmin Lee (GR2fork): игра считает время шагами по 1/60 секунды, поэтому '
                    'если компьютер не держит 60 кадров, игра замедляется. Файл патча скачивается один раз '
                    'из репозитория GR2fork в user/patches.',
        'fps_failed': 'Не удалось подготовить патчи GR2fork (частота кадров, разрешение):\n{error}\n\n'
                      'Запустить игру без них (30 FPS, 1080p)?',
        'game_resolution': 'Разрешение игры', 'res_1080': '1920 × 1080 (как на PS4)',
        'res_1440': '2560 × 1440 (патч GR2fork)', 'res_2160': '3840 × 2160 (патч GR2fork)',
        'game_resolution_help': 'В каком разрешении игра рисует кадр (патч разрешения Junmin Lee, GR2fork), '
                                'в отличие от размера окна, до которого кадр 1080p только растягивается. '
                                'Выше разрешение — чётче и тяжелее для видеокарты. Окно лучше поставить того же размера.',
        'window_size_help': 'Только в оконном режиме. Картинка игры масштабируется под окно.',
        'vsync': 'Вертикальная синхронизация (VSync)', 'vsync_help': 'Без разрывов кадра.',
        'anisotropy': 'Анизотропная фильтрация', 'aniso_game': 'Как в игре',
        'mip_bias': 'Чёткость дальних текстур', 'mip_game': 'Как в игре',
        'mip_sharper': 'чётче', 'mip_softer': 'мягче',
        'mip_bias_help': 'Чётче: больше деталей вдали, но может мерцать. Мягче: наоборот.',
        'fsr1': 'Масштабирование FSR 1',
        'fsr1_help': 'Для экранов выше 1080p (1440p, 4K): чётче обычного растяжения. На 1080p ничего не меняет.',
        'sharpen': 'Повышение резкости (RCAS)', 'show_fps': 'Показывать FPS на экране',
        'gs_outline': 'Комиксный контур персонажей (экспериментально)',
        'perf_heading': 'Производительность (патчи GR2fork)',
        'perf_help': 'В людных местах частоту кадров ограничивает число объектов на экране, а не видеокарта: '
                     'эти патчи Junmin Lee убирают часть объектов и эффектов.',
        'perf_crowd': 'Прохожие, птицы, патрульные роботы', 'crowd_full': 'Как в игре',
        'crowd_half': 'Меньше (около 60%)', 'crowd_quarter': 'Мало (около 35%)',
        'perf_particles': 'Вдвое меньше частиц (дым, пыль, обломки)',
        'perf_shadows': 'Без динамических теней (статичные и запечённые остаются)',
        'perf_haze': 'Без марева (искажение над огнём и взрывами)',
        'gs_outline_help': 'Контур рисуется геометрическим шейдером, который порт пока переводит неверно: '
                           'по всему кадру появляются чёрные линии. Выключено — линий нет, но без части контура.',
        'graphics_note': 'Во время игры F12 открывает меню порта (на геймпаде R3 + L2), F11 делает скриншот. '
                         'Игра на PS4 работает в 30 FPS, порт держит ту же частоту.',
        'keyboard': 'Клавиатура', 'controller': 'Геймпад',
        'press_key': 'Нажми клавишу…', 'reset_defaults': 'По умолчанию',
        'controls_help': 'Нажми на клавишу в таблице, затем нужную клавишу, кнопку мыши или колесо (Esc — отмена). '
                         'Движение: WASD. Стрелки: крестовина.\n'
                         'Сенсорная панель: Tab / Backspace — левое / правое нажатие (фоторежим, меню паузы).',
        'controller_help': 'Кнопка геймпада для каждой кнопки PS4. Курки и стики не переназначаются.',
        'vibration': 'Вибрация', 'dead_zone': 'Мёртвая зона стиков', 'pad_speaker': 'Динамик геймпада',
        'pad_extra_help': 'Мёртвая зона: увеличь, если камера или Кэт двигаются сами (изношенный стик). '
                          'Динамик геймпада: звуки, которые PS4 отправляет в динамик DualShock; здесь они '
                          'играют через колонки (0% — выключено).',
        'mouse_look': 'Камера мышью', 'mouse_sens': 'Чувствительность', 'mouse_invert': 'Инвертировать ось Y',
        'mouse_look_help': 'Мышь поворачивает камеру как правый стик; левая кнопка — R2, правая — L2. '
                           'Левый Alt освобождает курсор.',
        'advanced_help': 'Только если что-то выглядит неправильно или игра работает плохо. Если не уверен, не меняй.',
        'async_shaders': 'Компилировать новые шейдеры в фоне',
        'async_help': 'Меньше рывков в новых местах; объект может появиться на мгновение позже.',
        'smart_pending': 'Пропускать отрисовку, пока компилируется шейдер',
        'smart_pending_help': 'Почти без рывков при первом посещении мест. Выключи, если пропадают или мигают '
                              'объекты или свет: тогда игра ждёт каждый шейдер.',
        'precompile': 'Подготавливать шейдеры из кэша при запуске',
        'precompile_help': 'Меньше рывков в уже пройденных местах, но запуск дольше. Пока не включай: '
                           'с шейдерами из кэша листва на деревьях разлетается (ищу причину).',
        'readback_heading': 'Копирование с видеокарты',
        'no_gpu_copies': 'Не копировать данные с видеокарты (как GR2fork)',
        'no_gpu_copies_help': 'Игра не получает обратно изображения и буферы, которые рисует видеокарта. '
                              'Так работает GR2fork: быстрее и без разлетающейся листвы при 60 FPS. '
                              'Если игра начнёт вылетать при запуске — выключи.',
        'readback_max': 'Предел размера копируемых изображений',
        'readback_max_help': 'Изображения крупнее остаются на видеокарте. 2 МБ дают 30 FPS; больше — '
                             'медленнее, но если где-то появятся артефакты, попробуй увеличить.',
        'perf_diag': 'Диагностика производительности (статистика в логах)',
        'perf_diag_help': 'Каждые 5 секунд пишет в лог FPS, худший кадр, ожидания видеокарты и профиль '
                          'потоков (profile.txt рядом с логом). F9 в игре записывает устройство кадра '
                          '(папка captures рядом с логом). Немного снижает FPS.',
        'stale_readback': 'Не ждать видеокарту при чтении',
        'stale_help': 'Больше FPS. Выключи, если освещение или объекты выглядят неправильно.',
        'deferred_readback': 'Отложенное копирование с видеокарты',
        'deferred_help': 'Меньше ожиданий. Выключи, если неправильно выглядят текстуры или тени.',
        'frames_queued': 'Кадров в очереди', 'queued_1': '1 (меньше задержка)', 'queued_2': '2', 'queued_0': 'Без ограничения',
        'menu_key': 'Клавиша меню порта',
        'reset_all': 'Сбросить все настройки',
        'reset_all_confirm': 'Вернуть все настройки графики, управления и дополнительные к значениям по умолчанию?',
        'language': 'Язык',
        'game_ended': 'Игра закрылась с ошибкой (код {code}).\n\nОткрыть папку с логами, чтобы посмотреть, что случилось?',
        'cant_start': 'Не удалось запустить игру:\n{error}',
        'fix_folder': 'Сначала выбери подходящие папки (вкладка «Игра»).',
        'footer_hint': 'Нативный порт для Windows · твой собственный дамп',
        'project': 'Проект на GitHub',
    },
    'en': {
        'tab_game': 'Game', 'tab_graphics': 'Graphics', 'tab_controls': 'Controls', 'tab_advanced': 'Advanced',
        'play': 'PLAY', 'playing': 'IN GAME', 'preparing': 'Starting…',
        'game_folder': 'Game folder',
        'game_folder_help': f'The folder of your PS4 dump (eboot.bin, sce_module, sce_sys and the .psarc files). '
                            f'Version {GAME_VERSION} is needed: the update merged into the game folder, or as a '
                            f'separate folder below. Paste a path into the field (Enter applies it) or browse. '
                            f'With the field empty, the game folder next to the launcher is used: just copy the '
                            f'game files there.',
        'update_folder': 'Update folder',
        'update_folder_help': f'Your dump is not version {GAME_VERSION}: choose the folder of update {GAME_VERSION}.',
        'modules_folder': 'PS4 system modules',
        'modules_help': 'Decrypted firmware modules dumped from your console (the files shadPS4 takes from '
                        'user/sys_modules). libSceNgs2.sprx is required (the game\'s audio engine); libSceJson2, '
                        'libSceRtc, libSceFreeTypeOt, libSceFontFt and libSceFont are used when present. With the '
                        'field empty, the sys_modules folder next to the launcher is used: just copy the .sprx files there.',
        'open_builtin': 'Open the {name} folder',
        'dlc_found': 'DLC ({n}): {names}',
        'using_builtin': ' (the {name} folder next to the launcher)',
        'browse': 'Browse…', 'choose_game': 'Choose the game folder (the one with eboot.bin)',
        'choose_update': 'Choose the update folder', 'choose_modules': 'Choose the modules folder (.sprx)',
        'no_folder': 'No game folder chosen yet.',
        'no_eboot': 'There is no eboot.bin in that folder.',
        'no_sfo': 'sce_sys/param.sfo is missing: the dump is incomplete.',
        'wrong_game': 'That folder holds another game ({id}). This port is for Gravity Rush 2 ({want}).',
        'other_region': 'Release {id}: untested (the port was built and tested on {want}), it may work.',
        'need_update': 'Version {ver}: update {want} is missing.',
        'bad_update': 'The update folder does not fit: version {ver_want} is needed, it holds {id} {ver}.',
        'ready': 'Ready: {title} · {id} · version {ver}',
        'ready_update': 'Ready: {title} · {id} · version {ver} (with update)',
        'modules_none': 'No modules folder chosen yet.',
        'modules_missing': 'A required module is missing: {names}.',
        'modules_ok': 'Modules: {found} of {total}.',
        'modules_partial': ' Missing: {names} (the port uses its own versions).',
        'tools': 'Folders and maintenance',
        'open_saves': 'Saved games', 'open_shots': 'Screenshots', 'open_logs': 'Logs', 'clear_cache': 'Clear cache',
        'cache_size': 'Cache: {size}. It is rebuilt while playing (the first start and new areas take longer).',
        'clear_confirm': 'The shader cache and the prepared game image ({size}) will be deleted.\n'
                         'Your saved games are kept. Continue?',
        'cleared': 'Cache cleared.', 'busy_running': 'Close the game first.',
        'display_mode': 'Display mode', 'mode_windowed': 'Windowed', 'mode_borderless': 'Borderless window',
        'mode_fullscreen': 'Fullscreen',
        'window_size': 'Window size',
        'frame_rate': 'Frame rate', 'fps_30': '30 FPS (as on PS4)',
        'fps_60': '60 FPS (GR2fork patch, experimental)',
        'fps_help': 'Junmin Lee\'s 60 FPS patch (GR2fork): the game steps its time 1/60 s per frame, so it '
                    'slows down when the PC cannot hold 60 FPS. The patch file is downloaded once from the '
                    'GR2fork repository into user/patches.',
        'fps_failed': 'Could not prepare the GR2fork patches (frame rate, resolution):\n{error}\n\n'
                      'Start the game without them (30 FPS, 1080p)?',
        'game_resolution': 'Game resolution', 'res_1080': '1920 × 1080 (as on PS4)',
        'res_1440': '2560 × 1440 (GR2fork patch)', 'res_2160': '3840 × 2160 (GR2fork patch)',
        'game_resolution_help': 'The resolution the game draws at (Junmin Lee\'s resolution patch, GR2fork), '
                                'unlike the window size, to which a 1080p frame is only stretched. Higher is '
                                'sharper and heavier on the GPU. Best with a window of the same size.',
        'window_size_help': 'Windowed mode only. The game image is scaled to the window.',
        'vsync': 'VSync (vertical sync)', 'vsync_help': 'Prevents tearing.',
        'anisotropy': 'Anisotropic filtering', 'aniso_game': 'Game default',
        'mip_bias': 'Distant texture sharpness', 'mip_game': 'Game default',
        'mip_sharper': 'sharper', 'mip_softer': 'softer',
        'mip_bias_help': 'Sharper: more distant detail, a bit of shimmer. Softer: the opposite.',
        'fsr1': 'FSR 1 upscaling',
        'fsr1_help': 'For screens above 1080p (1440p, 4K): sharper than plain stretching. No change at 1080p.',
        'sharpen': 'Sharpening (RCAS)', 'show_fps': 'Show FPS on screen',
        'gs_outline': 'Comic outline around characters (experimental)',
        'perf_heading': 'Performance (GR2fork patches)',
        'perf_help': 'In busy places the frame rate is limited by the number of objects on screen, not by the '
                     'graphics card: these patches by Junmin Lee remove some objects and effects.',
        'perf_crowd': 'Pedestrians, birds, patrol robots', 'crowd_full': 'As in the game',
        'crowd_half': 'Fewer (about 60%)', 'crowd_quarter': 'Few (about 35%)',
        'perf_particles': 'Half the particles (smoke, dust, debris)',
        'perf_shadows': 'No dynamic shadows (static and baked ones stay)',
        'perf_haze': 'No heat haze (distortion over fire and explosions)',
        'gs_outline_help': 'The outline is drawn by a geometry shader the port still translates wrongly: '
                           'black streaks across the frame. Off: no streaks, part of the outline is missing.',
        'graphics_note': 'In game, F12 opens the port menu (on a controller: R3 + L2) and F11 takes a screenshot. '
                         'The game runs at 30 FPS on PS4; the port keeps that rate.',
        'keyboard': 'Keyboard', 'controller': 'Controller',
        'press_key': 'Press a key…', 'reset_defaults': 'Defaults',
        'controls_help': 'Click a key, then press a key, a mouse button or the wheel (Esc cancels). '
                         'Move: WASD. Arrow keys: D-pad.\n'
                         'Touchpad: Tab / Backspace are left / right touchpad clicks (photo mode, pause menu).',
        'controller_help': 'The controller button that acts as each PS4 button. Triggers and sticks are fixed.',
        'vibration': 'Vibration', 'dead_zone': 'Stick dead zone', 'pad_speaker': 'Controller speaker',
        'pad_extra_help': 'Dead zone: raise it if the camera or Kat moves on its own (worn stick). '
                          'Controller speaker: sounds the PS4 sends to the DualShock speaker; here they play on '
                          'your speakers (0% = off).',
        'mouse_look': 'Camera with the mouse', 'mouse_sens': 'Sensitivity', 'mouse_invert': 'Invert Y axis',
        'mouse_look_help': 'The mouse turns the camera as the right stick; left click is R2, right click L2. '
                           'Left Alt frees the cursor.',
        'advanced_help': 'Only if something looks wrong or the game misbehaves. When in doubt, leave it as it is.',
        'async_shaders': 'Compile new shaders in the background',
        'async_help': 'Fewer stutters in new areas; an object may appear a moment later.',
        'smart_pending': 'Skip draws while their shader compiles',
        'smart_pending_help': 'Almost no stutter the first time you visit a place. Turn it off if objects or '
                              'lights go missing or flicker: the game then waits for every shader.',
        'precompile': 'Prepare cached shaders at startup',
        'precompile_help': 'Less stutter in places you already visited, but a longer start. Keep it off for '
                           'now: with cached shaders the tree foliage scatters (being investigated).',
        'readback_heading': 'Copies from the graphics card',
        'no_gpu_copies': "Don't copy data back from the graphics card (like GR2fork)",
        'no_gpu_copies_help': 'The game does not get back the images and buffers the graphics card draws. '
                              'This is how GR2fork runs: faster, and no scattered foliage at 60 FPS. '
                              'Turn it off if the game starts crashing at launch.',
        'readback_max': 'Size limit of copied images',
        'readback_max_help': 'Larger images stay on the graphics card. 2 MB gives 30 FPS; more is slower, '
                             'but try raising it if artifacts appear somewhere.',
        'perf_diag': 'Performance diagnostics (statistics in the logs)',
        'perf_diag_help': 'Every 5 seconds the log gets the FPS, the worst frame, GPU waits and a thread '
                          'profile (profile.txt beside the log). F9 in the game records the structure of a '
                          'frame (captures folder beside the log). Costs a little FPS.',
        'stale_readback': 'GPU readbacks without waiting',
        'stale_help': 'More FPS. Turn it off if lighting or objects look wrong.',
        'deferred_readback': 'Deferred GPU readbacks',
        'deferred_help': 'Fewer waits. Turn it off if textures or shadows look wrong.',
        'frames_queued': 'Frames queued', 'queued_1': '1 (lowest latency)', 'queued_2': '2', 'queued_0': 'Unlimited',
        'menu_key': 'Port menu key',
        'reset_all': 'Restore all options',
        'reset_all_confirm': 'Set every graphics, control and advanced option back to its default?',
        'language': 'Language',
        'game_ended': 'The game closed with an error (code {code}).\n\nOpen the logs folder to see what happened?',
        'cant_start': 'The game could not start:\n{error}',
        'fix_folder': 'Choose valid folders first (Game tab).',
        'footer_hint': 'Native Windows port · uses your own dump',
        'project': 'Project on GitHub',
    },
}
LANG = 'ru'


def tr(key, **values):
    text = TEXTS.get(LANG, TEXTS['en']).get(key) or TEXTS['en'].get(key, key)
    return text.format(**values) if values else text


def windows_language():
    if WINDOWS:
        try:
            return 'ru' if (ctypes.windll.kernel32.GetUserDefaultUILanguage() & 0x3ff) == 0x19 else 'en'
        except (AttributeError, OSError):
            pass
    return 'ru' if os.environ.get('LANG', '').startswith('ru') else 'en'


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
# Game and module folders.

def read_sfo(path):
    try:
        from prepare import load_sfo
        return load_sfo(path)
    except (OSError, ValueError, ImportError):
        return None


def clean_title(title):
    title = ''.join(ch for ch in str(title) if ch.isprintable() and ch not in '™®�')
    return ' '.join(word.capitalize() for word in title.split()) or APP_NAME


def is_gravity_rush_2(sfo):
    """The port's release, or another one whose title names the game (other regions, untested)."""
    title = str(sfo.get('TITLE', '')).upper()
    return str(sfo.get('TITLE_ID')) == TITLE_ID or ('GRAVITY' in title and '2' in title) or 'グラビティ' in title


def check_game(game_dir, update_dir):
    """(ok, message, needs_update, other_region) for these folders."""
    if not game_dir:
        return False, tr('no_folder'), False, False
    game = Path(game_dir)
    if not (game / 'eboot.bin').is_file():
        return False, tr('no_eboot'), False, False
    sfo = read_sfo(game / 'sce_sys' / 'param.sfo')
    if not sfo:
        return False, tr('no_sfo'), False, False
    title_id = str(sfo.get('TITLE_ID', '?'))
    if not is_gravity_rush_2(sfo):
        return False, tr('wrong_game', id=title_id, want=TITLE_ID), False, False
    title = clean_title(sfo.get('TITLE', APP_NAME))
    version = str(sfo.get('APP_VER', '?'))
    other = title_id != TITLE_ID
    note = ' ' + tr('other_region', id=title_id, want=TITLE_ID) if other else ''
    if version == GAME_VERSION:
        return True, tr('ready', title=title, id=title_id, ver=version) + note, False, other
    if not update_dir:
        return False, tr('need_update', ver=version, want=GAME_VERSION), True, other
    update = read_sfo(Path(update_dir) / 'sce_sys' / 'param.sfo') or {}
    up_id, up_ver = str(update.get('TITLE_ID', '?')), str(update.get('APP_VER', '?'))
    if up_id != title_id or up_ver != GAME_VERSION or not (Path(update_dir) / 'eboot.bin').is_file():
        return False, tr('bad_update', ver_want=GAME_VERSION, id=up_id, ver=up_ver), True, other
    return True, tr('ready_update', title=title, id=title_id, ver=up_ver) + note, True, other


def check_modules(folder):
    """(ok, message, missing optional) for the system modules folder."""
    if not folder:
        return False, tr('modules_none'), []
    path = Path(folder)
    missing = [m for m in REQUIRED_MODULES if not (path / m).is_file()]
    if missing:
        return False, tr('modules_missing', names=', '.join(missing)), []
    optional = [m for m in OPTIONAL_MODULES if not (path / m).is_file()]
    total = len(REQUIRED_MODULES) + len(OPTIONAL_MODULES)
    message = tr('modules_ok', found=total - len(optional), total=total)
    if optional:
        message += tr('modules_partial', names=', '.join(optional))
    return True, message, optional


def find_modules_folder(game_dir):
    """A sys_modules folder next to the port or in a shadPS4 install beside the game, if any."""
    candidates = [USER_DIR / 'sys_modules']
    if game_dir:
        parent = Path(game_dir).parent
        candidates += [parent / 'sys_modules'] + sorted(parent.glob('*/user/sys_modules'))
    return next((str(c) for c in candidates if (c / REQUIRED_MODULES[0]).is_file()), '')


# Folders next to the launcher the player may simply copy the files into; a path typed or chosen
# in the launcher takes precedence. GAME_HOME may hold the game directly or one folder down
# (game/CUSA04943/eboot.bin); MODULES_HOME holds the .sprx files.
GAME_HOME = PORT_DIR / 'game'
MODULES_HOME = PORT_DIR / 'sys_modules'


def builtin_game_dir():
    if (GAME_HOME / 'eboot.bin').is_file():
        return GAME_HOME
    return next((p.parent for p in sorted(GAME_HOME.glob('*/eboot.bin'))), None)


def builtin_modules_dir():
    return next((c for c in (MODULES_HOME, MODULES_HOME / 'sys_modules')
                 if (c / REQUIRED_MODULES[0]).is_file()), None)


def game_addons(game_dir):
    """The DLC of the game folder (scripts/addcont.py: <game>/addcont/<TITLE_ID>/<label>/, the
    layout shadPS4 reads), after unpacking unlock-key .pkg files dropped into the game folder."""
    if not game_dir:
        return []
    try:
        import addcont
        title_id = addcont.game_title_id(game_dir)
        if not title_id:
            return []
        addcont.install_packages(game_dir, title_id, log=lambda _message: None)
        return addcont.find_addons(game_dir, title_id)
    except Exception:  # noqa: BLE001 - DLC never stops the launcher
        return []


def addon_name(title):
    import re
    return re.sub(r'^Gravity Rush\S*\s*2\s*[-–:]\s*', '', title).strip() or title


def game_dir_of(settings):
    """The game folder to use: the one set in the launcher, else the built-in game folder."""
    chosen = settings.get('game_dir') or ''
    if chosen:
        return chosen
    found = builtin_game_dir()
    return str(found) if found else ''


def modules_dir_of(settings):
    """The modules folder to use: the one set in the launcher, else the built-in sys_modules
    folder, else one found next to the port or a shadPS4 install beside the game."""
    chosen = settings.get('modules_dir') or ''
    if chosen:
        return chosen
    found = builtin_modules_dir()
    return str(found) if found else find_modules_folder(game_dir_of(settings))


# ---------------------------------------------------------------------------------------------
# Settings.

GRAPHICS_DEFAULTS = {
    'display_mode': 'windowed', 'vsync': '1', 'anisotropy': '16', 'rcas': '0', 'fsr1': '0',
    'mip_lod_bias': '0', 'show_fps': '1', 'vibration': '100', 'async_shaders': '1',
    'stale_readback': '0', 'deferred_readback': '1', 'frames_queued': '1',
}
LAUNCHER_DEFAULTS = {'game_dir': '', 'update_dir': '', 'modules_dir': '', 'language': '',
                     'window_size': '3840x2160', 'key_map': {}, 'pad_map': {}, 'mouse_look': True,
                     'mouse_sens': 1.0, 'mouse_invert': False, 'pad_dead_zone': 6, 'pad_speaker': 100,
                     'smart_pending': True, 'precompile': False, 'readback_max_kb': 2048, 'menu_key': 'F12',
                     'gs_outline': True, 'fps': '60', 'game_resolution': '3840x2160',
                     'perf_diag': True, 'perf_crowd': 'full', 'perf_particles': False,
                     'perf_shadows': False, 'perf_haze': False, 'no_gpu_copies': False}
# The banner picture shipped with the launcher.
BANNER_FILE = PORT_DIR / 'launcher' / 'banner.jpg'
# Frame rate patches: Junmin Lee's GoldHEN/shadPS4 patch file for Gravity Rush 2 01.11 (the GR2fork
# repository; not bundled), downloaded once into user/patches, where run.py's patches.py finds it;
# user/patches.json enables the chosen one and disables the rest of that file (its other patches
# stay off unless the player edits the file).
FPS_PATCH_URL = ('https://raw.githubusercontent.com/junminlee2004/GoldHEN_Patch_Repository/main/'
                 'patches/xml/GravityDaze2-Orbis.xml')
FPS_PATCH_FILE = 'GravityDaze2-Orbis.xml'
FPS_CHOICES = ('30', '60')
RESOLUTION_CHOICES = ('1920x1080', '2560x1440', '3840x2160')
DMEM_RETAIL_MB = 4608  # runtime_memory.c's pool; patch notes ask for extra_dmem_in_mbytes on top
PATCHES_DIR = USER_DIR / 'patches'
PATCHES_CONFIG = USER_DIR / 'patches.json'


def fps_patch_names(path):
    """Names of the 01.11 eboot patches in the patch file."""
    import xml.etree.ElementTree as ET
    return [m.get('Name') for m in ET.parse(path).getroot().iter('Metadata')
            if m.get('AppVer') == GAME_VERSION and m.get('AppElf', 'eboot.bin') == 'eboot.bin' and m.get('Name')]


# Junmin Lee's performance patches in the same file (fewer objects and effects: fewer draws,
# which is what the frame rate depends on in busy scenes). launcher.json key -> patch name prefix.
PERF_PATCHES = {'crowd_half': 'Performance: half pedestrian density',
                'crowd_quarter': 'Performance: quarter pedestrian density',
                'perf_particles': 'Performance: half particle density',
                'perf_shadows': 'Performance: disable dynamic shadows',
                'perf_haze': 'Performance: disable heat-haze distortion'}
CROWD_CHOICES = ('full', 'half', 'quarter')


def performance_patch_prefixes(settings):
    """The patch name prefixes of the performance options chosen in the launcher."""
    chosen = []
    crowd = settings.get('perf_crowd', 'full')
    if crowd in ('half', 'quarter'):
        chosen.append(PERF_PATCHES['crowd_' + crowd])
    chosen += [PERF_PATCHES[key] for key in ('perf_particles', 'perf_shadows', 'perf_haze') if settings.get(key)]
    return chosen


def prepare_game_patches(fps, resolution, extras=()):
    """Selects the frame rate and resolution patches for this start ('30' and '1920x1080' turn them
    off), downloading the patch file first when needed. Returns the extra direct memory (MiB) the
    chosen patches' notes ask for. Raises OSError/ValueError when they cannot be prepared."""
    import re
    import xml.etree.ElementTree as ET
    path = PATCHES_DIR / FPS_PATCH_FILE
    wanted_any = fps != '30' or resolution != '1920x1080' or bool(extras)
    if wanted_any and not path.is_file():
        import urllib.request
        request = urllib.request.Request(FPS_PATCH_URL, headers={'User-Agent': 'Mozilla/5.0 (Windows NT 10.0)'})
        with urllib.request.urlopen(request, timeout=30) as response:
            data = response.read(16 * 1024 * 1024)
        PATCHES_DIR.mkdir(parents=True, exist_ok=True)
        partial = path.with_suffix('.part')
        partial.write_bytes(data)
        try:
            fps_patch_names(partial)
        except Exception as error:  # noqa: BLE001  (not the XML we expect)
            partial.unlink(missing_ok=True)
            raise ValueError(f'{FPS_PATCH_FILE}: {error}') from error
        partial.replace(path)
    if not path.is_file():
        return 0
    names = fps_patch_names(path)
    wanted = []
    if fps != '30':
        name = next((n for n in names if n.lower().startswith(f'{fps}fps')), None)
        if not name:
            raise ValueError(f'{FPS_PATCH_FILE} has no {fps} FPS patch for {GAME_VERSION}')
        wanted.append(name)
    if resolution != '1920x1080':
        name = next((n for n in names if n.startswith(f'Resolution {resolution} ')), None)
        if not name:
            raise ValueError(f'{FPS_PATCH_FILE} has no {resolution} resolution patch for {GAME_VERSION}')
        wanted.append(name)
    for prefix in extras:
        name = next((n for n in names if n.startswith(prefix)), None)
        if not name:
            raise ValueError(f'{FPS_PATCH_FILE} has no "{prefix}" patch for {GAME_VERSION}')
        wanted.append(name)
    extra_dmem = 0
    for meta in ET.parse(path).getroot().iter('Metadata'):
        if meta.get('Name') in wanted and meta.get('AppVer') == GAME_VERSION:
            if m := re.search(r'extra_dmem_in_mbytes\s+(\d+)', meta.get('Note', '')):
                extra_dmem = max(extra_dmem, int(m.group(1)))
    try:
        config = json.loads(PATCHES_CONFIG.read_text(encoding='utf-8'))
    except (OSError, ValueError):
        config = {}
    if not isinstance(config, dict):
        config = {}
    prefix = FPS_PATCH_FILE + '/'
    keep = lambda key: [k for k in config.get(key, []) if isinstance(k, str) and not k.startswith(prefix)]
    config['enabled'] = keep('enabled') + [prefix + n for n in wanted]
    config['disabled'] = keep('disabled') + [prefix + n for n in names if n not in wanted]
    PATCHES_CONFIG.write_text(json.dumps(config, indent=2, ensure_ascii=False) + '\n', encoding='utf-8')
    return extra_dmem


WINDOW_SIZES = ('1280x720', '1600x900', '1920x1080', '2560x1440', '3840x2160')
DISPLAY_MODES = ('windowed', 'borderless', 'fullscreen')
ANISOTROPY = ('0', '2', '4', '8', '16')
MIP_BIASES = ('-2.0', '-1.0', '-0.5', '0', '0.5', '1.0', '2.0')
FRAMES_QUEUED = ('1', '2', '0')
READBACK_LIMITS = (1024, 2048, 4096, 8192, 16384, 0)
MENU_KEYS = ('F12', 'Insert', 'Home', 'F10')

# PS4 button, label, default key (SDL scancode name) and default controller button (SDL name).
BUTTONS = (
    ('cross', '✕', 'Space', 'south'), ('circle', '○', 'C', 'east'),
    ('square', '□', 'R', 'west'), ('triangle', '△', 'Q', 'north'),
    ('l1', 'L1', '1', 'leftshoulder'), ('r1', 'R1', '3', 'rightshoulder'),
    ('l2', 'L2', 'Mouse Right', ''), ('r2', 'R2', 'Mouse Left', ''),
    ('l3', 'L3', 'Left Shift', 'leftstick'), ('r3', 'R3', 'F', 'rightstick'),
    ('options', 'OPTIONS', 'Return', 'start'),
    ('touchpad_left', 'Touch ←', 'Tab', ''), ('touchpad_right', 'Touch →', 'Backspace', 'back'),
    ('up', '↑', 'Up', 'dpup'), ('down', '↓', 'Down', 'dpdown'), ('left', '←', 'Left', 'dpleft'),
    ('right', '→', 'Right', 'dpright'),
)
PAD_BUTTONS = (
    ('', '—'), ('south', 'A / ✕'), ('east', 'B / ○'), ('west', 'X / □'), ('north', 'Y / △'),
    ('leftshoulder', 'LB / L1'), ('rightshoulder', 'RB / R1'), ('leftstick', 'LS / L3'),
    ('rightstick', 'RS / R3'), ('start', 'Start / Options'), ('back', 'Back / Share'),
    ('dpup', 'D-pad ↑'), ('dpdown', 'D-pad ↓'), ('dpleft', 'D-pad ←'), ('dpright', 'D-pad →'),
    ('misc1', 'Misc / Mic'), ('right_paddle1', 'Paddle R1'), ('left_paddle1', 'Paddle L1'),
    ('right_paddle2', 'Paddle R2'), ('left_paddle2', 'Paddle L2'),
)
TK_MOUSE = {1: 'Mouse Left', 2: 'Mouse Middle', 3: 'Mouse Right', 4: 'Mouse 4', 5: 'Mouse 5'}
# Keys the port itself uses: the menu key (chosen), the screenshot (F11 when the menu is on F12).
BASE_RESERVED_KEYS = {'W', 'A', 'S', 'D', 'Delete', 'Escape', 'F11', 'F12', 'Insert'}
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
for _digit, _name in enumerate(('KP_Insert', 'KP_End', 'KP_Down', 'KP_Next', 'KP_Left', 'KP_Begin',
                                'KP_Right', 'KP_Home', 'KP_Up', 'KP_Prior')):
    TK_TO_SDL[_name] = f'Keypad {_digit}'
    TK_TO_SDL[f'KP_{_digit}'] = f'Keypad {_digit}'

# Controls the in-game menu also changes: kept in settings.ini as well, which the game reads live.
# On load settings.ini wins (it may hold a change made in game); every save writes both.
SHARED_SETTINGS = {
    'mouse_sens': (float, lambda v: f'{float(v):.1f}'),
    'mouse_invert': (bool, lambda v: '1' if v else '0'),
    'pad_dead_zone': (int, lambda v: str(int(v))),
    'pad_speaker': (int, lambda v: str(int(v))),
}
SETTINGS_HEADER = '# Gravity Rush 2 (PC port) settings: launcher and in-game menu (F12)\n'


def sdl_key_name(keysym):
    """The SDL scancode name of a Tk keysym, or None when the port cannot read that key."""
    if keysym in TK_TO_SDL:
        return TK_TO_SDL[keysym]
    if len(keysym) == 1 and keysym.isalnum():
        return keysym.upper()
    if keysym.startswith('F') and keysym[1:].isdigit() and 1 <= int(keysym[1:]) <= 12:
        return keysym
    return None


def write_launcher_file(settings):
    USER_DIR.mkdir(parents=True, exist_ok=True)
    LAUNCHER_FILE.write_text(json.dumps(settings, indent=2, ensure_ascii=False) + '\n', encoding='utf-8')


def load_launcher():
    try:
        data = json.loads(LAUNCHER_FILE.read_text(encoding='utf-8'))
    except (OSError, ValueError):
        data = {}
    settings = {**LAUNCHER_DEFAULTS, **(data if isinstance(data, dict) else {})}
    for key in ('key_map', 'pad_map'):
        if not isinstance(settings[key], dict):
            settings[key] = {}
    from graphics_options import read_ini
    ini = read_ini(SETTINGS_FILE)
    for key, (kind, _) in SHARED_SETTINGS.items():
        if key in ini:
            try:
                settings[key] = ini[key] not in ('0', '') if kind is bool else kind(float(ini[key]))
            except ValueError:
                pass
    return settings


def ensure_settings_file():
    USER_DIR.mkdir(parents=True, exist_ok=True)
    if not SETTINGS_FILE.exists():
        SETTINGS_FILE.write_text(SETTINGS_HEADER, encoding='utf-8')


def save_launcher(settings):
    write_launcher_file(settings)
    from graphics_options import write_keys
    ensure_settings_file()
    write_keys(SETTINGS_FILE, {key: text(settings[key]) for key, (_, text) in SHARED_SETTINGS.items()
                               if key in settings})


def load_graphics():
    from graphics_options import read_ini
    ini = read_ini(SETTINGS_FILE)
    values = {key: ini.get(key, value) for key, value in GRAPHICS_DEFAULTS.items()}
    if values['display_mode'] not in DISPLAY_MODES:
        values['display_mode'] = 'windowed'
    if values['anisotropy'] not in ANISOTROPY:
        values['anisotropy'] = '0'
    try:
        bias = float(values['mip_lod_bias'])
    except ValueError:
        bias = 0.0
    values['mip_lod_bias'] = min(MIP_BIASES, key=lambda b: abs(float(b) - bias))
    if values['frames_queued'] not in FRAMES_QUEUED:
        values['frames_queued'] = '1'
    return values


def save_graphics(values):
    from graphics_options import write_keys
    ensure_settings_file()
    write_keys(SETTINGS_FILE, values)


def map_spec(mapping, column):
    """GR2_KEY_MAP / GR2_PAD_MAP for the buttons that differ from the defaults."""
    defaults = {name: (key, pad) for name, _, key, pad in BUTTONS}
    return ','.join(f'{name}={value}' for name, value in mapping.items()
                    if name in defaults and value != defaults[name][column])


def game_environment(settings, graphics, needs_update):
    """The environment run.py and the game start with, from the launcher's choices."""
    env = dict(os.environ)
    menu_key = settings.get('menu_key') if settings.get('menu_key') in MENU_KEYS else 'F12'
    readback = settings.get('readback_max_kb', 2048)
    env.update({
        'GR2_GAME_DIR': game_dir_of(settings),
        'GR2_SYS_MODULES': modules_dir_of(settings),
        'GR2_DATA_DIR': str(USER_DIR),
        'GR2_USER_DIR': str(PS4_DIR),
        'GR2_CONFIG': str(SETTINGS_FILE),
        'GR2_WINDOW_SIZE': settings.get('window_size') or '1920x1080',
        'GR2_KEY_MAP': map_spec(settings['key_map'], 0),
        'GR2_PAD_MAP': map_spec(settings['pad_map'], 1),
        'GR2_MOUSE_LOOK': '1' if settings.get('mouse_look', True) else '0',
        'GR2_MENU_KEY': menu_key,
        'GR2_READBACK_MAX_KB': str(int(readback)) if readback else '0',
        'GR2_SMART_PENDING': '1' if settings.get('smart_pending', True) else '0',
        'GR2_GS_OUTLINE': '1' if settings.get('gs_outline') else '0',
        'GR2_PRECOMPILE': os.environ.get('GR2_PRECOMPILE', '1' if settings.get('precompile', False) else '0'),
        # An "Apply and restart" in the game ends it with code 75: game_ended starts it again.
        'GR2_LAUNCHER_RESTART': '1',
        'GR2_DISCORD': '0',
        'PYTHONIOENCODING': 'utf-8',
    })
    if settings.get('no_gpu_copies'):
        # As GR2fork (readbacks = false, readbackLinearImages = false): no image or buffer
        # readbacks. With them, the 60 FPS patch scattered tree foliage.
        env['GR2_READBACK_LINEAR'] = '0'
        env['GR2_READBACKS'] = '0'
    if needs_update and settings.get('update_dir'):
        env['GR2_PATCH_DIR'] = settings['update_dir']
    else:
        env.pop('GR2_PATCH_DIR', None)
    return env


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
        for folder in (GAME_HOME, MODULES_HOME):
            try:
                folder.mkdir(exist_ok=True)
            except OSError:
                pass
        self.graphics = load_graphics()
        self.process = None
        self.capturing = None
        self.images = {}
        self.game_ok = False
        self.scale = max(1.0, root.winfo_fpixels('1i') / 96.0)
        root.title(APP_NAME)
        root.configure(bg=BG)
        root.minsize(self.px(900), self.px(760))
        root.geometry(f'{self.px(980)}x{self.px(840)}')
        self.style()
        self.build()
        root.protocol('WM_DELETE_WINDOW', self.close)

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
        tk.Label(footer, text=f'v{VERSION}  ·  {tr("footer_hint")}', fg=MUTED, bg=BG,
                 font=self.font(9)).pack(side='left')
        link = tk.Label(footer, text=tr('project'), fg=VIOLET, bg=BG, font=self.font(9, 'underline'),
                        cursor='hand2')
        link.pack(side='left', padx=(self.px(12), 0))
        link.bind('<Button-1>', lambda e: webbrowser.open(REPO_URL))
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
        for code in ('en', 'ru'):
            label = tk.Label(lang, text=code.upper(), bg=BG, fg=ACCENT if LANG == code else MUTED,
                             font=self.font(9, 'bold'), padx=self.px(6), cursor='hand2')
            label.pack(side='right')
            label.bind('<Button-1>', lambda e, c=code: self.set_language(c))
        tk.Label(lang, text=tr('language') + ':', bg=BG, fg=MUTED, font=self.font(9)).pack(side='right')
        self.show_tab(getattr(self, 'current_tab', 'game'))
        self.refresh_game()

    def play_hover(self, inside):
        if self.process is None and self.game_ok:
            self.play_button.configure(bg=TEXT if inside else ACCENT)

    def show_tab(self, key):
        self.current_tab = key
        for name, label in self.tab_labels.items():
            label.configure(fg=TEXT if name == key else MUTED, bg=PANEL if name == key else BG)
        for child in self.body.winfo_children():
            child.destroy()
        # The tab scrolls when the window is too small for it (bar and mouse wheel only then).
        canvas = self.tk.Canvas(self.body, bg=PANEL, highlightthickness=0, bd=0)
        bar = self.tk.Scrollbar(self.body, orient='vertical', command=canvas.yview)
        canvas.configure(yscrollcommand=bar.set)
        canvas.pack(side='left', fill='both', expand=True)
        holder = self.tk.Frame(canvas, bg=PANEL)
        window = canvas.create_window(0, 0, window=holder, anchor='nw')
        frame = self.tk.Frame(holder, bg=PANEL)
        frame.pack(fill='both', expand=True, padx=self.px(24), pady=self.px(18))

        def fits():
            return holder.winfo_reqheight() <= canvas.winfo_height()

        def layout(_event=None):
            canvas.itemconfigure(window, width=canvas.winfo_width())
            canvas.configure(scrollregion=(0, 0, canvas.winfo_width(), holder.winfo_reqheight()))
            if fits():
                bar.pack_forget()
                canvas.yview_moveto(0)
            elif not bar.winfo_ismapped():
                bar.pack(side='right', fill='y', before=canvas)

        def wheel(event):
            if not fits():
                canvas.yview_scroll(int(-event.delta / 120) or (-1 if event.delta > 0 else 1), 'units')
        holder.bind('<Configure>', layout)
        canvas.bind('<Configure>', layout)
        canvas.bind('<Enter>', lambda _e: canvas.bind_all('<MouseWheel>', wheel))
        canvas.bind('<Leave>', lambda _e: canvas.unbind_all('<MouseWheel>'))
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

    def banner_source(self):
        """The banner picture: the launcher's own (launcher/banner.jpg), else the game's background
        (sce_sys/pic1.png, else pic0.png). None: the plain banner."""
        if BANNER_FILE.is_file():
            return BANNER_FILE
        game = game_dir_of(self.settings)
        if not game:
            return None
        return next((Path(game) / 'sce_sys' / n for n in ('pic1.png', 'pic0.png')
                     if (Path(game) / 'sce_sys' / n).is_file()), None)

    def load_art(self):
        """The banner picture (see banner_source), fitted to the banner and darkened left."""
        path = self.banner_source()
        if not path:
            return None
        key = (str(path), self.banner.winfo_width(), self.banner.winfo_height())
        if self.images.get('banner_key') == key:
            return self.images.get('banner')
        try:
            from PIL import Image, ImageChops, ImageTk
            width, height = key[1], key[2]
            with Image.open(path) as source:
                image = source.convert('RGB')
            scale = width / image.width
            image = image.resize((width, max(height, int(image.height * scale))), Image.LANCZOS)
            top = int((image.height - height) * 0.35)
            image = image.crop((0, top, width, top + height))
            shade = Image.new('L', (width, 1))
            shade.putdata([int(225 - 160 * min(1.0, x / (width * 0.7))) for x in range(width)])
            shade = shade.resize((width, height))
            bottom = Image.new('L', (1, height))
            bottom.putdata([int(120 * max(0.0, (y - height * 0.55) / (height * 0.45))) for y in range(height)])
            bottom = bottom.resize((width, height))
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
                t = i / max(1, height)
                c.create_rectangle(0, i, width, i + 2, width=0,
                                   fill=f'#{int(14 + 30 * t):02x}{int(16 + 14 * t):02x}{int(32 + 40 * t):02x}')
        x = self.px(30)
        c.create_text(x, self.px(54), text='GRAVITY RUSH', anchor='w', fill=TEXT,
                      font=self.font(34, 'bold', 'Segoe UI Black'))
        c.create_text(x + self.px(3), self.px(98), text='2', anchor='w', fill=ACCENT,
                      font=self.font(26, 'bold', 'Segoe UI Black'))
        c.create_rectangle(x, self.px(124), x + self.px(64), self.px(127), fill=VIOLET, width=0)
        c.create_text(x, self.px(144), text='PC', anchor='w', fill=ACCENT, font=self.font(10, 'bold'))
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
        self.tk.Label(frame, text=label, bg=PANEL, fg=TEXT, font=self.font(10), width=30, anchor='w').pack(side='left')
        return frame

    def choice(self, parent, label, values, labels, current, on_change, help_text=None):
        frame = self.row(parent, label)
        combo = self.ttk.Combobox(frame, values=labels, state='readonly',
                                  width=max(24, max(len(text) for text in labels) + 2))
        combo.current(values.index(current) if current in values else 0)
        combo.pack(side='left')
        combo.bind('<<ComboboxSelected>>', lambda e: on_change(values[combo.current()]))
        if help_text and len(help_text) > 100:
            self.note(parent, '      ' + help_text)
        elif help_text:
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

    def setting_check(self, parent, label, key, help_text=None):
        """A checkbox of a launcher.json choice."""
        var = self.tk.BooleanVar(value=bool(self.settings.get(key, LAUNCHER_DEFAULTS.get(key))))

        def changed():
            self.settings[key] = var.get()
            save_launcher(self.settings)
        self.ttk.Checkbutton(parent, text=label, variable=var, command=changed).pack(anchor='w', pady=self.px(4))
        if help_text:
            self.note(parent, '      ' + help_text)
        self.images.setdefault('vars', []).append(var)
        return var

    def set_graphic(self, key, value):
        self.graphics[key] = value
        save_graphics({key: value})

    def set_setting(self, key, value):
        self.settings[key] = value
        save_launcher(self.settings)

    def folder_row(self, parent, value, command, on_edit=None, builtin=None):
        """A path field (typed or pasted; Enter or leaving the field applies it), Browse, and for
        the game and modules folders a button that opens the built-in folder."""
        row = self.tk.Frame(parent, bg=PANEL)
        row.pack(fill='x')
        entry = self.tk.Entry(row, bg=FIELD, fg=TEXT, insertbackground=TEXT, relief='flat', font=self.font(10),
                              highlightthickness=1, highlightbackground=FIELD, highlightcolor=ACCENT)
        entry.insert(0, value or '')
        entry.pack(side='left', fill='x', expand=True, ipady=self.px(6), ipadx=self.px(6))
        if on_edit:
            applied = {'text': value or ''}

            def apply(_event=None):
                text = entry.get().strip().strip('"')
                if text != applied['text']:
                    applied['text'] = text
                    on_edit(text)
            entry.bind('<Return>', apply)
            entry.bind('<FocusOut>', apply)
        self.button(row, tr('browse'), command).pack(side='left', padx=(self.px(8), 0))
        if builtin is not None:
            self.button(row, tr('open_builtin', name=builtin.name),
                        lambda: (builtin.mkdir(exist_ok=True), open_folder(builtin))).pack(side='left', padx=(self.px(8), 0))
        return entry

    def set_entry(self, entry, text):
        if entry is not None and entry.winfo_exists():
            entry.delete(0, 'end')
            entry.insert(0, text or '')

    def set_folder(self, key, text):
        """A path typed into a folder field: applied as chosen with Browse (empty: built-in folder)."""
        folder = Path(text) if text else None
        if key == 'game_dir' and folder and not (folder / 'eboot.bin').is_file():
            folder = next((p.parent for p in sorted(folder.glob('*/eboot.bin'))), folder)
        if key == 'modules_dir' and folder and not (folder / REQUIRED_MODULES[0]).is_file() and \
                (folder / 'sys_modules' / REQUIRED_MODULES[0]).is_file():
            folder = folder / 'sys_modules'
        self.settings[key] = str(folder) if folder else ''
        save_launcher(self.settings)
        if key == 'game_dir':
            self.images.pop('banner_key', None)
            self.set_window_icon()
            self.draw_banner()
        self.refresh_game()

    # -- game tab ---------------------------------------------------------------------------

    def game_tab(self, parent):
        tk = self.tk
        self.heading(parent, tr('game_folder'))
        self.game_entry = self.folder_row(parent, self.settings['game_dir'], self.choose_game,
                                          lambda text: self.set_folder('game_dir', text), GAME_HOME)
        self.note(parent, tr('game_folder_help'))
        self.update_frame = tk.Frame(parent, bg=PANEL)
        self.update_frame.pack(fill='x')
        self.game_status = tk.Label(parent, text='', bg=PANEL, font=self.font(10, 'bold'), anchor='w',
                                    justify='left', wraplength=self.px(820))
        self.game_status.pack(fill='x', pady=(self.px(10), 0))

        self.heading(parent, tr('modules_folder'), pady=(20, 6))
        self.modules_entry = self.folder_row(parent, self.settings['modules_dir'], self.choose_modules,
                                             lambda text: self.set_folder('modules_dir', text), MODULES_HOME)
        self.note(parent, tr('modules_help'))
        self.modules_status = tk.Label(parent, text='', bg=PANEL, font=self.font(10, 'bold'), anchor='w',
                                       justify='left', wraplength=self.px(820))
        self.modules_status.pack(fill='x', pady=(self.px(10), 0))

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
        game_dir, modules_dir = game_dir_of(self.settings), modules_dir_of(self.settings)
        ok, message, needs_update, other = check_game(game_dir, self.settings['update_dir'])
        modules_ok, modules_message, partial = check_modules(modules_dir)
        if game_dir and not self.settings.get('game_dir'):
            message += tr('using_builtin', name='game')
        addons = game_addons(game_dir) if ok else []
        if addons:
            message += '\n' + tr('dlc_found', n=len(addons), names=', '.join(addon_name(t) for _l, _f, t in addons))
        if modules_dir and not self.settings.get('modules_dir') and Path(modules_dir) in (MODULES_HOME, MODULES_HOME / 'sys_modules'):
            modules_message += tr('using_builtin', name='sys_modules')
        self.game_ok = ok and modules_ok
        self.needs_update = needs_update
        if getattr(self, 'game_status', None) is not None and self.game_status.winfo_exists():
            self.game_status.configure(text=('✓  ' if ok else '✕  ') + message,
                                       fg=(WARN if other else OK) if ok else
                                       (WARN if needs_update or not game_dir else ERROR))
            for child in self.update_frame.winfo_children():
                child.destroy()
            if needs_update:
                self.heading(self.update_frame, tr('update_folder'), pady=(16, 6))
                self.folder_row(self.update_frame, self.settings['update_dir'], self.choose_update,
                                lambda text: self.set_folder('update_dir', text))
                self.note(self.update_frame, tr('update_folder_help'))
        if getattr(self, 'modules_status', None) is not None and self.modules_status.winfo_exists():
            self.modules_status.configure(text=('✓  ' if modules_ok else '✕  ') + modules_message,
                                          fg=(WARN if partial else OK) if modules_ok else
                                          (WARN if not modules_dir else ERROR))
        self.update_play_state()

    def choose_game(self):
        folder = self.filedialog.askdirectory(title=tr('choose_game'), parent=self.root,
                                              initialdir=self.settings['game_dir'] or None)
        if not folder:
            return
        folder = Path(folder)
        # The eboot.bin may be one level down (a folder holding CUSA04943/).
        if not (folder / 'eboot.bin').is_file():
            folder = next((p.parent for p in sorted(folder.glob('*/eboot.bin'))), folder)
        self.set_entry(self.game_entry, str(folder))
        self.set_folder('game_dir', str(folder))

    def choose_update(self):
        folder = self.filedialog.askdirectory(title=tr('choose_update'), parent=self.root,
                                              initialdir=self.settings['update_dir'] or None)
        if folder:
            self.settings['update_dir'] = str(Path(folder))
            save_launcher(self.settings)
            self.refresh_game()

    def choose_modules(self):
        folder = self.filedialog.askdirectory(title=tr('choose_modules'), parent=self.root,
                                              initialdir=self.settings['modules_dir'] or None)
        if not folder:
            return
        folder = Path(folder)
        if not (folder / REQUIRED_MODULES[0]).is_file() and (folder / 'sys_modules' / REQUIRED_MODULES[0]).is_file():
            folder = folder / 'sys_modules'
        self.set_entry(self.modules_entry, str(folder))
        self.set_folder('modules_dir', str(folder))

    def clear_cache(self):
        if self.process is not None:
            self.messagebox.showinfo(APP_NAME, tr('busy_running'), parent=self.root)
            return
        size = human_size(sum(folder_size(p) for p in cache_paths()))
        if not self.messagebox.askyesno(APP_NAME, tr('clear_confirm', size=size), parent=self.root):
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
                    lambda v: self.set_setting('window_size', v), tr('window_size_help'))
        fps = self.settings.get('fps') if self.settings.get('fps') in FPS_CHOICES else '30'
        self.choice(parent, tr('frame_rate'), FPS_CHOICES, [tr('fps_30'), tr('fps_60')], fps,
                    lambda v: self.set_setting('fps', v), tr('fps_help'))
        resolution = (self.settings.get('game_resolution') if self.settings.get('game_resolution') in
                      RESOLUTION_CHOICES else '1920x1080')
        self.choice(parent, tr('game_resolution'), RESOLUTION_CHOICES,
                    [tr('res_1080'), tr('res_1440'), tr('res_2160')], resolution,
                    lambda v: self.set_setting('game_resolution', v), tr('game_resolution_help'))
        self.choice(parent, tr('anisotropy'), ANISOTROPY, [tr('aniso_game'), '2x', '4x', '8x', '16x'],
                    self.graphics['anisotropy'], lambda v: self.set_graphic('anisotropy', v))
        self.choice(parent, tr('mip_bias'), MIP_BIASES,
                    [f"-2 ({tr('mip_sharper')})", f"-1 ({tr('mip_sharper')})", f"-0.5 ({tr('mip_sharper')})",
                     tr('mip_game'), f"+0.5 ({tr('mip_softer')})", f"+1 ({tr('mip_softer')})",
                     f"+2 ({tr('mip_softer')})"],
                    self.graphics['mip_lod_bias'], lambda v: self.set_graphic('mip_lod_bias', v),
                    tr('mip_bias_help'))
        self.tk.Frame(parent, bg=PANEL, height=self.px(8)).pack()
        self.check(parent, tr('vsync'), 'vsync', tr('vsync_help'))
        self.check(parent, tr('fsr1'), 'fsr1')
        self.note(parent, '      ' + tr('fsr1_help'))
        self.check(parent, tr('sharpen'), 'rcas')
        self.check(parent, tr('show_fps'), 'show_fps')
        self.setting_check(parent, tr('gs_outline'), 'gs_outline', tr('gs_outline_help'))
        self.heading(parent, tr('perf_heading'), pady=(16, 4))
        self.note(parent, tr('perf_help'))
        crowd = self.settings.get('perf_crowd') if self.settings.get('perf_crowd') in CROWD_CHOICES else 'full'
        self.choice(parent, tr('perf_crowd'), CROWD_CHOICES, [tr('crowd_full'), tr('crowd_half'), tr('crowd_quarter')],
                    crowd, lambda v: self.set_setting('perf_crowd', v))
        for key in ('perf_particles', 'perf_shadows', 'perf_haze'):
            self.setting_check(parent, tr(key), key)
        self.tk.Frame(parent, bg=LINE, height=1).pack(fill='x', pady=self.px(14))
        self.note(parent, tr('graphics_note'))

    # -- controls tab -----------------------------------------------------------------------

    def controls_tab(self, parent):
        tk = self.tk
        self.note(parent, tr('controls_help'), wrap=880)
        grid = tk.Frame(parent, bg=PANEL)
        grid.pack(fill='x', pady=(self.px(8), 0))
        self.key_labels = {}
        keys, pads = self.settings['key_map'], self.settings['pad_map']
        values = [v for v, _ in PAD_BUTTONS]
        rows = (len(BUTTONS) + 1) // 2
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
        bottom.pack(fill='x', pady=(self.px(10), 0))
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
        self.pad_extra_section(parent)

    def slider(self, parent, label, key, top, default, fmt, first):
        tk = self.tk
        tk.Label(parent, text=label, bg=PANEL, fg=TEXT, font=self.font(10)).pack(
            side='left', padx=(0 if first else self.px(24), 0))
        try:
            current = max(0, min(top, int(self.settings.get(key, default))))
        except (TypeError, ValueError):
            current = default
        var = tk.IntVar(value=current)
        value_label = tk.Label(parent, text=fmt(current), bg=PANEL, fg=MUTED, font=self.font(9), width=5)

        def moved(value):
            value = int(float(value))
            value_label.configure(text=fmt(value))
            if value != self.settings.get(key):
                self.set_setting(key, value)
        self.ttk.Scale(parent, from_=0, to=top, variable=var, command=moved,
                       length=self.px(150)).pack(side='left', padx=self.px(10))
        value_label.pack(side='left')
        self.images.setdefault('vars', []).append(var)

    def pad_extra_section(self, parent):
        row = self.tk.Frame(parent, bg=PANEL)
        row.pack(fill='x', pady=(self.px(8), 0))
        self.slider(row, tr('dead_zone'), 'pad_dead_zone', 30, 0, lambda v: f'{v}%', True)
        self.slider(row, tr('pad_speaker'), 'pad_speaker', 100, 100, lambda v: f'{v}%', False)
        self.note(parent, tr('pad_extra_help'))

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
                self.set_setting('mouse_sens', value)
        self.ttk.Scale(row, from_=0.2, to=4.0, variable=sens, command=moved, length=self.px(160)).pack(
            side='left', padx=self.px(8))
        sens_label.pack(side='left')
        self.ttk.Checkbutton(row, text=tr('mouse_invert'), variable=invert, command=changed).pack(
            side='left', padx=(self.px(18), 0))
        self.images['mouse_vars'] = (look, invert, sens)
        self.note(parent, tr('mouse_look_help'))

    def reserved_keys(self):
        return BASE_RESERVED_KEYS | {self.settings.get('menu_key', 'F12')}

    def capture_key(self, name):
        if self.capturing:
            self.cancel_capture()
        self.capturing = name
        self.key_labels[name].configure(text=tr('press_key'), fg=ACCENT, highlightbackground=ACCENT)
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
        for event in ('<KeyPress>', '<ButtonPress>', '<MouseWheel>'):
            self.root.unbind(event)
        if name and name in self.key_labels and self.key_labels[name].winfo_exists():
            default = next(key for n, _, key, _ in BUTTONS if n == name)
            self.key_labels[name].configure(text=self.settings['key_map'].get(name, default), fg=TEXT,
                                            highlightbackground=LINE)

    def key_pressed(self, event):
        if not self.capturing:
            return
        if event.keysym == 'Escape':
            self.cancel_capture()
            return
        self.assign_key(sdl_key_name(event.keysym))

    def assign_key(self, key):
        name = self.capturing
        if not name:
            return
        if not key or key in self.reserved_keys():
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
        self.settings.update(key_map={}, pad_map={}, mouse_look=True, mouse_sens=1.0, mouse_invert=False,
                             pad_dead_zone=0, pad_speaker=100)
        save_launcher(self.settings)
        self.set_graphic('vibration', '100')
        self.show_tab('controls')

    # -- advanced tab -----------------------------------------------------------------------

    def advanced_tab(self, parent):
        self.note(parent, tr('advanced_help'), color=WARN)
        self.tk.Frame(parent, bg=PANEL, height=self.px(8)).pack()
        self.check(parent, tr('async_shaders'), 'async_shaders')
        self.note(parent, '      ' + tr('async_help'))
        self.setting_check(parent, tr('smart_pending'), 'smart_pending', tr('smart_pending_help'))
        self.setting_check(parent, tr('precompile'), 'precompile', tr('precompile_help'))
        self.setting_check(parent, tr('perf_diag'), 'perf_diag', tr('perf_diag_help'))
        self.heading(parent, tr('readback_heading'), pady=(16, 4))
        self.setting_check(parent, tr('no_gpu_copies'), 'no_gpu_copies', tr('no_gpu_copies_help'))
        limit = self.settings.get('readback_max_kb', 2048)
        limit = limit if limit in READBACK_LIMITS else 2048
        self.choice(parent, tr('readback_max'), READBACK_LIMITS,
                    [f'{v // 1024} MB' if v else '∞' for v in READBACK_LIMITS], limit,
                    lambda v: self.set_setting('readback_max_kb', v))
        self.note(parent, '      ' + tr('readback_max_help'))
        self.check(parent, tr('stale_readback'), 'stale_readback')
        self.note(parent, '      ' + tr('stale_help'))
        self.check(parent, tr('deferred_readback'), 'deferred_readback')
        self.note(parent, '      ' + tr('deferred_help'))
        self.tk.Frame(parent, bg=PANEL, height=self.px(8)).pack()
        self.choice(parent, tr('frames_queued'), FRAMES_QUEUED, [tr('queued_1'), tr('queued_2'), tr('queued_0')],
                    self.graphics['frames_queued'], lambda v: self.set_graphic('frames_queued', v))
        menu_key = self.settings.get('menu_key') if self.settings.get('menu_key') in MENU_KEYS else 'F12'
        self.choice(parent, tr('menu_key'), MENU_KEYS, list(MENU_KEYS), menu_key,
                    lambda v: self.set_setting('menu_key', v))
        self.tk.Frame(parent, bg=LINE, height=1).pack(fill='x', pady=self.px(14))
        self.button(parent, tr('reset_all'), self.reset_all).pack(anchor='w')

    def reset_all(self):
        if not self.messagebox.askyesno(APP_NAME, tr('reset_all_confirm'), parent=self.root):
            return
        self.graphics = dict(GRAPHICS_DEFAULTS)
        save_graphics(self.graphics)
        keep = {k: self.settings[k] for k in ('game_dir', 'update_dir', 'modules_dir', 'language') if k in self.settings}
        self.settings = {**LAUNCHER_DEFAULTS, 'key_map': {}, 'pad_map': {}, **keep}
        save_launcher(self.settings)
        self.show_tab('advanced')

    # -- playing ----------------------------------------------------------------------------

    def update_play_state(self):
        if not hasattr(self, 'play_button') or not self.play_button.winfo_exists():
            return
        if self.process is not None:
            self.play_button.configure(text=tr('playing'), bg=FIELD, fg=MUTED, cursor='arrow')
        elif not self.game_ok:
            self.play_button.configure(text=tr('play'), bg=FIELD, fg=MUTED, cursor='arrow')
        else:
            self.play_button.configure(text=tr('play'), bg=ACCENT, fg=BG, cursor='hand2')

    def play(self):
        if self.process is not None:
            return
        ok, message, needs_update, _ = check_game(game_dir_of(self.settings), self.settings['update_dir'])
        modules_ok, modules_message, _ = check_modules(modules_dir_of(self.settings))
        if not ok or not modules_ok:
            self.messagebox.showwarning(APP_NAME, f'{message if not ok else modules_message}\n\n{tr("fix_folder")}',
                                        parent=self.root)
            self.show_tab('game')
            return
        save_graphics(self.graphics)
        save_launcher(self.settings)
        fps = self.settings.get('fps') if self.settings.get('fps') in FPS_CHOICES else '30'
        resolution = (self.settings.get('game_resolution') if self.settings.get('game_resolution') in
                      RESOLUTION_CHOICES else '1920x1080')
        try:
            extra_dmem = prepare_game_patches(fps, resolution, performance_patch_prefixes(self.settings))
        except Exception as error:  # noqa: BLE001  (network, HTTP, file or XML errors: shown to the player)
            if not self.messagebox.askyesno(APP_NAME, tr('fps_failed', error=error), parent=self.root):
                return
            resolution = '1920x1080'
            try:
                extra_dmem = prepare_game_patches('30', resolution)
            except Exception:  # noqa: BLE001
                extra_dmem = 0
        # Shaders cached for another game resolution drew the comic panels' borders at the old
        # positions, and a cache from another frame rate patch could crash the game: the shader
        # cache is cleared when either changes.
        cache_key = f'{resolution}@{fps}'
        if self.settings.get('cache_resolution', '1920x1080@30') != cache_key:
            shutil.rmtree(PS4_DIR / 'cache', ignore_errors=True)
            self.settings['cache_resolution'] = cache_key
            save_launcher(self.settings)
        run_dir = LOGS_DIR / time.strftime('%Y-%m-%d_%H-%M-%S')
        run_dir.mkdir(parents=True, exist_ok=True)
        prune_logs()
        env = game_environment(self.settings, self.graphics, needs_update)
        if extra_dmem:
            env['GR2_DMEM_MB'] = str(DMEM_RETAIL_MB + extra_dmem)
        if self.settings.get('perf_diag'):
            env.update(GR2_FRAME_STATS='1', GR2_PERF_DIAG='1', GR2_PROFILE=str(run_dir / 'profile.txt'),
                       GR2_FRAME_CAPTURE_KEY='1', GR2_CAPTURE_DIR=str(run_dir / 'captures'))
        if resolution != '1920x1080' and env.get('GR2_READBACK_MAX_KB', '0') != '0':
            # Render targets grow with the resolution: the readback size limit grows with them, so
            # the same images (those the CPU reads) are still copied back.
            w, h = (int(v) for v in resolution.split('x'))
            env['GR2_READBACK_MAX_KB'] = str(int(int(env['GR2_READBACK_MAX_KB']) * w * h / (1920 * 1080)))
        try:
            out = open(run_dir / 'game.log', 'w', encoding='utf-8')
            err = open(run_dir / 'errors.log', 'w', encoding='utf-8')
            self.process = subprocess.Popen(run_command(), cwd=str(PORT_DIR), env=env, stdin=subprocess.DEVNULL,
                                            stdout=out, stderr=err, creationflags=NO_WINDOW)
        except OSError as error:
            self.process = None
            self.messagebox.showerror(APP_NAME, tr('cant_start', error=error), parent=self.root)
            return
        self.status.configure(text=tr('preparing'), fg=MUTED)
        self.update_play_state()
        threading.Thread(target=self.wait_game, args=(self.process, out, err, run_dir), daemon=True).start()

    def wait_game(self, process, out, err, run_dir):
        code = process.wait()
        try:  # Windows status codes (0xC0000005 ...) tell how a run ended without a report
            err.write(f'Launcher: the game ended with exit code {code} (0x{code & 0xffffffff:08X})\n')
        except (OSError, ValueError, TypeError):
            pass
        out.close()
        err.close()
        self.root.after(0, lambda: self.game_ended(code, run_dir))

    def game_ended(self, code, run_dir):
        self.process = None
        self.status.configure(text='', fg=MUTED)
        self.update_play_state()
        if code == RESTART_CODE:
            # "Apply and restart" in the game: the options it changed (settings.ini) are read again.
            self.settings = load_launcher()
            self.graphics = load_graphics()
            self.build()
            self.play()
            return
        if getattr(self, 'cache_note', None) is not None and self.cache_note.winfo_exists():
            self.refresh_cache_size()
        if code not in (0, None) and self.messagebox.askyesno(APP_NAME, tr('game_ended', code=code),
                                                               parent=self.root):
            open_folder(run_dir)

    def set_window_icon(self):
        """The game's icon (sce_sys/icon0.png) on the launcher window, when Pillow can read it."""
        game = game_dir_of(self.settings)
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
            ctypes.windll.shell32.SetCurrentProcessExplicitAppUserModelID('Gr2Recompile.GravityRush2')
        except (AttributeError, OSError):
            pass
    import tkinter as tk
    from tkinter import filedialog, messagebox, ttk
    root = tk.Tk()
    icon = PORT_DIR / 'launcher' / 'gr2.ico'
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
