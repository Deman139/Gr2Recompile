#!/bin/bash
# Builds gow3_dlss.dll (gpu/dlss_bridge, MSVC: Visual Studio 2022) and copies it with NVIDIA's
# nvngx_dlss.dll to out/; package_kzsf.sh ships them. DLSS_SDK_ROOT=<NVIDIA/DLSS checkout>.
set -e
cd -- "$(dirname -- "$0")/../.."
sdk=${DLSS_SDK_ROOT:?Set DLSS_SDK_ROOT to a checkout of github.com/NVIDIA/DLSS}
rm -rf out/vkinclude && mkdir -p out/vkinclude
cp -r /c/msys64/clang64/include/vulkan /c/msys64/clang64/include/vk_video out/vkinclude/
cmake -S gpu/dlss_bridge -B out/dlss-bridge -G "Visual Studio 17 2022" -A x64 \
    -DDLSS_SDK_ROOT="$(cygpath -m "$sdk")" -DVULKAN_INCLUDE="$(cygpath -m "$PWD/out/vkinclude")"
cmake --build out/dlss-bridge --config Release
cp out/dlss-bridge/Release/gow3_dlss.dll "$sdk/lib/Windows_x86_64/rel/nvngx_dlss.dll" out/
cp "$sdk/LICENSE.txt" out/NVIDIA-DLSS-LICENSE.txt
echo "DLSS bridge ready"
