// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <bit>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unordered_set>

namespace Vulkan {

/// gr2 (Killzone): GR2_RES_CENSUS=1 with a render resolution patch looks for what still
/// assumes 1080p (the source of lighting flicker and shadows that trail the camera above
/// 1080p): viewports and scissors of the 1080p family (1920x1080, 960x540, ...) on targets of
/// another size, compute dispatches sized for 1920/1080, and 1080p-family values (1920, 1080,
/// 960, 540, ... and their reciprocals) in the shaders' constant buffers and user data. Each
/// case is logged once ("Census: ..."). Used on the GPU command thread only.
class ResCensus {
public:
    ResCensus() {
        const char* on = std::getenv("GR2_RES_CENSUS");
        const char* patches = std::getenv("GR2_PATCHES");
        if (!on || on[0] != '1' || !patches) {
            return;
        }
        static constexpr struct {
            const char* name;
            std::uint32_t w, h;
        } sizes[] = {{"Resolution Patch - 720p", 1280, 720},
                     {"Resolution Patch - 1440p", 2560, 1440},
                     {"Resolution Patch - 1800p", 3200, 1800},
                     {"Resolution Patch - 4K", 3840, 2160}};
        for (const auto& s : sizes) {
            if (std::strstr(patches, s.name)) {
                render_w = s.w;
                render_h = s.h;
            }
        }
        if (render_w) {
            std::printf("Census: render %ux%u, looking for 1080p leftovers\n", render_w, render_h);
            std::fflush(stdout);
        }
    }

    [[nodiscard]] bool On() const noexcept {
        return render_w != 0;
    }

    /// 1920>>k x 1080>>k: k, else -1.
    static int NativePair(std::uint32_t w, std::uint32_t h) {
        for (int k = 0; k < 5; ++k) {
            if (w == (1920u >> k) && h == (1080u >> k)) {
                return k;
            }
        }
        return -1;
    }

    /// A draw's viewport or scissor (`what`) of `w` x `h` on a target of `tw` x `th`.
    void Extent(const char* what, std::uint64_t vs, std::uint64_t ps, std::uint64_t target,
                std::uint32_t tw, std::uint32_t th, std::uint32_t w, std::uint32_t h) {
        if (NativePair(w, h) < 0 || (tw == w && th == h) || !First(Key(vs, ps, w, tw, what[0]))) {
            return;
        }
        std::printf("Census: %s %ux%u on target %#llx %ux%u (vs %016llx ps %016llx)\n", what, w, h,
                    static_cast<unsigned long long>(target), tw, th,
                    static_cast<unsigned long long>(vs), static_cast<unsigned long long>(ps));
        std::fflush(stdout);
    }

    /// A draw into a target still sized for 1080p (1920x1080, 960x540, ...).
    void Target(std::uint64_t target, std::uint32_t tw, std::uint32_t th, bool color,
                std::uint64_t vs, std::uint64_t ps) {
        // Heights may be padded to the tile size (1080 -> 1088).
        int k = -1;
        for (int i = 0; i < 5 && k < 0; ++i) {
            if (tw == (1920u >> i) && th >= (1080u >> i) && th < (1080u >> i) + 16) {
                k = i;
            }
        }
        if (k < 0 || !First(Key(target, tw, th, 't'))) {
            return;
        }
        std::printf("Census: 1080p-sized %s target %#llx %ux%u (vs %016llx ps %016llx)\n",
                    color ? "color" : "depth", static_cast<unsigned long long>(target), tw, th,
                    static_cast<unsigned long long>(vs), static_cast<unsigned long long>(ps));
        std::fflush(stdout);
    }

    /// The game's UI viewport scale (eboot 0x3CE0060: {width / 1920, 0, height / 1080, 0},
    /// written by 0xe4a820), logged when it changes.
    void UiScale(const float* scale) {
        if (scale[0] == last_scale[0] && scale[2] == last_scale[1]) {
            return;
        }
        last_scale[0] = scale[0];
        last_scale[1] = scale[2];
        if (lines < 4000) {
            ++lines;
            std::printf("Census: UI viewport scale %.4f x %.4f\n", scale[0], scale[2]);
            std::fflush(stdout);
        }
    }

