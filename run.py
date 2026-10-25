#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Prepares the game image and starts gr2-probe (Windows and Linux; run.sh wraps it on Linux).

Environment variables (GR2_GAME_DIR, GR2_DATA_DIR, GR2_PATCHES, GR2_MODS_*,
GR2_USER_DIR, ...). Extra arguments go to gr2-probe.
The in-game "Apply and restart" runs this script again through GR2_RESTART_COMMAND;
`--after PID` waits for the previous game process to end first (its GPU device and memory).
"""
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import sys
import time

WINDOWS = sys.platform == 'win32'
EXE_SUFFIX = '.exe' if WINDOWS else ''
if WINDOWS:
    import ctypes

PORT = Path(__file__).resolve().parent


def fail(message):
    print(message, file=sys.stderr)
    sys.exit(1)


def wait_for(pid):
    if not WINDOWS:
        deadline = time.monotonic() + 30
        while time.monotonic() < deadline:
            try:
                os.kill(pid, 0)
            except ProcessLookupError:
                return
            except PermissionError:
                pass
            time.sleep(0.1)
        return
    kernel32 = ctypes.windll.kernel32
    SYNCHRONIZE = 0x00100000
    handle = kernel32.OpenProcess(SYNCHRONIZE, False, pid)
    if handle:
        kernel32.WaitForSingleObject(handle, 30000)
        kernel32.CloseHandle(handle)


def no_console():
    """Started without a console (the launcher): console programs must not open their own."""
    if not WINDOWS:
        return 0
    return 0 if ctypes.windll.kernel32.GetConsoleWindow() else subprocess.CREATE_NO_WINDOW


def parse_cpu_features(output):
    """Instruction sets `gr2-probe --cpu-features` reports missing ([] when complete)."""
    for line in output.splitlines():
        if line.startswith('CPU: missing '):
            return [name.strip() for name in line[len('CPU: missing '):].split(',') if name.strip()]
    return []


def isa_rewrite_wanted(missing, env):
    """GR2_ISA_REWRITE=1 forces the ISA rewrite table, 0 disables it; otherwise the CPU decides."""
    policy = env.get('GR2_ISA_REWRITE')
    if policy in ('0', '1'):
        return policy == '1'
    return bool(missing)


def run_script(name, *args, capture=False, check=True):
    # Inside the packaged launcher executable (PyInstaller) the executable runs scripts itself.
    script = ['--script'] if getattr(sys, 'frozen', False) else []
    command = [sys.executable, *script, str(PORT / 'scripts' / name), *map(str, args)]
    env = {**os.environ, 'PYTHONIOENCODING': 'utf-8'}
    if capture:
        result = subprocess.run(command, stdin=subprocess.DEVNULL, stdout=subprocess.PIPE, text=True,
                                encoding='utf-8', env=env, creationflags=no_console())
        if not check:
            return result.returncode, result.stdout
        if result.returncode:
            sys.exit(result.returncode)
        return result.stdout
    # stdin given: the output handles are passed explicitly (a windowed launcher child would
    # get none otherwise).
    result = subprocess.run(command, stdin=subprocess.DEVNULL, env=env, creationflags=no_console())
    if result.returncode:
        sys.exit(result.returncode)
    return None


def launch_prefix(env):
    """Linux integrations chosen in the launcher: MangoHud is its Vulkan layer (an environment
    variable), GameMode a command that starts the game."""
    if WINDOWS:
        return []
    if env.get('GR2_MANGOHUD') == '1':
        env.setdefault('MANGOHUD', '1')
    if env.get('GR2_GAMEMODE') == '1':
        if gamemode := shutil.which('gamemoderun'):
            return [gamemode]
        print('GameMode: gamemoderun not found, starting without it', flush=True)
    return []


def find_executable(name):
    """bin/ (packaged), else out/ (built with build.sh)."""
    for candidate in (PORT / 'bin' / name, PORT / 'out' / name):
        if candidate.is_file():
            return candidate
    return None


# Gravity Rush 2 links the modules it ships (sce_module, and sysdata/libSceSecure.prx) and these
# PS4 firmware modules, which the player dumps from their console (decrypted .sprx files, the
# ones shadPS4 takes in user/sys_modules). Each serves the libraries listed: the host runtime
# then leaves them to the native code (GR2_NATIVE_LIBS, src/runtime.c).
GAME_MODULES = ('libc.prx', 'libSceFios2.prx', 'libSceJobManager.prx', 'libSceNpToolkit.prx',
                'sysdata/libSceSecure.prx')
SYSTEM_MODULES = {
    'libSceNgs2.sprx': ('libSceNgs2',),           # the game's audio engine
    'libSceJson2.sprx': ('libSceJson2',),
    'libSceRtc.sprx': ('libSceRtc',),
    'libSceFreeTypeOt.sprx': ('libSceFreeType',),  # fonts: FreeType, then libSceFontFt, libSceFont
    'libSceFontFt.sprx': ('libSceFontFt',),
    'libSceFont.sprx': ('libSceFont',),
}
REQUIRED_SYSTEM_MODULES = ('libSceNgs2.sprx',)


def gr2_modules(env, game, data):
    """(game modules, link_modules.py arguments for the firmware modules); sets GR2_NATIVE_LIBS."""
    modules = [m for m in env.get('GR2_MODULES', ' '.join(GAME_MODULES)).split()
               if '/' not in m or (game / m).is_file()]
    native = ['libSceSecure'] if 'sysdata/libSceSecure.prx' in modules else []
    # Modules from the game root are ones the game also loads itself (sceKernelLoadStartModule).
    env['GR2_STATIC_MODULES'] = ';'.join('/app0/' + m for m in modules if '/' in m)
    folder = Path(env.get('GR2_SYS_MODULES', data / 'sys_modules'))
    wanted = env.get('GR2_SYSTEM_MODULES', ' '.join(SYSTEM_MODULES)).split()
    found = [m for m in wanted if (folder / m).is_file()]
    missing = [m for m in REQUIRED_SYSTEM_MODULES if m in wanted and m not in found]
    if missing:
        fail(f'Missing PS4 system modules in {folder}: {", ".join(missing)}. Copy the decrypted .sprx '
             'files dumped from your PS4 there (the same files shadPS4 uses in user/sys_modules), '
             'or set GR2_SYS_MODULES to their folder.')
    for m in wanted:
        if m not in found:
            print(f'System module {m} not found in {folder}: its functions use the port\'s own versions', flush=True)
    for m in found:
        native.extend(SYSTEM_MODULES.get(m, ()))
    env['GR2_NATIVE_LIBS'] = ','.join(native)
    print(f'System modules: {", ".join(found) or "none"} (from {folder})', flush=True)
    return modules, (['--system-dir', str(folder), '--system', *found] if found else [])


def main():
    args = sys.argv[1:]
    if len(args) >= 2 and args[0] == '--after':
        wait_for(int(args[1]))
        args = args[2:]
    # As run.sh: relative paths (GR2_GAME_DIR, GR2_DATA_DIR, fsr4_shaders/) are from the port.
    os.chdir(PORT)
    env = os.environ
    # Variables under the port's earlier name (GOW3_*) still work; GR2_* wins when both are set.
    for key in [k for k in env if k.startswith('GOW3_')]:
        env.setdefault('GR2_' + key[5:], env[key])
    data = Path(env.get('GR2_DATA_DIR', PORT))
    out = data / 'out'
    out.mkdir(parents=True, exist_ok=True)
    env.setdefault('GR2_CONFIG', str(data / 'gr2.ini'))
    # Values this script derived on the previous start ("Apply and restart" inherits them) are
    # derived again from the new options.
    for key in env.pop('GR2_DERIVED', '').split(','):
        env.pop(key, None)
    derived = []
    sys.path.insert(0, str(PORT / 'scripts'))
    import graphics_options
    try:
        import discord_presence
    except Exception:
        discord_presence = None
    graphics, reverted = graphics_options.prepare(env['GR2_CONFIG'], out / 'graphics_state.json')
    if reverted:
        print('Graphics: the last restart never reached the game; previous options restored', flush=True)
    # Port-side output resolution (GR2_OUTPUT_RES=WxH): host scene targets and the UI at the
    # output size while the game itself stays at 1080p, so no resolution patch is applied.
    # The saved output_res never decides it (the in-game menu could have stored another one).
    env.setdefault('GR2_OUTPUT_RES', '1920x1080')
    # Killzone's UI and TAA shader hashes (GR2_UI_TRIGGER_VS, GR2_GAME_TAA_PS) do not apply to
    # Gravity Rush 2: the output-size UI composition and the TAA skip stay off unless set.
    # Upscaler quality modes (GR2_RENDER_RES=WxH below 1080p, the launcher): the loader renders
    # the 3D scene at that size (src/scene_res_hook.h) while the display and UI stay 1080p, and
    # FSR / DLSS reach the output. No resolution patch: it shrank the UI and movies too.
    render_res = env.get('GR2_RENDER_RES', '')
    if render_res and render_res != '1920x1080':
        graphics = dict(graphics, render_resolution='native')
        print(f"Graphics: scene at {render_res}, output {env['GR2_OUTPUT_RES']} by the upscaler", flush=True)
    elif env['GR2_OUTPUT_RES'] != '1920x1080':
        env.pop('GR2_RENDER_RES', None)
        graphics = dict(graphics, render_resolution='native')
        print(f"Graphics: output {env['GR2_OUTPUT_RES']} by the port (game at 1080p)", flush=True)
    else:
        env.pop('GR2_RENDER_RES', None)
    env.update(graphics_options.environment(graphics))
    if not env.get('GR2_FSR411_DIR') and not (PORT / 'fsr4_411').is_dir() and (data / 'fsr4_411').is_dir():
        env['GR2_FSR411_DIR'] = str(data / 'fsr4_411')

    # A package names the game executable after the game.
    probe = (Path(env['GR2_PROBE']) if env.get('GR2_PROBE') else
             find_executable('GravityRush2' + EXE_SUFFIX) or find_executable('gr2-probe' + EXE_SUFFIX))
    if not probe or not probe.is_file():
        fail(f'gr2-probe{EXE_SUFFIX} not found: build it with build.sh or use a packaged build.')
    # Development builds run from the MSYS2 tree: its CLANG64 DLLs (SDL3, FFmpeg, ...).
    if WINDOWS and not (probe.parent / 'SDL3.dll').is_file():
        clang64 = Path(env.get('MSYS2_ROOT', r'C:\msys64')) / 'clang64' / 'bin'
        if clang64.is_dir():
            env['PATH'] = f'{clang64}{os.pathsep}{env.get("PATH", "")}'
    # Instruction sets this CPU lacks (MOVBE, BMI1, LZCNT): the ISA rewrite covers them; exit 20
    # is a set nothing covers.
    features = subprocess.run([str(probe), '--cpu-features'], stdin=subprocess.DEVNULL, stdout=subprocess.PIPE,
                              text=True, creationflags=no_console())
    if features.returncode == 20:
        print(features.stdout, end='', flush=True)
        sys.exit(20)
    missing = parse_cpu_features(features.stdout)

    game = Path(env.get('GR2_GAME_DIR', PORT.parent / 'CUSA04943'))
    if not (game / 'eboot.bin').is_file():
        fail(f'No eboot.bin in {game} (set GR2_GAME_DIR).')
    update_view = None
    if env.get('GR2_PATCH_DIR'):
        import game_update
        try:
            update_view = game_update.create_view(game, env['GR2_PATCH_DIR'], out)
        except (ValueError, OSError) as error:
            fail(str(error))
        game = update_view
        print(f'Game and update combined from {env["GR2_PATCH_DIR"]}', flush=True)
    original_game = game.resolve()
    game = Path(run_script('mods.py', game, '--out', out,
                           '--mods-dir', env.get('GR2_MODS_DIR', data / 'mods'),
                           '--config', env.get('GR2_MODS_CONFIG', data / 'mods.json'),
                           '--enabled', env.get('GR2_MODS_ENABLED', '1'), capture=True).strip())
    mod_view = game if game.resolve() != original_game else None
    try:
        run_script('prepare.py', game, '--out', out)
        # Modules in the game root are ones the game loads itself (Killzone: all_programs.prx).
        dynamic = sorted(p.name for p in game.glob('*.prx'))
        modules, system_args = gr2_modules(env, game, data)
        run_script('link_modules.py', game, '--out', out, '--modules', *modules, *system_args,
                   *(['--dynamic', *dynamic] if dynamic else []))
        # A failed rewrite prints its reason and the game starts without the table.
        isa_args = []
        if isa_rewrite_wanted(missing, env):
            code, output = run_script('isa_rewrite.py', out / 'boot-linked.bin', '--out', out,
                                      capture=True, check=False)
            print(output, end='', flush=True)
            if code == 0:
                isa_args = ['--isa-rewrite', str(out / 'isa-rewrite.bin')]
        run_script('content_profile.py', game, '--out', out, '--sku', env.get('GR2_CONTENT_SKU', 'full'))

        import patches
        from patches import game_profile_for, patch_requirements, patch_status, selected_patches
        profile = game_profile_for(game)
        engine_fps = 60
        # The window shows the game's icon (BMP: the format SDL loads without extra libraries); on
        # Linux the launcher's own icon (the AppImage ships the one chosen when it was built) and
        # an application id the desktop entry matches, so the shell shows the same icon.
        icon_file = game / 'sce_sys' / 'icon0.png'
        if not WINDOWS:
            env.setdefault('SDL_APP_ID', 'gr2')
            if (PORT / 'launcher' / 'gr2_port.png').is_file():
                icon_file = PORT / 'launcher' / 'gr2_port.png'
        try:
            from PIL import Image
            with Image.open(icon_file) as icon:
                icon.convert('RGBA').resize((64, 64), Image.LANCZOS).save(out / 'window_icon.bmp')
            env.setdefault('GR2_WINDOW_ICON', str(out / 'window_icon.bmp'))
        except (ImportError, OSError):
            pass
        # The temporal upscalers and motion vectors are not calibrated for this game yet: they stay
        # off unless the launcher's experimental switch is on (then gr2.ini decides). Linear image
        # readbacks: Gravity Rush 2 crashes at start without them. It copies many more images a
        # frame than Killzone did, so the pending-copy budget is larger: past it, every new copy
        # drained the GPU first (10 FPS).
        if env.get('GR2_EXPERIMENTAL_UPSCALER') != '1':
            # Above 1080p the port's output path needs the upscaler's machinery (scene proxies, UI
            # composition) enabled, with the temporal upscaler itself off.
            env.setdefault('GR2_UPSCALER', 'none' if env['GR2_OUTPUT_RES'] == '1920x1080' else 'off')
        env.setdefault('GR2_READBACK_LINEAR', '1')
        env.setdefault('GR2_READBACK_COPIES', '1024')
        env.setdefault('GR2_READBACK_MB', '512')
        # Its full-screen linear render targets (8-16 MiB) are never read by the CPU: they stay on
        # the GPU, which otherwise waited for their copies every frame.
        env.setdefault('GR2_READBACK_MAX_KB', '2048')
        # The port's menu on F12 (the screenshot then on F11); GR2_MENU_KEY=Insert for the old key.
        env.setdefault('GR2_MENU_KEY', 'F12')
        if profile:
            names = selected_patches(profile[1], profile[2], env.get('GR2_PATCHES', ''), env.get('GR2_PATCHES_ONLY') == '1')
            # gr2.ini decides the resolution and frame rate patches (in-game menu, launcher).
            names = graphics_options.patch_names(names, graphics)
            names = graphics_options.restrict_to_available(names, patches.available_patches(profile[1], profile[2]))
            env['GR2_PATCHES'] = ';'.join(names)
            env['GR2_PATCHES_ONLY'] = '1'
            # The in-game menu offers the engine frame rate only when this game has such patches.
            env['GR2_FPS_PATCHES'] = '1' if any(n.startswith(graphics_options.FPS_PREFIX) for n in
                                                 patches.available_patches(profile[1], profile[2])) else '0'
        (out / 'patch_status.json').unlink(missing_ok=True)
        run_script('patches.py', '--out', out, '--extra', env.get('GR2_PATCHES', ''), '--game-dir', game,
                   '--patches-dir', env.get('GR2_PATCHES_DIR', data / 'patches'),
                   '--patches-config', env.get('GR2_PATCHES_CONFIG', data / 'patches.json'))
        if profile:
            # Only the patches found in this eboot.bin count: a frame rate patch that was skipped
            # leaves the engine at 60 FPS, and the notes' memory and VBlank needs follow the same list.
            applied = (patch_status(out) or {}).get('applied', [])
            engine_fps = graphics_options.applied_engine_fps(applied)
            dmem, vblank = patch_requirements(profile[1], applied, profile[2])
            for key, value in (('GR2_DMEM_MB', dmem), ('GR2_VBLANK_HZ', vblank)):
                if value and key not in env:
                    env[key] = str(value)
                    derived.append(key)
        if 'GR2_VBLANK_HZ' not in env:
            env['GR2_VBLANK_HZ'] = '60'
            derived.append('GR2_VBLANK_HZ')
        # Without the frame rate patch the engine steps 1/60 s per frame: the present thread must
        # not flip faster (gr2_graphics.h PresentLimit), or the game speeds up.
        env['GR2_ENGINE_FPS'] = str(engine_fps)
        derived.append('GR2_ENGINE_FPS')
        print(f'Engine frame rate: {engine_fps} FPS'
              + ('' if not profile or engine_fps > 60 or graphics['engine_fps'] == '60' else
                 ' (the frame rate patch was not found in this eboot.bin)'), flush=True)
        env['GR2_DERIVED'] = ','.join(derived)
        # The guest memory pool is committed in full at start: a resolution patch above 1080p
        # can need more than 16 GB of RAM, and the game then closes when the page file cannot grow.
        import guest_memory
        if warning := guest_memory.memory_warning(env.get('GR2_DMEM_MB'), guest_memory.physical_memory_mb(),
                                                  env.get('GR2_STREAM_MEMORY', 'cached')):
            print(warning, flush=True)

        # The in-game restart starts this script again once this process is gone.
        # GPU caches (shaders, pipelines) beside the saves; read before main(), so set here.
        user_dir = Path(env.get('GR2_USER_DIR', data / 'user')).resolve()
        user_dir.mkdir(parents=True, exist_ok=True)
        env.setdefault('GR2_GPU_USER_DIR', str(user_dir))
        from save_backup import backup
        try:
            if made := backup(user_dir):
                print(f'Saves backed up to {made}', flush=True)
        except OSError as error:
            print(f'Save backup failed, starting anyway: {error}', flush=True)
        this = ['--run'] if getattr(sys, 'frozen', False) else [str(Path(__file__).resolve())]
        # Windows: the probe starts the new launch and ends, so it waits for this process. Linux:
        # the probe replaces itself with the new launch while this process still waits for it.
        after = ['--after', str(os.getpid())] if WINDOWS else []
        restart = [sys.executable, *this, *after, *args]
        env['GR2_RESTART_COMMAND'] = subprocess.list2cmdline(restart) if WINDOWS else shlex.join(restart)
        command = [*launch_prefix(env), str(probe), str(out / 'boot-linked.bin'), '--content-profile', str(out / 'content.bin'),
                   '--patches', str(out / 'patches.bin'), '--app0', str(game),
                   '--user', str(user_dir),
                   '--timeout', env.get('GR2_TIMEOUT', '0'), *isa_args, *args]
        # No stdin: an inherited pipe (shells such as Git Bash) cost the game its console output.
        process = subprocess.Popen(command, stdin=subprocess.DEVNULL, creationflags=no_console())
        presence = None
        try:
            try:
                presence = discord_presence.Presence(pid=process.pid) if discord_presence else None
            except (OSError, RuntimeError):
                print('Discord presence unavailable; the game will continue', flush=True)
            return process.wait()
        finally:
            if presence:
                presence.close()
    finally:
        if mod_view:
            shutil.rmtree(mod_view, ignore_errors=True)
        if update_view:
            game_update.remove_view(update_view)


if __name__ == '__main__':
    try:
        sys.exit(main())
    except KeyboardInterrupt:
        sys.exit(130)
