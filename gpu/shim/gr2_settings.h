// SPDX-License-Identifier: GPL-2.0-or-later
// gr2: user settings changed at run time from the in-game menu (gr2_overlay.h) and kept
// in gr2.ini (GR2_CONFIG overrides the path). Environment variables override the file at
// start. Readers load the atomics every frame; writers are the menu and Load().

#pragma once

#include "../../src/cheat_hook.h"

#include <atomic>
#include <cstdint>

namespace Gr2Settings {

enum Upscaler : int { UpscalerOff = 0, UpscalerFsr3 = 1, UpscalerFsr4 = 2, UpscalerFsr411 = 3,
                      UpscalerTaa = 4, UpscalerDlss = 5, UpscalerCount };
/// FSR 4 v07 or FSR 4.1.1: the same inputs, settings and placement in the frame.
inline bool IsFsr4(int upscaler) {
    return upscaler == UpscalerFsr4 || upscaler == UpscalerFsr411;
}
enum Preset : int { NativeAA = 0, Quality, Balanced, Performance, UltraPerformance, PresetCount };
/// Frame generation (vk_frame_generation.h): frames generated between two of the game's.
enum FrameGen : int { FrameGenOff = 0, FrameGenFsr3 = 1, FrameGenDlss = 2, FrameGenCount };
enum DebugView : int { DebugNone = 0, DebugReactive = 1, DebugMotion = 2, DebugViewCount };

/// Live output resolutions: the upscaler's output and the UI host targets.
inline constexpr int OutputWidths[] = {1280, 1920, 2560, 3840, 3200};
inline constexpr int OutputHeights[] = {720, 1080, 1440, 2160, 1800};
inline constexpr int OutputCount = 5;
inline constexpr int OutputDefault = 1; ///< 1920x1080, the game's own size

struct Values {
    std::atomic<int> upscaler{UpscalerFsr3};
    std::atomic<int> preset{NativeAA};
    std::atomic<bool> sharpen{true};
    std::atomic<float> sharpness{0.3f};
    std::atomic<bool> jitter{true};
    std::atomic<bool> reactive{false};
    std::atomic<bool> object_motion{true};
    std::atomic<float> reactive_scale{1.0f};
    std::atomic<float> reactive_threshold{0.2f};
    std::atomic<float> reactive_max{0.9f};
    std::atomic<int> debug_view{DebugNone};
    std::atomic<bool> show_fps{false};
    /// Performance overlay (gr2_perf_stats.h): items shown, font size and background opacity
    /// in percent, corner (0 top left, 1 top right, 2 bottom left, 3 bottom right), layout
    /// (0 one line, 1 one item per line). menu_tab: the menu's last tab.
    std::atomic<unsigned> overlay_items{7};
    std::atomic<int> overlay_scale{100}, overlay_corner{1}, overlay_opacity{50}, overlay_layout{0};
    std::atomic<int> menu_tab{0};
    std::atomic<float> red_orb_multiplier{1.0f};
    std::atomic<bool> red_orbs_supported{false};
    /// Damage and orb pickup multipliers (src/damage_hook.h, src/orb_hook.h), 0.1x to 100x.
    /// The game's code reads these floats directly.
    std::atomic<float> damage_dealt{1.0f}, damage_taken{1.0f};
    std::atomic<float> green_orb_multiplier{1.0f}, blue_orb_multiplier{1.0f}, gold_orb_multiplier{1.0f};
    std::atomic<bool> damage_supported{false}, orb_pickup_supported{false};
    std::atomic<bool> cheats[GR2_CHEAT_COUNT]{};
    std::atomic<uint32_t> cheats_supported{0};
    /// Damage hook (src/actor_hook.h): enemy health bar, off by default.
    std::atomic<bool> enemy_health_bar{false};
    std::atomic<bool> actor_watch_supported{false};
    // Graphics (gr2_graphics.h). Live: applied on the next frame.
    std::atomic<int> display_mode{0};
    std::atomic<bool> vsync{true};
    std::atomic<int> fps_limit{0};
    std::atomic<bool> async_shaders{true};
    std::atomic<bool> fsr1{false};
    std::atomic<bool> rcas{true};
    std::atomic<int> rcas_strength{75};
    /// Set by the presenter: sharpening ran on the last frame (it needs FSR 1 when the game's
    /// image is smaller than the window).
    std::atomic<bool> rcas_applied{false};
    /// Set by the presenter: the game's image and the window it is shown in, in pixels.
    std::atomic<int> image_width{0}, image_height{0}, window_width{0}, window_height{0};
    std::atomic<int> frames_queued{1};
    /// Controller vibration strength in percent; 0 turns it off (runtime_pad.c). Live.
    std::atomic<int> vibration{100};
    /// gr2 (iss_pc mip_lod_bias): extra texture LOD bias of every draw, -3..3 in steps of 1/8:
    /// negative is sharper at a distance (more shimmer), positive blurrier (a little less GPU).
    std::atomic<float> mip_lod_bias{0.0f};
    /// Killzone controls, live (in-game menu Controls tab and the launcher; runtime_pad.c and
    /// runtime_audio.c read them through gr2gpu_input_settings / gr2gpu_pad_speaker_percent).
    std::atomic<float> mouse_sens{1.0f};      ///< mouse look sensitivity, 0.2..4
    std::atomic<int> mouse_turn_boost{6};     ///< "fast turns": turn speed factor while the mouse moves, 1..12
    std::atomic<bool> mouse_invert{false};
    std::atomic<int> fov{100};                ///< horizontal FOV percent, 80..150 (src/probe.c FOV hook)
    std::atomic<int> pad_dead_zone{0};        ///< extra stick dead zone percent, 0..30
    std::atomic<int> pad_speaker{100};        ///< controller speaker sounds percent
    std::atomic<int> prompt_style{0};         ///< button prompts: 0 auto, 1 keyboard/mouse, 2 PS4
    std::atomic<bool> quick_menu{true};       ///< hold Select/Back/Share: quick menu (Resume / Exit)
    std::atomic<bool> quick_menu_hint{true};  ///< startup reminder of that shortcut
    /// Set when the in-game menu changed the upscaler or frame generation: the launcher adopts
    /// gr2.ini's upscaler/frame_gen into its own settings next time and clears it.
    std::atomic<bool> upscaling_from_menu{false};
    /// Killzone post-processing (src/postfx_hook.h): the game's own off path of each pass, set
    /// in the image before it starts (startup_* are what this run applied). Camera shake is live.
    std::atomic<bool> motion_blur{true}, depth_of_field{true}, bloom{true}, film_grain{true};
    bool startup_motion_blur = true, startup_depth_of_field = true, startup_bloom = true,
         startup_film_grain = true;
    std::atomic<bool> camera_shake{true};
    int startup_frame_gen = 0;
    // Startup: run.py turns them into patches and environment variables; "Apply and restart".
    std::atomic<int> render_resolution{0};
    std::atomic<int> engine_fps{120};
    std::atomic<bool> deferred_readback{true};
    std::atomic<bool> stale_readback{true};
    /// Gr2Graphics::Anisotropies; the samplers read startup_anisotropy.
    std::atomic<int> anisotropy{0};
    int startup_anisotropy = 0;
    /// Set before an "Apply and restart"; cleared once the new launch shows the game. run.py
    /// restores the previous startup options when a launch never cleared it.
    std::atomic<bool> restart_unconfirmed{false};
    int startup_render_resolution = 0, startup_engine_fps = 120;
    /// The engine rate this launch really runs at (run.py's GR2_ENGINE_FPS): 60 when the frame
    /// rate patch does not apply to this game build.
    std::atomic<int> applied_engine_fps{120};
    bool startup_deferred_readback = true, startup_stale_readback = true;
    // FSR 4 checks (menu): the provider's auto exposure, the jitter sign it is given.
    std::atomic<bool> fsr4_auto_exposure{true};
    std::atomic<bool> fsr4_invert_jitter{false};
    std::atomic<int> active_render_width{1920}, active_render_height{1080};
    std::atomic<int> output_res{OutputDefault}; ///< index into OutputWidths
    /// Live resolution and preset changes (run.sh): 0 off by default (startup patch, fastest
    /// on the Steam Deck and older GPUs), -1 auto (strong discrete GPUs), 1 on. On restart.
    std::atomic<int> live_resolution{0};
    /// Why FSR 4 cannot run (assets, device features), or null. Set by the renderer.
    std::atomic<const char*> fsr4_problem{nullptr};
    std::atomic<bool> fsr4_supported{false}, fsr411_supported{false};
    /// DLSS (gr2_dlss.dll, NVIDIA RTX) is ready, or why not (null before the device exists).
    std::atomic<bool> dlss_supported{false};
    std::atomic<const char*> dlss_problem{nullptr};
    /// Frame generation (GR2_FRAME_GEN off|fsr3|dlss): frames per game frame in all (2 with
    /// FSR 3.1, up to 4 with DLSS on RTX 50). dlssg_max_multiplier: 1 without DLSS Frame Generation.
    std::atomic<int> frame_gen{FrameGenOff};
    std::atomic<int> frame_gen_multiplier{2};
    std::atomic<int> dlssg_max_multiplier{1};
    std::atomic<const char*> frame_gen_problem{nullptr};

