#pragma once

#include <cstdint>
#include <limits>

struct ObjToken {
    uint32_t index = std::numeric_limits<uint32_t>::max();
    uint32_t generation = 0;
    bool operator==(const ObjToken& o) const noexcept { return index == o.index && generation == o.generation; }
    bool operator!=(const ObjToken& o) const noexcept { return !(*this == o); }
    static constexpr ObjToken Invalid() noexcept { return { std::numeric_limits<uint32_t>::max(), 0 }; }
};