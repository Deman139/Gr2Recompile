#!/usr/bin/env bash
# Builds dist/Gravity Rush 2/ and dist/GravityRush2-v<version>-windows.zip: the launcher
# (Gravity Rush 2.exe, frozen with PyInstaller so players need no Python), the game executable
# (bin/GravityRush2.exe) with the MSYS2 CLANG64 DLLs it needs, the preparation scripts and run.py.
# Run from an MSYS2 CLANG64 shell after `bash build.sh`. Freezing uses a Windows Python 3.10+
# (python.org; WINPYTHON overrides) and a private venv in out/pyenv.
# The icon is made from the player's own dump once (it is not in the repository):
#   python packaging/windows/make_game_icon.py "<game>/sce_sys/icon0.png" launcher/gr2.ico
# No game files and no PS4 system modules ship: the player points the launcher at their own.
set -euo pipefail
cd -- "$(dirname -- "$0")/../.."
source ./msys2-env.sh
[[ -f out/gr2-probe.exe ]] || { echo 'Build first: bash build.sh' >&2; exit 1; }
[[ -f launcher/gr2.ico ]] || { echo 'launcher/gr2.ico is missing: python packaging/windows/make_game_icon.py <game>/sce_sys/icon0.png launcher/gr2.ico' >&2; exit 1; }
version=$(sed -n "s/^VERSION = '\(.*\)'/\1/p" launcher/gr2_launcher.py)
name='Gravity Rush 2'

python=${WINPYTHON:-}
if [[ -z $python ]]; then
    for candidate in /c/Python3*/python.exe "${LOCALAPPDATA:-/c/Users/$USER/AppData/Local}"/Programs/Python/Python3*/python.exe; do
        [[ -x $candidate ]] && python=$candidate
    done
fi
[[ -n $python ]] || { echo 'Need a Windows Python 3 (python.org) or WINPYTHON=path\to\python.exe' >&2; exit 1; }
export USERPROFILE=${USERPROFILE:-$(cygpath -w "/c/Users/$(id -un)")}
if [[ ! -x out/pyenv/Scripts/python.exe ]]; then
    "$python" -m venv out/pyenv
fi
out/pyenv/Scripts/python.exe -m pip install -q --disable-pip-version-check pyinstaller pillow iced-x86==1.21.0
# The scripts run inside the launcher executable (--script): the standard modules they import come along.
hidden=()
for module in argparse base64 collections hashlib json re shutil socket struct tempfile threading uuid \
              xml.etree.ElementTree urllib.request ctypes.wintypes iced_x86; do
    hidden+=(--hidden-import "$module")
done
out/pyenv/Scripts/python.exe -m PyInstaller --noconfirm --clean --log-level WARN --windowed --copy-metadata pillow \
    --name "$name" --icon "$(cygpath -w "$PWD/launcher/gr2.ico")" --distpath out/gr2-pyi-dist \
    --workpath out/gr2-pyi-work --specpath out/gr2-pyi-work --paths "$(cygpath -w "$PWD/scripts")" "${hidden[@]}" \
    "$(cygpath -w "$PWD/launcher/gr2_launcher.py")"

dest="out/gr2-stage/$name"
rm -rf -- out/gr2-stage
mkdir -p "$dest/bin" "$dest/launcher"
cp -r "out/gr2-pyi-dist/$name/." "$dest/"
cp launcher/gr2.ico "$dest/launcher/"
# The game executable without debug information (out/ keeps the symbols for crash reports).
llvm-strip --strip-debug -o "$dest/bin/GravityRush2.exe" out/gr2-probe.exe
# Every DLL it loads from the CLANG64 tree (SDL3, FFmpeg, Vulkan loader, ...).
ldd "$dest/bin/GravityRush2.exe" | awk '/\/clang64\/bin\// {print $3}' | sort -u | while read -r dll; do
    cp -u "$dll" "$dest/bin/"
done
cp -r scripts "$dest/"
cp run.py LICENSE README.md "$dest/"
find "$dest" -name __pycache__ -prune -exec rm -r {} +
# No game files, no firmware modules and nothing a player made (saves, caches, settings, logs).
forbidden=$(find "$dest" \( -iname eboot.bin -o -iname sce_module -o -iname sce_sys -o -iname savedata \
    -o -iname user -o -iname cache -o -iname '*.log' -o -iname '*.ini' -o -iname launcher.json \
    -o -iname '*.spv' -o -iname '*.pkg' -o -iname '*.sprx' -o -iname '*.prx' -o -iname '*.psarc' \) -print)
if [[ -n $forbidden ]]; then
    echo "Package holds files that must not ship:" >&2
    echo "$forbidden" >&2
    exit 1
fi
zip="dist/GravityRush2-v$version-windows.zip"
mkdir -p dist
rm -f "$zip"
(cd out/gr2-stage && powershell -NoProfile -Command \
    "Compress-Archive -Path '$name' -DestinationPath '../../$zip'")

# Refresh dist/<name> (a playable copy), keeping what the player made there (user/).
play="dist/$name"
running=$(powershell -NoProfile -Command \
    "@(Get-Process | Where-Object { \$_.Path -like '$(cygpath -w "$PWD/$play")\\*' }).Count" | tr -d '\r')
if [[ ${running:-0} != 0 ]]; then
    echo "$play is in use ($running processes): not refreshed; the zip is ready." >&2
else
    mkdir -p "$play"
    find "$play" -mindepth 1 -maxdepth 1 ! -name user -exec rm -rf {} +
    cp -r "$dest/." "$play/"
fi
du -sh "$dest" "$zip"
echo "Play: $PWD/$play/$name.exe"
