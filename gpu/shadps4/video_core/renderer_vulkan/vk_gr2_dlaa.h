// SPDX-License-Identifier: GPL-2.0-or-later
// Gravity Rush 2: NVIDIA DLAA (DLSS at native resolution) in place of the game's own
// anti-aliasing compute pass (FXAA plus a light temporal blend, cs 0x6f705679).
//
// That pass reads the tonemapped frame (RGBA8 sRGB), the scene depth, the game's velocity
// target (previous - current NDC) and writes the anti-aliased frame (RGBA8, sRGB-encoded
// values) the display composite samples. With GOW3_DLAA=1 and DLSS available the port skips it:
// a small shader turns the velocity into DLSS motion vectors, DLSS writes the same output
// image, and scene geometry drawn with the scene depth gets a sub-pixel viewport jitter.
//
// Settings (environment, set by the launcher):
//   GOW3_DLAA=1               on (needs gow3_dlss.dll + nvngx_dlss.dll, NVIDIA RTX)
//   GOW3_DLAA_CS=<hex>        the game's AA compute shader hash (default 6f705679)
//   GOW3_DLAA_JITTER=0        no jitter (temporal stabilisation only)
//   GOW3_DLAA_JITTER_SIGN=-1  the opposite jitter sign convention (tests)
//   GOW3_DLAA_PHASES=<n>      jitter sequence length (default 16)
//   GOW3_DLSS_PRESET=<n>      NGX render preset (0 driver default, 10 J, 11 K)
//   GOW3_DLAA_SHARPNESS=<f>   DLSS sharpening 0..1 (older DLLs only)

#pragma once

#include <array>
#include <chrono>
#include <span>

#include "common/types.h"
#include "video_core/renderer_vulkan/vk_common.h"
#include "video_core/texture_cache/image.h"

namespace VideoCore {
class TextureCache;
}

namespace Vulkan {

class Instance;
class Scheduler;
class Runtime;

class Gr2Dlaa {
public:
    Gr2Dlaa(const Instance& instance, Scheduler& scheduler, VideoCore::TextureCache& texture_cache,
            Runtime& runtime);
    ~Gr2Dlaa();

    /// Switched on and DLSS is ready (checked at the first use).
    [[nodiscard]] bool Enabled() const noexcept {
        return enabled && !failed;
    }
    [[nodiscard]] bool IsTaa(u64 cs_hash) const noexcept {
        return Enabled() && cs_hash == taa_hash;
    }

    /// The game's AA dispatch, its resources bound (`images` as the rasterizer bound them) and
    /// outside rendering. Records DLAA into its output image; false: run the game's pass.
    bool Replace(std::span<const VideoCore::ImageId> images);

    /// Viewport shift (pixels, x right, y down) for a scene draw with depth target `depth`.
    [[nodiscard]] std::array<float, 2> JitterFor(VideoCore::ImageId depth) const noexcept {
        if (!active || depth != scene_depth) {
            return {};
        }
        return jitter;
    }

private:
    bool EnsureResources(u32 w, u32 h);
    vk::ImageView View(const VideoCore::Image& image, vk::Format format,
                       vk::ImageAspectFlags aspect);
    void NextJitter();

    const Instance& instance;
    Scheduler& scheduler;
    VideoCore::TextureCache& texture_cache;
    Runtime& runtime;

    bool enabled = false;
    bool failed = false;
    bool checked = false;
    bool active = false; ///< the last AA pass ran as DLAA: scene draws are jittered
    bool reset = true;
    bool use_jitter = true;
    float jitter_sign = 1.0f;
    u32 phases = 16;
    u32 preset = 0;
    float sharpness = 0.0f;
    u64 taa_hash = 0x6f705679;
    u32 frames = 0;

    VideoCore::ImageId scene_depth{};
    std::array<float, 2> jitter{};
    u32 jitter_index = 0;
    std::chrono::steady_clock::time_point last_frame{};
    u64 last_inputs = 0;

    u32 width = 0, height = 0;
    VideoCore::UniqueImage motion_image;
    vk::UniqueImageView motion_view;
    vk::UniqueDescriptorSetLayout desc_layout;
    vk::UniquePipelineLayout pipeline_layout;
    vk::UniquePipeline pipeline;

    struct ViewEntry {
        vk::Image image;
        u64 uid = 0;
        vk::Format format{};
        vk::ImageAspectFlags aspect{};
        vk::ImageView view;
        u64 last_use = 0;
    };
    std::array<ViewEntry, 8> views{};
    u64 view_uses = 0;
};

} // namespace Vulkan
