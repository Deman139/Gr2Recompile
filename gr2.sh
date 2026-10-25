#!/usr/bin/env bash
# Gravity Rush 2: prepares the game image and starts it, from the MSYS2 CLANG64 shell after
# `bash build.sh`. The whole console output is also written to user/logs/gr2-<date>.log.
#
#   GR2_GAME=/d/Games/CUSA04943 GR2_SYS_MODULES=/d/ps4/sys_modules bash gr2.sh
#
# GR2_GAME: the game folder (eboot.bin, sce_module, sce_sys, the .psarc files), with the 01.11
#           update merged in.
# GR2_SYS_MODULES: decrypted PS4 firmware modules (.sprx) dumped from your console, the folder
#           shadPS4 uses as user/sys_modules. libSceNgs2.sprx is required (the game's audio);
#           libSceJson2, libSceRtc, libSceFreeTypeOt, libSceFontFt and libSceFont are used when there.
# GR2_PROFILE=1: sampling profiler report (where each thread spends its time, every 5 s) in
#           user/logs/profile-<date>.txt, beside the log, and GPU frame statistics in the log.
# Other GR2_* variables (see run.py) pass through (the older GOW3_* names too); arguments go to
# the game executable.
set -euo pipefail
cd -- "$(dirname -- "$0")"
GR2_GAME=${GR2_GAME:-${GR2_GAME_DIR:-}}
GR2_SYS_MODULES=${GR2_SYS_MODULES:-}
if [[ -z $GR2_GAME || ! -f $GR2_GAME/eboot.bin ]]; then
    echo "Set GR2_GAME to the Gravity Rush 2 folder with eboot.bin." >&2; exit 1
fi
if [[ -z $GR2_SYS_MODULES || ! -f $GR2_SYS_MODULES/libSceNgs2.sprx ]]; then
    echo "Set GR2_SYS_MODULES to the folder with libSceNgs2.sprx (firmware modules from your PS4)." >&2; exit 1
fi
case $(uname -s) in
MINGW*|MSYS*)
    # shellcheck source=msys2-env.sh
    source ./msys2-env.sh
    to_native() { cygpath -w "$1"; }
    ;;
*) to_native() { printf '%s\n' "$1"; } ;;
esac
export GR2_GAME_DIR GR2_SYS_MODULES
GR2_GAME_DIR=$(to_native "$GR2_GAME")
GR2_SYS_MODULES=$(to_native "$GR2_SYS_MODULES")
mkdir -p user/logs
stamp=$(date +%Y%m%d-%H%M%S)
log=user/logs/gr2-$stamp.log
if [[ ${GR2_PROFILE:-} == 1 ]]; then
    export GR2_PROFILE
    GR2_PROFILE=$(to_native "$PWD/user/logs/profile-$stamp.txt")
    export GR2_FRAME_STATS=1 GR2_PERF_DIAG=1
    echo "Profile: $GR2_PROFILE"
fi
echo "Log: $PWD/$log"
set +e
python -u run.py "$@" 2>&1 | tee "$log"
status=${PIPESTATUS[0]}
echo "Exit status: $status" | tee -a "$log"
exit "$status"
