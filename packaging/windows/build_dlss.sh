#!/usr/bin/env bash
# Builds gr2_dlss.dll (gpu/dlss_bridge, the only code that uses the NVIDIA DLSS SDK) with
# MSVC and puts it with NVIDIA's nvngx_dlss.dll and license next to out/gr2-probe.exe;
# package_gr2.sh ships them. Needs Visual Studio 2022 or its Build Tools (C++ workload):
#   winget install Microsoft.VisualStudio.2022.BuildTools --override "--wait --passive --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended"
# The SDK (github.com/NVIDIA/DLSS, the files the bridge needs) is fetched into out/dlss-sdk;
# DLSS_SDK_ROOT=<checkout> uses another one.
#   bash packaging/windows/build_dlss.sh
set -euo pipefail
cd -- "$(dirname -- "$0")/../.."
DLSS_TAG=v310.9.16
sdk=${DLSS_SDK_ROOT:-}
if [[ -z $sdk ]]; then
    sdk=$PWD/out/dlss-sdk
    if [[ ! -f $sdk/include/nvsdk_ngx_vk.h || $(git -C "$sdk" describe --tags 2>/dev/null) != "$DLSS_TAG" ]]; then
        rm -rf "$sdk"
        git clone -q --depth 1 --branch "$DLSS_TAG" --filter=blob:none --sparse https://github.com/NVIDIA/DLSS "$sdk"
        git -C "$sdk" sparse-checkout set --no-cone /include/ /LICENSE.txt \
            /lib/Windows_x86_64/x64/nvsdk_ngx_s.lib /lib/Windows_x86_64/rel/nvngx_dlss.dll
        echo "DLSS SDK $DLSS_TAG fetched into $sdk"
    fi
fi
cmake=${CMAKE_EXE:-cmake}
# Only the Vulkan headers: MSYS2's C headers must not reach MSVC.
rm -rf out/vkinclude && mkdir -p out/vkinclude
prefix=${MSYSTEM_PREFIX:-/clang64}
cp -r "$prefix/include/vulkan" "$prefix/include/vk_video" out/vkinclude/
"$cmake" -S gpu/dlss_bridge -B out/dlss-bridge -G "Visual Studio 17 2022" -A x64 \
    -DDLSS_SDK_ROOT="$(cygpath -m "$sdk")" -DVULKAN_INCLUDE="$(cygpath -m "$PWD/out/vkinclude")"
"$cmake" --build out/dlss-bridge --config Release
cp out/dlss-bridge/Release/gr2_dlss.dll "$sdk/lib/Windows_x86_64/rel/nvngx_dlss.dll" out/
cp "$sdk/LICENSE.txt" out/NVIDIA-DLSS-LICENSE.txt
echo "DLSS bridge ready: out/gr2_dlss.dll, out/nvngx_dlss.dll"
