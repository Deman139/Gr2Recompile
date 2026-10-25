// gr2: address-space operations the video core needs, backed by the C
// runtime's VMA table (runtime_memory.c).
#pragma once
#include <boost/icl/interval_set.hpp>
#include "common/enum.h"
#include "common/types.h"

namespace Core {
enum class MemoryPermission : u32 {
    None = 0,
    Read = 1 << 0,
    Write = 1 << 1,
    ReadWrite = Read | Write,
    Execute = 1 << 2,
    ReadWriteExecute = Read | Write | Execute,
};
DECLARE_ENUM_FLAG_OPERATORS(MemoryPermission)

class AddressSpace {
public:
    void Protect(VAddr virtual_addr, u64 size, MemoryPermission perms);
    boost::icl::interval_set<VAddr> GetUsableRegions();
    // The last 4 GiB of the guest range the runtime reserves ([64 GiB, 0xfc00000000)): the game
    // maps fixed addresses here (Killzone maps the tessellation factor ring at base + size -
    // 256 MiB), so they must be guest addresses; ordinary mappings fill the range from the bottom.
    VAddr SystemReservedVirtualBase() const { return 0xfb'0000'0000ULL; }
    u64 SystemReservedVirtualSize() const { return 0x1'0000'0000ULL; }
};
} // namespace Core
