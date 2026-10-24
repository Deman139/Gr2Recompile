// SPDX-License-Identifier: GPL-2.0-or-later
// Gravity Rush 2: DLAA in place of the game's anti-aliasing pass (vk_gr2_dlaa.h).
#include "video_core/renderer_vulkan/vk_gr2_dlaa.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

#include <vk_mem_alloc.h>

#include "video_core/host_shaders/gr2_dlaa_motion_comp.h"
#include "video_core/renderer_vulkan/vk_dlss.h"
#include "video_core/renderer_vulkan/vk_instance.h"
#include "video_core/renderer_vulkan/vk_platform.h"
#include "video_core/renderer_vulkan/vk_runtime.h"
#include "video_core/renderer_vulkan/vk_scheduler.h"
#include "video_core/renderer_vulkan/vk_shader_util.h"
#include "video_core/texture_cache/texture_cache.h"

namespace Vulkan {

namespace {

struct MotionPush {
    float scale[2];
    s32 size[2];
};

float Halton(u32 index, u32 base) {
    float f = 1.0f, result = 0.0f;
    for (u32 i = index; i > 0; i /= base) {
        f /= float(base);
        result += f * float(i % base);
    }
    return result;
}

const char* Env(const char* name) {
    const char* value = std::getenv(name);
    return value && *value ? value : nullptr;
}

} // namespace

Gr2Dlaa::Gr2Dlaa(const Instance& instance_, Scheduler& scheduler_,
                 VideoCore::TextureCache& texture_cache_, Runtime& runtime_)
    : instance{instance_}, scheduler{scheduler_}, texture_cache{texture_cache_},
      runtime{runtime_} {
    const char* on = Env("GOW3_DLAA");
    enabled = on && on[0] == '1';
    if (!enabled) {
        return;
    }
    if (const char* hash = Env("GOW3_DLAA_CS")) {
        taa_hash = std::strtoull(hash, nullptr, 16);
    }
    if (const char* value = Env("GOW3_DLAA_JITTER")) {
        use_jitter = value[0] != '0';
    }
    if (const char* value = Env("GOW3_DLAA_JITTER_SIGN")) {
        jitter_sign = std::atof(value) < 0.0f ? -1.0f : 1.0f;
    }
    if (const char* value = Env("GOW3_DLAA_PHASES")) {
        phases = std::clamp(std::atoi(value), 1, 64);
    }
    if (const char* value = Env("GOW3_DLSS_PRESET")) {
        preset = u32(std::max(std::atoi(value), 0));
    }
    if (const char* value = Env("GOW3_DLAA_SHARPNESS")) {
        sharpness = std::clamp(float(std::atof(value)), 0.0f, 1.0f);
    }
    std::printf("DLAA: requested (game AA cs %08llx, jitter %s, %u phases, preset %u)\n",
                static_cast<unsigned long long>(taa_hash), use_jitter ? "on" : "off", phases,
                preset);
}

Gr2Dlaa::~Gr2Dlaa() {
    const auto device = instance.GetDevice();
    for (auto& entry : views) {
        if (entry.view) {
            device.destroyImageView(entry.view);
        }
    }
}

vk::ImageView Gr2Dlaa::View(const VideoCore::Image& image, vk::Format format,
                            vk::ImageAspectFlags aspect) {
    const vk::Image handle = image.GetImage();
    ViewEntry* oldest = &views[0];
    for (auto& entry : views) {
        if (entry.view && entry.image == handle && entry.uid == image.image_uid &&
            entry.format == format && entry.aspect == aspect) {
            entry.last_use = ++view_uses;
            return entry.view;
        }
        if (entry.last_use < oldest->last_use) {
            oldest = &entry;
        }
    }
    const auto device = instance.GetDevice();
    if (oldest->view) {
        scheduler.DeferOperation([device, view = oldest->view] { device.destroyImageView(view); });
    }
    // The cache's images carry extended usage: restrict the view to what DLSS does with it.
    const bool depth = bool(aspect & (vk::ImageAspectFlagBits::eDepth |
                                      vk::ImageAspectFlagBits::eStencil));
    vk::ImageViewUsageCreateInfo usage{
        .usage = depth ? vk::ImageUsageFlagBits::eSampled
                       : (image.usage_flags & (vk::ImageUsageFlagBits::eSampled |
                                               vk::ImageUsageFlagBits::eStorage)),
    };
    *oldest = {
        .image = handle,
        .uid = image.image_uid,
        .format = format,
        .aspect = aspect,
        .view = Check(device.createImageView({
            .pNext = &usage,
            .image = handle,
            .viewType = vk::ImageViewType::e2D,
            .format = format,
            .subresourceRange = {aspect, 0, 1, 0, 1},
        })),
        .last_use = ++view_uses,
    };
    return oldest->view;
}

bool Gr2Dlaa::EnsureResources(u32 w, u32 h) {
    const auto device = instance.GetDevice();
    if (!pipeline) {
        const std::array<vk::DescriptorSetLayoutBinding, 2> bindings = {{
            {.binding = 0,
             .descriptorType = vk::DescriptorType::eSampledImage,
             .descriptorCount = 1,
             .stageFlags = vk::ShaderStageFlagBits::eCompute},
            {.binding = 1,
             .descriptorType = vk::DescriptorType::eStorageImage,
             .descriptorCount = 1,
             .stageFlags = vk::ShaderStageFlagBits::eCompute},
        }};
        desc_layout = Check(device.createDescriptorSetLayoutUnique({
            .flags = vk::DescriptorSetLayoutCreateFlagBits::ePushDescriptorKHR,
            .bindingCount = static_cast<u32>(bindings.size()),
            .pBindings = bindings.data(),
        }));
        const vk::PushConstantRange range{.stageFlags = vk::ShaderStageFlagBits::eCompute,
                                          .offset = 0,
                                          .size = sizeof(MotionPush)};
        pipeline_layout = Check(device.createPipelineLayoutUnique({
            .setLayoutCount = 1,
            .pSetLayouts = &*desc_layout,
            .pushConstantRangeCount = 1,
            .pPushConstantRanges = &range,
        }));
        const auto module = CompileSPV(GR2_DLAA_MOTION_COMP, device);
        pipeline = Check(device.createComputePipelineUnique(
            {}, vk::ComputePipelineCreateInfo{
                    .stage = {.stage = vk::ShaderStageFlagBits::eCompute,
                              .module = module,
                              .pName = "main"},
                    .layout = *pipeline_layout,
                }));
        device.destroyShaderModule(module);
    }
    if (width == w && height == h && motion_view) {
        return true;
    }
    // The previous motion image may still be read by submitted work.
    scheduler.WaitSubmitted();
    motion_view.reset();
    motion_image = VideoCore::UniqueImage(device, instance.GetAllocator());
    motion_image.Create(vk::ImageCreateInfo{
        .imageType = vk::ImageType::e2D,
        .format = vk::Format::eR16G16Sfloat,
        .extent = {w, h, 1},
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = vk::SampleCountFlagBits::e1,
        .tiling = vk::ImageTiling::eOptimal,
        .usage = vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eSampled,
        .initialLayout = vk::ImageLayout::eUndefined,
    });
    motion_view = Check(device.createImageViewUnique({
        .image = vk::Image(motion_image),
        .viewType = vk::ImageViewType::e2D,
        .format = vk::Format::eR16G16Sfloat,
        .subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1},
    }));
    width = w;
    height = h;
    reset = true;
    return true;
}

