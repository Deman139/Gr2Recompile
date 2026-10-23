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
#           user/logs/profile-<date>.txt, beside the log.
# Other GOW3_* variables (see run.py) pass through; arguments go to the game executable.
set -euo pipefail
cd -- "$(dirname -- "$0")"
GR2_GAME=${GR2_GAME:-${GOW3_GAME_DIR:-}}
GR2_SYS_MODULES=${GR2_SYS_MODULES:-${GOW3_SYS_MODULES:-}}
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
export GOW3_GAME_DIR GOW3_SYS_MODULES
GOW3_GAME_DIR=$(to_native "$GR2_GAME")
GOW3_SYS_MODULES=$(to_native "$GR2_SYS_MODULES")
mkdir -p user/logs
stamp=$(date +%Y%m%d-%H%M%S)
log=user/logs/gr2-$stamp.log
if [[ ${GR2_PROFILE:-} == 1 ]]; then
    export GOW3_PROFILE
    GOW3_PROFILE=$(to_native "$PWD/user/logs/profile-$stamp.txt")
    echo "Profile: $GOW3_PROFILE"
fi
echo "Log: $PWD/$log"
set +e
python -u run.py "$@" 2>&1 | tee "$log"
status=${PIPESTATUS[0]}
echo "Exit status: $status" | tee -a "$log"
exit "$status"
