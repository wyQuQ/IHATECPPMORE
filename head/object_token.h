#pragma once

#include <cstdint>
#include <limits>

// ObjToken 是对托管对象的轻量句柄（handle）类型，面向使用者说明：
// - 使用 (index, generation) 的组合来安全引用 ObjManager 管理的对象，避免裸指针悬挂问题。
// - 当对象槽被回收并重用时，generation 会递增以使旧的 token 失效；这比裸指针更安全，但仍然假设在同一进程空间内使用。
// - token.index 在 pending 状态下被用于存储 pending id（ObjManager 的约定），因此在使用前可能需通过 ObjManager::TryGetRegisteration 将 pending 转换为真实 token。
// 字段说明：
// - index: 实际槽索引或 pending id（pending 时由 ObjManager 约定）
// - generation: 由 ObjManager 管理，每次槽回收时递增以使旧 token 失效
// - isRegitsered: 表示该 token 是否已经为“注册/真实” token（为 true 时 index/generation 指向 objects_ 中的条目）
// 使用建议：
// - 在跨帧保存 token 是安全的；使用前调用 ObjManager::IsValid 或 TryGetRegisteration 以确认 token 仍有效。
// - 不要直接比较 token.index 以推断对象生命周期，要使用 operator==/!= 或通过 ObjManager 提供的接口。
struct ObjToken {
    uint32_t index = std::numeric_limits<uint32_t>::max();
    uint32_t generation = 0;
	bool isRegitsered = false;
	bool isValid() const noexcept { return (index != std::numeric_limits<uint32_t>::max()); }
    bool operator==(const ObjToken& o) const noexcept { return index == o.index && generation == o.generation; }
    bool operator!=(const ObjToken& o) const noexcept { return !(*this == o); }
    static constexpr ObjToken Invalid() noexcept { return { std::numeric_limits<uint32_t>::max(), 0, false }; }
};