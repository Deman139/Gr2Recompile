// SPDX-License-Identifier: GPL-2.0-or-later
#include "gr2_actors.h"
#include "gr2_settings.h"
#include <cstdint>
#include "../../src/actor_hook.h"

alignas(64) Gr2ActorSlot gr2_actor_slots[2];

extern "C" uintptr_t gr2_actor_slots_address(void) {
    return reinterpret_cast<uintptr_t>(gr2_actor_slots);
}

extern "C" void gr2_actor_status(int supported) {
    Gr2Settings::Get().actor_watch_supported = supported != 0;
}
