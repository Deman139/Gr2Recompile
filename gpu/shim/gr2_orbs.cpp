// SPDX-License-Identifier: GPL-2.0-or-later
#include "gr2_orbs.h"
#include "gr2_settings.h"
#include <cstring>
#include <cstdint>
#include "../../src/cheat_hook.h"

extern "C" float __attribute__((sysv_abi)) gr2_orbs_gain(void* player, float requested) {
    float balance;
    std::memcpy(&balance, static_cast<unsigned char*>(player) + 0x1e8, sizeof(balance));
    const auto& settings = Gr2Settings::Get();
    if ((settings.cheats_supported.load() & (1u << GR2_CHEAT_ORBS)) &&
        settings.cheats[GR2_CHEAT_ORBS].load()) {
        return requested;
    }
    return Gr2Orbs::Scale(balance, requested, Gr2Settings::Get().red_orb_multiplier.load());
}

extern "C" uintptr_t gr2_cheat_flag(unsigned id) {
    static_assert(sizeof(std::atomic<bool>) == 1 && std::atomic<bool>::is_always_lock_free);
    return id < GR2_CHEAT_COUNT ? reinterpret_cast<uintptr_t>(&Gr2Settings::Get().cheats[id]) : 0;
}

extern "C" void gr2_cheat_status(unsigned id) {
    if (id < GR2_CHEAT_COUNT) {
        Gr2Settings::Get().cheats_supported.fetch_or(1u << id);
    }
}

extern "C" void gr2_orbs_status(int supported) {
    Gr2Settings::Get().red_orbs_supported = supported != 0;
}

/// Damage (src/damage_hook.h): the multiplier the player's (taken) or another actor's hits use.
extern "C" uintptr_t gr2_damage_multiplier(int taken) {
    static_assert(sizeof(std::atomic<float>) == 4 && std::atomic<float>::is_always_lock_free);
    auto& s = Gr2Settings::Get();
    return reinterpret_cast<uintptr_t>(taken ? &s.damage_taken : &s.damage_dealt);
}

/// Orb pickup (src/orb_hook.h): 0 green, 1 blue, 2 gold.
extern "C" uintptr_t gr2_orb_pickup_multiplier(unsigned kind) {
    auto& s = Gr2Settings::Get();
    return reinterpret_cast<uintptr_t>(kind == 0 ? &s.green_orb_multiplier
                                       : kind == 1 ? &s.blue_orb_multiplier
                                                   : &s.gold_orb_multiplier);
}

extern "C" void gr2_stat_multipliers_status(int damage, int orb_pickup) {
    Gr2Settings::Get().damage_supported = damage != 0;
    Gr2Settings::Get().orb_pickup_supported = orb_pickup != 0;
}
