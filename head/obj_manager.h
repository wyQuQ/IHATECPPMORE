#pragma once

#include <memory>
#include <vector>
#include <type_traits>
#include <unordered_set>
#include <unordered_map>
#include <cstdint>

#include "object_token.h"

// 前置声明，避免头文件循环依赖
class BaseObject;

// ObjManager 为应用提供对象生命周期管理与句柄（token）系统，面向使用者说明：
// - 提供基于 `ObjToken` 的对象引用与验证机制，避免裸指针悬挂问题。
// - 支持立即创建（CreateImmediate/CreateInit）与延迟创建（CreateDelayed），
//   延迟创建会在下一次 UpdateAll 时实际将对象合并到内部容器并注册到物理系统。
// - 支持立即销毁（DestroyImmediate）与延迟销毁（DestroyDelayed），延迟销毁会在 UpdateAll 中执行，
//   保证在遍历/回调过程中安全移除对象。
// - 提供 UpdateAll() 作为统一帧更新入口：负责逐对象的 FramelyUpdate、处理物理系统 Step、执行延迟创建/销毁。
// - 为外部系统（例如 PhysicsSystem）提供注册/反注册的 token 生命周期管理帮助。
class ObjManager {
public:
    static ObjManager& Instance() noexcept;

    ObjManager(const ObjManager&) = delete;
    ObjManager& operator=(const ObjManager&) = delete;

    using ObjToken = ::ObjToken;

    // Create: 立即构造对象并调用 Start()，但对象合并到内部容器并生成真实 ObjToken 会在下一帧 UpdateAll 的提交阶段完成。
    // 返回 PendingToken 以便调用方追踪 pending 对象，并在合并后通过 ResolvePending 获取真实 ObjToken。
    template <typename T, typename... Args>
    ObjToken Create(Args&&... args)
    {
        static_assert(std::is_base_of<BaseObject, T>::value, "T must derive from BaseObject");
        return Create<T>([](T*) {}, std::forward<Args>(args)...);
    }

    // Create: 允许在构造后、Start() 前对对象进行初始化的版本，返回 PendingToken。
    template <typename T, typename Init, typename... Args>
    ObjToken Create(Init&& initializer, Args&&... args)
    {
        static_assert(std::is_base_of<BaseObject, T>::value, "T must derive from BaseObject");
        static_assert(std::is_invocable_v<Init, T*>, "initializer must be callable with T*");
        auto obj = std::make_unique<T>(std::forward<Args>(args)...);
        if (initializer) {
            std::forward<Init>(initializer)(static_cast<T*>(obj.get()));
        }
        return CreateEntry(std::move(obj));
    }

    bool IsValid(const ObjToken& token) const noexcept;

    // operator[] 重载：通过 ObjToken 直接取得对象的左值引用。
    // 语义：若 token 无效或对象已被销毁，会抛出 std::out_of_range（并写入 std::cerr）。
    BaseObject& operator[](ObjToken& token);
    BaseObject& operator[](const ObjToken& token);
    const BaseObject& operator[](ObjToken& token) const;
    const BaseObject& operator[](const ObjToken& token) const;

    // 支持以 PendingToken 直接销毁（若已合并则转为销毁真实 ObjToken，否则销毁 pending）
    void Destroy(const ObjToken& p) noexcept;

    // DestroyAll: 立即销毁所有对象并清理所有挂起队列，通常在程序退出或重置时调用。
    void DestroyAll() noexcept;

    // UpdateAll: 每帧主更新入口，顺序：
    // 1) 为每个活跃对象调用 FramelyUpdate()
    // 2) 调用 PhysicsSystem::Step()
    // 3) 执行所有延迟销毁
    // 4) 提交本帧 pending 创建：在安全点把 pending_creates_ 合并到 objects_ 并注册物理系统，使其在下一帧参与更新
    void UpdateAll() noexcept;

    size_t Count() const noexcept { return alive_count_; }

private:
    ObjManager() noexcept;
    ~ObjManager() noexcept;

    struct Entry {
        std::unique_ptr<BaseObject> ptr;
        uint32_t generation = 0;
        bool alive = false;
        // 新增：创建当帧跳过 FramelyUpdate 的标志
        bool skip_update_this_frame = false;
    };

    // pending create 的中间结构：在 CreateEntry 时只把对象放到这里（不直接扩展 objects_），
    // 在 UpdateAll 的提交阶段再把它们合并到 objects_（安全点，避免在更新循环中重分配）
    struct PendingCreate {
        std::unique_ptr<BaseObject> ptr;
    };

    // 为延迟创建保留 slot，并返回可用索引。
    uint32_t ReserveSlotForCreate() noexcept;

    // 内部立即销毁实现（按 index）。
    void DestroyEntry(uint32_t index) noexcept;
    // DestroyPending: 如果 pending 尚未合并，可直接销毁 pending 对象
    void DestroyPending(const ObjToken& p) noexcept;
    // DestroyExisting: 将销毁请求入队（对于已合并对象），实际删除在下一次 UpdateAll 时执行；对于 pending 对象请使用 DestroyPending。
    void DestroyExisting(const ObjToken& token) noexcept;

    // 将 unique_ptr<BaseObject> 的对象纳入管理并在必要时调用 Start()，返回 PendingToken 表示创建请求。
    // 对象会被放入 pending_creates_（带 id），在 UpdateAll 的提交阶段合并到 objects_ 并完成物理注册。
    ObjToken CreateEntry(std::unique_ptr<BaseObject> obj);

	ObjToken check_pending_to_real(const ObjToken& p) const noexcept;

    // 存储对象条目
    std::vector<Entry> objects_;

    // 空闲索引池，用于重用 slots
    std::vector<uint32_t> free_indices_;

    // BaseObject* -> index 的映射，用于快速查找（仅包含已经合并到 objects_ 的对象）
    std::unordered_map<BaseObject*, uint32_t> object_index_map_;

    // 延迟销毁队列与去重集合（防止重复入队）
    std::vector<ObjToken> pending_destroys_;
    std::unordered_set<uint64_t> pending_destroy_set_; // compact key: ((uint64_t)index<<32)|generation

    // 本帧刚创建但尚未合并到 objects_ 的对象存储（pending 创建区）
    std::unordered_map<uint32_t, PendingCreate> pending_creates_; // key = pending id
    std::unordered_map<BaseObject*, uint32_t> pending_ptr_to_id_; // ptr -> pending id 快速查找

    // pending id -> 已合并后的真实 ObjToken（合并完成后写入，便于 ResolvePending）
    std::unordered_map<uint32_t, ObjToken> pending_to_real_map_;

    // 下一个 pending id（单调递增）
    uint32_t next_pending_id_ = 1;

    // 当前存活对象计数（包含 pending 创建）
    size_t alive_count_ = 0;
};