void Gr2Dlaa::NextJitter() {
    if (!use_jitter) {
        jitter = {};
        return;
    }
    jitter_index = jitter_index % phases + 1;
    jitter = {Halton(jitter_index, 2) - 0.5f, Halton(jitter_index, 3) - 0.5f};
}

bool Gr2Dlaa::Replace(std::span<const VideoCore::ImageId> images) {
    if (!Enabled()) {
        return false;
    }
    Dlss* dlss = Dlss::Get();
    if (!checked) {
        checked = true;
        if (!dlss || !dlss->Available()) {
            const std::string problem = dlss ? dlss->Problem() : std::string{};
            std::printf("DLAA: DLSS is not available%s%s; the game's own AA stays\n",
                        problem.empty() ? "" : ": ", problem.c_str());
            failed = true;
            return false;
        }
    }

    // The pass's images by format: color and history are RGBA8 sRGB (color first), depth,
    // velocity RG16F, output RGBA8 unorm.
    VideoCore::ImageId color{}, depth{}, velocity{}, output{};
    for (const auto id : images) {
        if (!id) {
            continue;
        }
        const auto& image = texture_cache.GetImage(id);
        const auto format = image.info.pixel_format;
        if (image.info.props.is_depth) {
            if (!depth) depth = id;
        } else if (format == vk::Format::eR16G16Sfloat) {
            if (!velocity) velocity = id;
        } else if (format == vk::Format::eR8G8B8A8Srgb) {
            if (!color) color = id;
        } else if (format == vk::Format::eR8G8B8A8Unorm) {
            output = id;
        }
    }
    static bool logged = false;
    if (!color || !depth || !velocity || !output) {
        if (!std::exchange(logged, true)) {
            std::printf("DLAA: the AA pass's images were not recognised (%zu bound); the game's "
                        "own AA runs\n",
                        images.size());
            for (const auto id : images) {
                if (id) {
                    const auto& image = texture_cache.GetImage(id);
                    std::printf("DLAA:   %#llx %s %ux%u\n",
                                static_cast<unsigned long long>(image.info.guest_address),
                                vk::to_string(image.info.pixel_format).c_str(),
                                image.info.size.width, image.info.size.height);
                }
            }
        }
        active = false;
        return false;
    }
    auto& color_image = texture_cache.GetImage(color);
    auto& depth_image = texture_cache.GetImage(depth);
    auto& velocity_image = texture_cache.GetImage(velocity);
    auto& output_image = texture_cache.GetImage(output);
    const u32 w = output_image.info.size.width, h = output_image.info.size.height;
    if (color_image.info.size.width != w || color_image.info.size.height != h ||
        depth_image.info.size.width != w || depth_image.info.size.height != h ||
        velocity_image.info.size.width != w || velocity_image.info.size.height != h ||
        !(output_image.usage_flags & vk::ImageUsageFlagBits::eStorage)) {
        if (!std::exchange(logged, true)) {
            std::printf("DLAA: the AA pass's images differ in size; the game's own AA runs\n");
        }
        active = false;
        return false;
    }
    if (!std::exchange(frames, frames + 1)) {
        std::printf("DLAA: color %#llx, depth %#llx, velocity %#llx -> %#llx, %ux%u\n",
                    static_cast<unsigned long long>(color_image.info.guest_address),
                    static_cast<unsigned long long>(depth_image.info.guest_address),
                    static_cast<unsigned long long>(velocity_image.info.guest_address),
                    static_cast<unsigned long long>(output_image.info.guest_address), w, h);
    }
    EnsureResources(w, h);

    // New targets (resolution change, level load) or a pause: start the history over.
    const u64 inputs = (u64(color.index) << 32) ^ (u64(depth.index) << 16) ^ output.index;
    if (inputs != last_inputs) {
        last_inputs = inputs;
        reset = true;
    }
    const auto now = std::chrono::steady_clock::now();
    float frame_ms = std::chrono::duration<float, std::milli>(now - last_frame).count();
    if (frame_ms > 250.0f) {
        reset = true;
    }
    if (frame_ms <= 0.0f || frame_ms > 200.0f) {
        frame_ms = 16.6f;
    }
    last_frame = now;

    const auto depth_aspect = vk::ImageAspectFlagBits::eDepth;
    const vk::ImageView color_view =
        View(color_image, vk::Format::eR8G8B8A8Unorm, vk::ImageAspectFlagBits::eColor);
    const vk::ImageView depth_view = View(depth_image, depth_image.info.pixel_format, depth_aspect);
    const vk::ImageView velocity_view =
        View(velocity_image, vk::Format::eR16G16Sfloat, vk::ImageAspectFlagBits::eColor);
    const vk::ImageView output_view =
        View(output_image, vk::Format::eR8G8B8A8Unorm, vk::ImageAspectFlagBits::eColor);

    const auto compute = vk::PipelineStageFlagBits2::eComputeShader;
    runtime.Transit(&color_image, vk::ImageLayout::eGeneral, compute,
                    vk::AccessFlagBits2::eShaderRead);
    runtime.Transit(&depth_image, vk::ImageLayout::eGeneral, compute,
                    vk::AccessFlagBits2::eShaderRead);
    runtime.Transit(&velocity_image, vk::ImageLayout::eGeneral, compute,
                    vk::AccessFlagBits2::eShaderRead);
    runtime.Transit(&output_image, vk::ImageLayout::eGeneral, compute,
                    vk::AccessFlagBits2::eShaderRead | vk::AccessFlagBits2::eShaderWrite);
    runtime.FlushBarriers();
    const auto cmdbuf = scheduler.CommandBuffer();

    const auto all = vk::PipelineStageFlagBits2::eAllCommands;
    const auto motion_barrier = [&](vk::ImageLayout old_layout, vk::PipelineStageFlags2 src_stage,
                                    vk::AccessFlags2 src_access, vk::PipelineStageFlags2 dst_stage,
                                    vk::AccessFlags2 dst_access) {
        const vk::ImageMemoryBarrier2 barrier{
            .srcStageMask = src_stage,
            .srcAccessMask = src_access,
            .dstStageMask = dst_stage,
            .dstAccessMask = dst_access,
            .oldLayout = old_layout,
            .newLayout = vk::ImageLayout::eGeneral,
            .image = vk::Image(motion_image),
            .subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1},
        };
        cmdbuf.pipelineBarrier2({.imageMemoryBarrierCount = 1, .pImageMemoryBarriers = &barrier});
    };
    // The previous frame's DLSS read it: wait for all earlier commands.
    motion_barrier(vk::ImageLayout::eUndefined, all, vk::AccessFlagBits2::eNone, compute,
                   vk::AccessFlagBits2::eShaderWrite);
    const vk::DescriptorImageInfo velocity_info{.imageView = velocity_view,
                                                .imageLayout = vk::ImageLayout::eGeneral};
    const vk::DescriptorImageInfo motion_info{.imageView = *motion_view,
                                              .imageLayout = vk::ImageLayout::eGeneral};
    const std::array<vk::WriteDescriptorSet, 2> writes = {{
        {.dstBinding = 0,
         .descriptorCount = 1,
         .descriptorType = vk::DescriptorType::eSampledImage,
         .pImageInfo = &velocity_info},
        {.dstBinding = 1,
         .descriptorCount = 1,
         .descriptorType = vk::DescriptorType::eStorageImage,
         .pImageInfo = &motion_info},
    }};
    const MotionPush push{{float(w) * 0.5f, -float(h) * 0.5f}, {s32(w), s32(h)}};
    cmdbuf.bindPipeline(vk::PipelineBindPoint::eCompute, *pipeline);
    cmdbuf.pushDescriptorSetKHR(vk::PipelineBindPoint::eCompute, *pipeline_layout, 0, writes);
    cmdbuf.pushConstants(*pipeline_layout, vk::ShaderStageFlagBits::eCompute, 0, sizeof(push),
                         &push);
    cmdbuf.dispatch((w + 7) / 8, (h + 7) / 8, 1);
    motion_barrier(vk::ImageLayout::eGeneral, compute, vk::AccessFlagBits2::eShaderWrite, all,
                   vk::AccessFlagBits2::eShaderRead);

    const Dlss::FeatureDesc desc{
        .input_width = w,
        .input_height = h,
        .output_width = w,
        .output_height = h,
        .quality = 0, // DLAA
        .hdr = false,
        .depth_inverted = false,
        .preset = preset,
    };
    bool ok = true;
    if (!dlss->HasFeature(desc)) {
        // The previous feature may still be in use by submitted work; `cmdbuf` stays open.
        scheduler.WaitSubmitted();
        dlss->ReleaseFeature();
        ok = dlss->CreateFeature(cmdbuf, desc);
        reset = true;
    }
    if (ok) {
        ok = dlss->Evaluate(cmdbuf, {
            .color = {color_image.GetImage(), color_view, vk::Format::eR8G8B8A8Unorm,
                      vk::ImageAspectFlagBits::eColor, w, h},
            .depth = {depth_image.GetImage(), depth_view, depth_image.info.pixel_format,
                      depth_aspect, w, h},
            .motion = {vk::Image(motion_image), *motion_view, vk::Format::eR16G16Sfloat,
                       vk::ImageAspectFlagBits::eColor, w, h},
            .output = {output_image.GetImage(), output_view, vk::Format::eR8G8B8A8Unorm,
                       vk::ImageAspectFlagBits::eColor, w, h},
            .jitter_x = jitter_sign * jitter[0],
            .jitter_y = jitter_sign * jitter[1],
            .reset = reset,
            .frame_ms = frame_ms,
            .sharpness = sharpness,
        });
    }
    if (!ok) {
        std::printf("DLAA: DLSS failed; the game's own AA runs from now on\n");
        failed = true;
        active = false;
        jitter = {};
        // The game's pass still runs this frame: its resources were bound before.
        return false;
    }
    // DLSS ran its own compute shaders on the output: later reads wait for every stage.
    const vk::MemoryBarrier2 done{
        .srcStageMask = all,
        .srcAccessMask = vk::AccessFlagBits2::eShaderWrite | vk::AccessFlagBits2::eShaderRead,
        .dstStageMask = all,
        .dstAccessMask = vk::AccessFlagBits2::eMemoryRead | vk::AccessFlagBits2::eMemoryWrite,
    };
    cmdbuf.pipelineBarrier2({.memoryBarrierCount = 1, .pMemoryBarriers = &done});
    if (frames == 1) {
        std::printf("DLAA: active\n");
    }
    reset = false;
    active = true;
    scene_depth = depth;
    NextJitter();
    return true;
}

} // namespace Vulkan
