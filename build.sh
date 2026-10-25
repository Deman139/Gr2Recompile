#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "$0")"
mkdir -p out
# "Gravity Rush 2.exe" (packaging/windows/launcher_exe.c): the launcher as a Windows program, with
# the game's own icon once the launcher knows the game folder (launcher/gr2.ico), else the port's.
# A failure here only warns: the game itself is built.
build_launcher_exe() {
    local py icon=launcher/gr2_port.ico game rc_tool
    py=$(command -v python || true)
    if [[ ! -f launcher/gr2.ico && -n $py ]]; then
        game=''
        [[ -f user/launcher.json ]] &&
            game=$("$py" -c "import json; print(json.load(open('user/launcher.json', encoding='utf-8')).get('game_dir', ''))" 2>/dev/null || true)
        if [[ -z $game ]]; then
            for c in game game/*/; do [[ -f ${c%/}/sce_sys/icon0.png ]] && { game=${c%/}; break; }; done
        fi
        if [[ -n $game && -f $game/sce_sys/icon0.png ]]; then
            "$py" packaging/windows/make_game_icon.py "$game/sce_sys/icon0.png" launcher/gr2.ico >/dev/null 2>&1 || true
        fi
    fi
    [[ -f launcher/gr2.ico ]] && icon=launcher/gr2.ico
    rc_tool=$(command -v windres || command -v llvm-windres || true)
    if [[ -z $rc_tool ]]; then echo 'Gravity Rush 2.exe skipped: no windres' >&2; return 0; fi
    local pydir=''
    [[ -n $py ]] && pydir=$(cygpath -w "$(dirname "$py")" | sed 's/\\/\\\\/g')
    printf '#define GR2_PYTHON_DIR L"%s"\n' "$pydir" > out/launcher_exe_config.h
    printf '1 ICON "%s"\n' "$(cygpath -m "$PWD/$icon")" > out/launcher_exe.rc
    if "$rc_tool" out/launcher_exe.rc -O coff -o out/launcher_exe_res.o &&
       clang -O2 -municode -mwindows -include out/launcher_exe_config.h packaging/windows/launcher_exe.c \
             out/launcher_exe_res.o -o "Gravity Rush 2.exe"; then
        echo "Built $PWD/Gravity Rush 2.exe (icon: $icon)"
    else
        echo 'Gravity Rush 2.exe could not be built (the game was): start launcher/gr2_launcher.py instead' >&2
    fi
}
# This port's changes to FSR-Vulkan (gpu/patches/fsr-vulkan), applied to the submodule's working
# tree once. A tree holding an older version of them (renamed or edited patches) is reset first.
apply_fsr_patches() {
    local sub=gpu/third_party/fsr-vulkan reset=${1:-}
    for patch in gpu/patches/fsr-vulkan/*.patch; do
        git -C "$sub" apply --reverse --check "$PWD/$patch" 2>/dev/null && continue
        if ! git -C "$sub" apply --check "$PWD/$patch" 2>/dev/null; then
            if [[ -n $reset ]]; then echo "FSR-Vulkan: $patch does not apply" >&2; exit 1; fi
            echo 'FSR-Vulkan: resetting its working tree for the current patches'
            git -C "$sub" reset -q --hard && git -C "$sub" clean -qfd
            apply_fsr_patches reset; return
        fi
        git -C "$sub" apply "$PWD/$patch"
    done
}
# Windows: MSYS2 CLANG64 (packages in README.md). The GPU library is static
# there and CMake links gr2-probe.exe as well; LTO off by default (no gain measured). PGO
# (GR2_PGO: generate = instrumented build in out/gpu-pgo writing pgo/*.profraw while the game
# runs; use = build with pgo/gr2.profdata, merged from them; default use when that file exists).
case $(uname -s) in
MINGW*|MSYS*)
    # shellcheck source=msys2-env.sh
    source ./msys2-env.sh
    for tool in clang cmake ninja pkg-config; do
        command -v "$tool" >/dev/null || { echo "Need $tool from MSYS2 CLANG64 (README.md)." >&2; exit 1; }
    done
    if [[ ! -f gpu/third_party/fsr-vulkan/CMakeLists.txt || ! -f gpu/third_party/imgui/imgui.h ||
          ! -f gpu/third_party/xbyak/xbyak/xbyak.h ]]; then
        git submodule update --init --recursive
    fi
    apply_fsr_patches
    pgo=${GR2_PGO:-}
    if [[ -z $pgo ]]; then
        if [[ -f pgo/gr2.profdata ]]; then pgo=use; else pgo=off; fi
    fi
    if [[ $pgo == use && ! -f pgo/gr2.profdata ]]; then
        if compgen -G 'pgo/*.profraw' >/dev/null; then
            llvm-profdata merge -o pgo/gr2.profdata pgo/*.profraw
        else
            echo 'GR2_PGO=use: no pgo/gr2.profdata nor pgo/*.profraw' >&2; exit 1
        fi
    fi
    # The instrumented build has its own directory: the normal one stays as it was.
    build=out/gpu
    [[ $pgo == generate ]] && build=out/gpu-pgo
    mkdir -p pgo
    cmake -S gpu -B "$build" -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DGR2_PGO="$pgo" \
        -DGR2_PGO_DIR="$PWD/pgo" -DGR2_LTO="${GR2_LTO:-OFF}" -DCMAKE_C_COMPILER=clang \
        -DCMAKE_CXX_COMPILER=clang++ >/dev/null
    echo "GPU library and loader: PGO $pgo, LTO ${GR2_LTO:-OFF}"
    if ! ninja -C "$build" gr2-probe gr2-gpu-capabilities > out/gpu-build.log 2>&1; then
        grep -v '^\[' out/gpu-build.log | tail -40 >&2
        echo 'Build failed (full log: out/gpu-build.log)' >&2; exit 1
    fi
    if [[ $pgo == generate ]]; then
        echo "Built $PWD/$build/gr2-probe.exe (instrumented: play it, then GR2_PGO=use bash build.sh)"
        exit 0
    fi
    cp out/gpu/gr2-probe.exe out/gpu/gr2-gpu-capabilities.exe out/
    echo "Built $PWD/out/gr2-probe.exe"
    build_launcher_exe
    exit 0
    ;;
esac
if [[ -z ${CC:-} ]]; then
    CC=$(command -v cc || command -v gcc || true)
    if [[ -z $CC ]]; then
        for candidate in /nix/store/*-gcc-wrapper-*/bin/gcc; do
            if [[ -x $candidate ]]; then CC=$candidate; break; fi
        done
    fi
fi
if [[ -z ${CC:-} ]]; then echo 'Install GCC/Clang or set CC.' >&2; exit 1; fi
# Dependencies come from pkg-config (Vulkan loader/headers, SDL3). On NixOS the
# environment is provided by shell.nix; re-enter it automatically if needed.
if ! { command -v pkg-config >/dev/null && pkg-config --exists vulkan sdl3 && command -v cmake >/dev/null && command -v ninja >/dev/null; }; then
    if [[ -z ${GR2_IN_NIX_SHELL:-} ]] && command -v nix-shell >/dev/null; then
        exec env GR2_IN_NIX_SHELL=1 nix-shell shell.nix --run "bash build.sh $*"
    fi
    echo 'Need pkg-config with vulkan and sdl3, cmake and ninja (see shell.nix).' >&2; exit 1
fi
read -r -a includes <<< "$(pkg-config --cflags vulkan sdl3 libavcodec libavutil libswscale libpng)"
read -r -a libraries <<< "$(pkg-config --libs vulkan sdl3 libavcodec libavutil libswscale libpng)"
# GPU library (shadPS4 video core + drivers), built by CMake into out/gpu/libgr2gpu.so.
# GR2_PGO: generate (instrumented build that writes pgo/ while the game runs), use, off.
# Default: use the profile in pgo/ when there is one. GR2_LTO=OFF disables link-time optimization.
pgo=${GR2_PGO:-}
if [[ -z $pgo ]]; then
    if [[ -n $(find pgo -name '*.gcda' -print -quit 2>/dev/null) ]]; then pgo=use; else pgo=off; fi
fi
mkdir -p pgo
# Submodules (git clone --recursive, or: git submodule update --init) and the FSR-Vulkan patches.
if [[ ! -f gpu/third_party/fsr-vulkan/CMakeLists.txt || ! -f gpu/third_party/imgui/imgui.h ]]; then
    git submodule update --init --recursive
fi
apply_fsr_patches
cmake -S gpu -B out/gpu -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DGR2_PGO="$pgo" \
    -DGR2_LTO="${GR2_LTO:-ON}" -DGR2_PGO_DIR="$PWD/pgo" >/dev/null
echo "GPU library: PGO $pgo, LTO ${GR2_LTO:-ON}"
# A failed GPU build must stop here: an older libgr2gpu.so would otherwise be used silently.
if ! ninja -C out/gpu gr2gpu > out/gpu-build.log 2>&1; then
    grep -v '^\[' out/gpu-build.log | tail -40 >&2
    echo 'GPU library build failed (full log: out/gpu-build.log)' >&2; exit 1
fi
# $ORIGIN/gpu: packaged copies keep the library next to the binary without patching it.
gpu=(-Lout/gpu -lgr2gpu -Wl,-rpath,'$ORIGIN/gpu' -Wl,-rpath,"$PWD/out/gpu" -rdynamic)
runtime=(src/runtime*.c)
# Third-party decoders: compiled once, without this project's -Werror policy.
atrac9=(third_party/LibAtrac9/C/src/*.c)
if [[ ! -f out/libatrac9.a || -n $(find third_party/LibAtrac9/C/src -newer out/libatrac9.a -name '*.c') ]]; then
    rm -rf out/atrac9 && mkdir -p out/atrac9
    for source in "${atrac9[@]}"; do "$CC" -std=c99 -O2 -g -w -c "$source" -o "out/atrac9/$(basename "${source%.c}").o"; done
    ar rcs out/libatrac9.a out/atrac9/*.o
fi
# isa_emulate runs inside guest code: no XMM/YMM.
"$CC" -std=c11 -O2 -g -Wall -Wextra -Werror -mgeneral-regs-only -c src/isa_emulate.c -o out/isa_emulate.o
"$CC" -std=c11 -O2 -g -Wall -Wextra -Werror -pthread -no-pie "${includes[@]}" -I. -Isrc src/probe.c "${runtime[@]}" src/cpu_check.c src/vulkan_smoke.c out/isa_emulate.o src/isa_rewrite.c src/tsc_patch.c src/profiler_win.c out/libatrac9.a -lm -lZydis "${gpu[@]}" "${libraries[@]}" -o out/gr2-probe
echo "Built $PWD/out/gr2-probe"
# GPU check for run.sh (live_resolution=auto): links only the Vulkan loader.
"$CC" -std=c11 -O2 -Wall -Wextra -Werror -Igpu/shadps4 tools/gpu_capabilities.c "${libraries[@]}" -o out/gr2-gpu-capabilities
if [[ ${1:-} == --test ]]; then
    "$CC" -std=c11 -O2 -g -Wall -Wextra -Werror -pthread "${includes[@]}" -I. -Isrc tests/test_pad.c "${libraries[@]}" -o out/pad-test
    out/pad-test
    "$CC" -std=c11 -O2 -g -Wall -Wextra -Werror -pthread -I. -Isrc tests/test_runtime.c "${runtime[@]}" out/libatrac9.a -lm "${gpu[@]}" "${libraries[@]}" -o out/runtime-test
    out/runtime-test
    "$CC" -std=c11 -O2 -g -Wall -Wextra -Werror -pthread -Isrc tests/test_file_mods.c -o out/file-mods-test
    out/file-mods-test
    "$CC" -std=c11 -O2 -g -Wall -Wextra -Werror -pthread -I. -Isrc tests/test_sema.c "${runtime[@]}" out/libatrac9.a -lm "${gpu[@]}" "${libraries[@]}" -o out/sema-test
    out/sema-test
    "$CC" -std=c11 -D_GNU_SOURCE -O2 -g -Wall -Wextra -Werror -I. -Isrc tests/test_content.c src/runtime_content.c -o out/content-test
    out/content-test
    "$CC" -std=c11 -O2 -g -Wall -Wextra -Werror -pthread "${includes[@]}" -I. -Isrc tests/test_mp3.c src/runtime_mp3.c out/libatrac9.a -lm "${libraries[@]}" -o out/mp3-test
    out/mp3-test tests/data/sine.mp3
    "$CC" -std=c11 -O2 -g -Wall -Wextra -Werror -pthread "${includes[@]}" -I. -Isrc tests/test_videodec.c "${libraries[@]}" -o out/videodec-test
    out/videodec-test tests/data/tiny.h264
    "$CC" -std=c11 -O2 -g -Wall -Wextra -Werror -pthread -I. -Isrc tests/test_services.c -o out/services-test
    out/services-test
    "$CC" -std=c11 -O2 -g -Wall -Wextra -Werror -mbmi -mlzcnt -mmovbe -I. -Isrc tests/test_isa_emulate.c out/isa_emulate.o -o out/isa-emulate-test
    out/isa-emulate-test
    "$CC" -std=c11 -O2 -g -Wall -Wextra -Werror -c src/isa_rewrite.c -I. -Isrc -o out/isa_rewrite.o
    "$CC" -std=c11 -O2 -g -Wall -Wextra -Werror -mbmi -mlzcnt -I. -Isrc tests/test_isa_stub.c out/isa_rewrite.o out/isa_emulate.o -o out/isa-stub-test
    out/isa-stub-test tests/data/isa_fixture.bin
    "$CC" -std=c11 -O2 -g -Wall -Wextra -Werror -pthread -I. -Isrc tests/test_fiber.c -o out/fiber-test
    out/fiber-test
    "$CC" -std=c11 -O2 -g -Wall -Wextra -Werror -pthread -I. -Isrc tests/test_eventflag.c -o out/eventflag-test
    out/eventflag-test
    "$CC" -std=c11 -O2 -g -Wall -Wextra -Werror -pthread -I. -Isrc tests/test_audio3d.c -lm -o out/audio3d-test
    out/audio3d-test
fi
