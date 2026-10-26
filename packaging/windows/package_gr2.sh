#!/usr/bin/env bash
# Builds dist/Gravity Rush 2/, dist/GravityRush2-v<version>-windows.zip and, with Inno Setup 6
# installed (winget install JRSoftware.InnoSetup; ISCC overrides), the installer
# dist/GravityRush2-Setup-v<version>.exe (packaging/windows/gr2_setup.iss): the launcher
# (Gravity Rush 2.exe, frozen with PyInstaller so players need no Python), the game executable
# (bin/GravityRush2.exe) with the MSYS2 CLANG64 DLLs it needs, DLSS (gr2_dlss.dll and NVIDIA's
# nvngx_dlss.dll with its license, when build_dlss.sh made them), the preparation scripts and run.py.
# Run from an MSYS2 CLANG64 shell after `bash build.sh`. Freezing uses a Windows Python 3.10+
# (python.org; WINPYTHON overrides) and a private venv in out/pyenv.
# The launcher's icon is its default one (launcher/gr2_launcher.ico); the window takes the game's
# icon from the player's dump at run time.
# No game files, DLC or PS4 system modules ship: the player copies their own into game/,
# game/addcont/ and sys_modules/ or points the launcher at them.
set -euo pipefail
cd -- "$(dirname -- "$0")/../.."
source ./msys2-env.sh
[[ -f out/gr2-probe.exe ]] || { echo 'Build first: bash build.sh' >&2; exit 1; }
icon=launcher/gr2_launcher.ico
version=$(sed -n "s/^VERSION = '\(.*\)'/\1/p" launcher/gr2_launcher.py)
name='Gravity Rush 2'
dest="out/gr2-stage/$name"
# LOCALAPPDATA is not always passed into the MSYS2 shell: the user's folder then.
local_app_data=$(cygpath -u "${LOCALAPPDATA:-C:\\Users\\$(id -un)\\AppData\\Local}")

# The installer (packaging/windows/gr2_setup.iss) from the staged package, when Inno Setup 6 is
# installed (winget puts it in the user's Programs folder, an installer run as administrator in
# Program Files). --installer builds only it, from the package staged by an earlier run.
build_installer() {
    local iscc=${ISCC:-} candidate setup="dist/GravityRush2-Setup-v$version.exe"
    if [[ -z $iscc ]]; then
        for candidate in "$local_app_data/Programs/Inno Setup 6/ISCC.exe" \
                         "/c/Program Files (x86)/Inno Setup 6/ISCC.exe" "/c/Program Files/Inno Setup 6/ISCC.exe"; do
            [[ -f $candidate ]] && { iscc=$candidate; break; }
        done
    fi
    if [[ -z $iscc ]]; then
        echo 'Installer skipped: Inno Setup 6 not found (winget install JRSoftware.InnoSetup, or set ISCC=path/to/ISCC.exe)' >&2
        return 0
    fi
    mkdir -p dist
    echo "Installer: $iscc"
    # MSYS2 would take /Q and /D... for Unix paths and rewrite them: no argument conversion here.
    MSYS2_ARG_CONV_EXCL='*' "$iscc" /Q "/DAppVersion=$version" "/DNumericVersion=$(sed 's/[^0-9.].*//; s/\.$//' <<< "$version")" \
        "/DSourceDir=$(cygpath -w "$PWD/$dest")" "/DOutputDir=$(cygpath -w "$PWD/dist")" \
        "/DPortDir=$(cygpath -w "$PWD")" "$(cygpath -w "$PWD/packaging/windows/gr2_setup.iss")"
    du -sh "$setup"
    echo "Installer: $PWD/$setup"
}
if [[ ${1:-} == --installer ]]; then
    [[ -d $dest ]] || { echo "No staged package in $dest: run bash packaging/windows/package_gr2.sh first" >&2; exit 1; }
    build_installer
    exit 0
fi

python=${WINPYTHON:-}
if [[ -z $python ]]; then
    for candidate in /c/Python3*/python.exe "/c/Program Files/Python3"*/python.exe \
                     "$local_app_data"/Programs/Python/Python3*/python.exe; do
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
    --name "$name" --icon "$(cygpath -w "$PWD/$icon")" --distpath out/gr2-pyi-dist \
    --workpath out/gr2-pyi-work --specpath out/gr2-pyi-work --paths "$(cygpath -w "$PWD/scripts")" "${hidden[@]}" \
    "$(cygpath -w "$PWD/launcher/gr2_launcher.py")"

rm -rf -- out/gr2-stage
mkdir -p "$dest/bin" "$dest/launcher"
cp -r "out/gr2-pyi-dist/$name/." "$dest/"
cp "$icon" launcher/banner.jpg "$dest/launcher/"
# The game executable without debug information (out/ keeps the symbols for crash reports).
llvm-strip --strip-debug -o "$dest/bin/GravityRush2.exe" out/gr2-probe.exe
# Every DLL it loads from the CLANG64 tree (SDL3, FFmpeg, Vulkan loader, ...).
ldd "$dest/bin/GravityRush2.exe" | awk '/\/clang64\/bin\// {print $3}' | sort -u | while read -r dll; do
    cp -u "$dll" "$dest/bin/"
done
# DLSS: the bridge (a gow3_dlss.dll built before the rename works under the new name: the game
# looks for either export) and NVIDIA's runtime, which ships only with NVIDIA's license.
bridge=$(ls out/gr2_dlss.dll out/gow3_dlss.dll 2>/dev/null | head -1 || true)
if [[ -n $bridge && -f out/nvngx_dlss.dll && -f out/NVIDIA-DLSS-LICENSE.txt ]]; then
    cp "$bridge" "$dest/bin/gr2_dlss.dll"
    cp out/nvngx_dlss.dll "$dest/bin/"
    cp out/NVIDIA-DLSS-LICENSE.txt "$dest/"
    echo "DLSS included ($bridge, nvngx_dlss.dll, NVIDIA-DLSS-LICENSE.txt)"
else
    echo 'DLSS not included: run packaging/windows/build_dlss.sh first (out/gr2_dlss.dll, nvngx_dlss.dll, NVIDIA-DLSS-LICENSE.txt)' >&2
fi
cp -r scripts "$dest/"
cp run.py LICENSE README.md "$dest/"
# Empty folders the player may copy the game and the PS4 modules into (the launcher finds them).
mkdir -p "$dest/game/addcont" "$dest/sys_modules"
cp game/README.txt "$dest/game/"
cp game/addcont/README.txt "$dest/game/addcont/"
cp sys_modules/README.txt "$dest/sys_modules/"
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

build_installer
du -sh "$dest" "$zip"
echo "Play: $PWD/$play/$name.exe"