    /// Startup settings for the explicit GR2_RENDER_RES compatibility patch only.
    int startup_preset = NativeAA;
    int startup_upscaler = UpscalerFsr3;
    bool startup_object_motion = true;
    int startup_output_res = OutputDefault;
    int startup_live_resolution = 0;
};

Values& Get();

/// Reads the file, then the environment overrides. Called once at start.
void Load();
/// Checks the loaded choice before the first frame; unsupported FSR 4 uses FSR 3.1.
void ConfigureUpscalerSupport(bool fsr4, bool fsr411);
/// After device creation: DLSS availability; a DLSS setting falls back to FSR 3.1 without it.
void ConfigureDlssSupport(bool available, const char* problem);
/// Startup-patched scene dimensions cannot change until run.sh prepares a new image.
bool FixedRenderSession();
int RenderPreset();
bool ResolutionNeedsRestart();
/// A startup graphics option differs from the one this launch started with.
bool GraphicsNeedRestart();
/// Killzone post-processing switches changed since start (they apply on restart).
bool EffectsNeedRestart();
/// Writes the file (menu changes).
void Save();

/// Render resolution divisor of a preset (1.0 native, 1.5 quality, ...).
float PresetScale(int preset);
const char* PresetName(int preset);
const char* UpscalerName(int upscaler);
const char* FrameGenName(int frame_gen);

} // namespace Gr2Settings
