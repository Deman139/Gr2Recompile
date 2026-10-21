// gow3: where the guest image (eboot and the modules loaded after it) starts, so faulting
// instructions can be named by guest offset (GOW3_FAULT_PROFILE, GOW3_NOWAIT_WRITES).
#pragma once

#include <atomic>
#include <cstdint>

namespace Gow3GuestImage {

inline std::atomic<std::uintptr_t> base{0};
/// The eboot plus every module the game loads sits within this span from the base.
constexpr std::uintptr_t Span = 0x20000000;

/// Guest offset of a host address, or ~0 outside the guest image.
inline std::uintptr_t Offset(std::uintptr_t address) {
    const std::uintptr_t b = base.load(std::memory_order_relaxed);
    return b && address - b < Span ? address - b : ~std::uintptr_t{0};
}

} // namespace Gow3GuestImage