    /// A compute dispatch of `x` x `y` threads.
    void Dispatch(std::uint64_t cs, std::uint32_t groups_x, std::uint32_t groups_y,
                  std::uint32_t tx, std::uint32_t ty) {
        const std::uint32_t x = groups_x * tx, y = groups_y * ty;
        bool hit = false;
        for (int k = 0; k < 5 && !hit; ++k) {
            const std::uint32_t nw = 1920u >> k, nh = 1080u >> k;
            hit = (x >= nw && x < nw + tx) || (y >= nh && y < nh + ty && y > 16);
        }
        if (!hit || !First(Key(cs, groups_x, groups_y, tx, 'c'))) {
            return;
        }
        std::printf("Census: dispatch cs %016llx %ux%u groups of %ux%u = %ux%u threads\n",
                    static_cast<unsigned long long>(cs), groups_x, groups_y, tx, ty, x, y);
        std::fflush(stdout);
    }

    /// Scans `count` dwords of a shader's constants (`where`: "cb2", "ud") for 1080p values.
    void Constants(const char* stage, std::uint64_t hash, const char* where,
                   const std::uint32_t* data, std::size_t count) {
        for (std::size_t i = 0; i < count; ++i) {
            const char* name = Value(data[i]);
            if (!name) {
                continue;
            }
            // A neighbouring partner (1920 next to 1080...) makes it a screen size for sure.
            const char* next = i + 1 < count ? Value(data[i + 1]) : nullptr;
            if (!First(Key(hash, i, data[i], std::strlen(where), where[0] + where[2]))) {
                continue;
            }
            std::printf("Census: const %s %016llx %s +%#zx = %s%s%s\n", stage,
                        static_cast<unsigned long long>(hash), where, i * 4, name,
                        next ? ", next " : "", next ? next : "");
            std::fflush(stdout);
        }
    }

private:
    static const char* Value(std::uint32_t bits) {
        static const struct Entry {
            std::uint32_t bits;
            const char* name;
        } table[] = {
            {std::bit_cast<std::uint32_t>(1920.0f), "1920.0"},
            {std::bit_cast<std::uint32_t>(1080.0f), "1080.0"},
            {std::bit_cast<std::uint32_t>(960.0f), "960.0"},
            {std::bit_cast<std::uint32_t>(540.0f), "540.0"},
            {std::bit_cast<std::uint32_t>(480.0f), "480.0"},
            {std::bit_cast<std::uint32_t>(270.0f), "270.0"},
            {std::bit_cast<std::uint32_t>(float(1.0 / 1920)), "1/1920"},
            {std::bit_cast<std::uint32_t>(float(1.0 / 1080)), "1/1080"},
            {std::bit_cast<std::uint32_t>(float(1.0 / 960)), "1/960"},
            {std::bit_cast<std::uint32_t>(float(1.0 / 540)), "1/540"},
            {std::bit_cast<std::uint32_t>(float(1.0 / 480)), "1/480"},
            {std::bit_cast<std::uint32_t>(float(1.0 / 270)), "1/270"},
            {std::bit_cast<std::uint32_t>(float(2.0 / 1920)), "2/1920"},
            {std::bit_cast<std::uint32_t>(float(2.0 / 1080)), "2/1080"},
        };
        for (const auto& e : table) {
            if (bits == e.bits) {
                return e.name;
            }
        }
        // Reciprocals computed on the GPU side may differ in the last bit.
        const float f = std::bit_cast<float>(bits);
        for (const double d : {1920.0, 1080.0, 960.0, 540.0}) {
            if (std::isfinite(f) && f > 0 && std::fabs(f * d - 1.0) < 2e-6) {
                return d == 1920.0 ? "~1/1920" : d == 1080.0 ? "~1/1080" : d == 960.0 ? "~1/960" : "~1/540";
            }
        }
        return nullptr;
    }

    template <typename... T>
    static std::uint64_t Key(T... parts) {
        std::uint64_t h = 1469598103934665603ull;
        ((h = (h ^ static_cast<std::uint64_t>(parts)) * 1099511628211ull), ...);
        return h;
    }
    bool First(std::uint64_t key) {
        return lines < 4000 && seen.insert(key).second && ++lines;
    }

    std::uint32_t render_w = 0, render_h = 0;
    float last_scale[2] = {-1.0f, -1.0f};
    std::unordered_set<std::uint64_t> seen;
    unsigned lines = 0;
};

} // namespace Vulkan
