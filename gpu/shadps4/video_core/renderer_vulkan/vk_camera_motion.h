// SPDX-License-Identifier: GPL-2.0-or-later
// gow3: camera motion vectors for temporal upscaling (docs/upscaler.md). The scene constants
// give the camera of each frame; the previous one is kept here. GOW3_DEBUG_MOTION=1 blends the
// motion vectors as colors into the frame before it is copied to the display.

#pragma once

#include <array>
#include <vector>

#include "common/types.h"
#include "video_core/renderer_vulkan/vk_common.h"
#include "video_core/buffer_cache/buffer.h"
#include "video_core/texture_cache/image.h"

namespace VideoCore {
class TextureCache;
}

namespace Vulkan {

class Instance;
class Scheduler;
class Runtime;
class ObjectMotion;

class CameraMotion {
public:
    CameraMotion(const Instance& instance, Scheduler& scheduler,
                 VideoCore::TextureCache& texture_cache, Runtime& runtime);
    ~CameraMotion();
    void InvalidateHistory() { current.valid = previous.valid = false; }
    void SetObjectMotion(ObjectMotion* motion) noexcept {
        object_motion = motion;
    }
    /// This frame's object motion image (after RecordMotion), for the debug view; or null.
    vk::ImageView ObjectMotionView() const noexcept;
    vk::Image ObjectMotionImage(u32 width, u32 height) const noexcept;

    void SetJitter(std::array<float, 2> value) noexcept {
        previous_jitter = jitter;
        jitter = value;
    }

    [[nodiscard]] bool Enabled() const noexcept {
        return debug_overlay || for_upscaler;
    }

    /// Both cameras and the scene depth of the current frame are known.
    [[nodiscard]] bool Ready() const noexcept {
        return current.valid && previous.valid && depth_id;
    }
    [[nodiscard]] VideoCore::ImageId Depth() const noexcept {
        return depth_id;
    }
    /// Killzone: the G-buffer's fourth target (RGBA16F, scene size) holds the game's own
    /// per-pixel velocity in xy (objects and the first-person weapon included), or null.
    [[nodiscard]] VideoCore::ImageId Velocity() const noexcept {
        return velocity_id;
    }
    /// A G-buffer draw's fourth color target (the frame's first one is kept).
    void OnGBufferVelocity(VideoCore::ImageId color);
    /// The scene depth, or before this frame's G-buffer pass the previous frame's one: Killzone
    /// lays scene depth down in a depth-only pass first, which must get the same jitter as the
    /// G-buffer it then tests EQUAL against.
    [[nodiscard]] bool SceneDepth(VideoCore::ImageId depth) const noexcept {
        return depth && (depth == depth_id || (!depth_id && depth == last_depth_id));
    }
    /// The render size in the scene constants (the game's viewport; its targets may be
    /// allocated larger, e.g. 1916x1080 for a 1916x1078 scene), or zero before the first camera.
    [[nodiscard]] std::array<u32, 2> RenderSize() const noexcept {
        return render_size;
    }
    /// Vertical field of view and near/far planes of the current camera.
    [[nodiscard]] float VerticalFov() const noexcept;
    [[nodiscard]] float Near() const noexcept;
    // Current projection, previous projection, and row Z of previous-view * inverse-view.
    [[nodiscard]] std::array<std::array<float, 4>, 3> TaaDepthParameters() const noexcept;

    /// Records the motion vector pass: `depth_view` (depth aspect, General layout) into
    /// `motion_view` (RG16F storage, General), pixels, previous minus current. `velocity_view`
    /// (Killzone's G-buffer velocity, sampled in General layout): used where the scene was drawn.
    void RecordMotion(vk::CommandBuffer cmdbuf, vk::ImageView depth_view, vk::ImageView motion_view,
                      u32 width, u32 height, vk::ImageView velocity_view = {});

    /// A bound constant buffer of 864 bytes: checks the scene constant signature.
    void OnConstants(const float* data);
    /// Killzone: the G-buffer pixel shaders' 640-byte view constants (column-major, right-handed
    /// view looking down -z): world to view at 0, projection at 16, view to world at 32, the
    /// render size at 114 and its reciprocal at 120.
    void OnKillzoneConstants(const float* data);

    /// The G-buffer pass (5+ color targets): its depth is the scene depth.
    void OnGBufferPass(VideoCore::ImageId depth);

    /// The pass copying the finished frame (`frame`, the last target drawn) to the display: the
    /// frame's depth and camera are complete. Records the debug overlay, starts a new frame.
    void OnDisplayPass(VideoCore::ImageId frame);

private:
    struct Camera {
        std::array<float, 12> view{};     ///< world to view, 3x4 rows
        std::array<float, 12> inv_view{}; ///< view to world, 3x4 rows
        std::array<float, 4> proj{};      ///< x scale, y scale, z scale, z offset
        bool valid = false;
    };

    void Overlay(VideoCore::ImageId frame);

    const Instance& instance;
    Scheduler& scheduler;
    VideoCore::TextureCache& texture_cache;
    Runtime& runtime;
    bool debug_overlay = false;
    bool for_upscaler = false;
    vk::UniqueDescriptorSetLayout motion_desc_layout;
    vk::UniquePipelineLayout motion_pipeline_layout;
    vk::UniquePipeline motion_pipeline;

    ObjectMotion* object_motion = nullptr;
    Camera current, previous;
    bool frame_has_camera = false;
    /// Killzone: this frame's G-buffer cameras and how many draws used each (the world's has
    /// the most; its first-person weapon has its own field of view and near plane).
    struct Candidate {
        Camera camera;
        u32 draws = 0;
    };
    std::array<Candidate, 4> candidates{};
    u32 candidate_count = 0;
    std::array<float, 2> jitter{}, previous_jitter{};
    std::array<u32, 2> render_size{};
    VideoCore::ImageId depth_id{};
    VideoCore::ImageId last_depth_id{}; ///< the previous frame's scene depth
    VideoCore::ImageId velocity_id{};

    vk::UniqueDescriptorSetLayout desc_layout;
    vk::UniquePipelineLayout pipeline_layout;
    vk::UniquePipeline overlay_pipeline;
    /// Debug: the last two finished frames (RGBA8 packed), for the reprojection check.
    std::unique_ptr<VideoCore::Buffer> frames[2];
    u32 frame_index = 0;
};

} // namespace Vulkan
