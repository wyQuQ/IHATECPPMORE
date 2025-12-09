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
// - 提供基于 `ObjToken` 的对象引用与验证机制，避免裸指针悬挂问题。主流用法：
//     auto tok = objs.Create<MyObject>(...); // 返回 pending token（pending id 存放在 token.index）
//     // 下一帧 UpdateAll 提交后，token 可能被映射为真实 token（index -> objects_ 槽索引），可使用 TryGetRegisteration / operator[] 访问
// - 支持延迟创建（CreateEntry 将对象放入 pending_creates_ 并立即调用 Start()，但直到下一帧 UpdateAll 才合并到 objects_）
//   这样做可以避免在更新循环中动态分配导致迭代器/引用失效的问题。
// - 支持延迟销毁（DestroyExisting 会将真实 token 入队，实际销毁在下一次 UpdateAll 的安全点执行）
// - UpdateAll() 是统一的帧更新入口，职责包括：调用每个对象的 FramelyApply、推进 PhysicsSystem、执行对象 Update、处理 pending create/destroy。
// 语义契约：
// - ObjManager 的大部分接口不是线程安全的，应在主线程的游戏循环中使用。
// - operator[] 在 token 无效时将抛出 std::out_of_range（并写入 std::cerr），调用方应捕获或先使用 IsValid/TryGetRegisteration 检查。
class ObjManager {
public:
    static ObjManager& Instance() noexcept;

    ObjManager(const ObjManager&) = delete;
    ObjManager& operator=(const ObjManager&) = delete;

    using ObjToken = ::ObjToken;

    // Create: 立即构造对象并调用 Start()，但对象合并到内部容器并生成真实 ObjToken 会在下一帧 UpdateAll 的提交阶段完成。
    // 返回 PendingToken 以便调用方追踪 pending 对象，并在合并后通过 ResolvePending/ TryGetRegisteration 获取真实 ObjToken。
    template <typename T, typename... Args>
    ObjToken Create(Args&&... args)
    {
        static_assert(std::is_base_of<BaseObject, T>::value, "T must derive from BaseObject");
        return Create<T>([](T*) {}, std::forward<Args>(args)...);
    }

    // Create: 允许在构造后、Start() 前对对象进行初始化的版本，返回 PendingToken。
    // - initializer 会在对象构造后、Start() 调用前被调用，适合做一些初始状态绑定
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

    // 验证 token 是否为当前有效的已合并对象（不考虑 pending 情况）
    bool IsValid(const ObjToken& token) const noexcept;

    // operator[] 重载：通过 ObjToken 直接取得对象的左值引用。
    // 语义：若 token 无效或对象已被销毁，会抛出 std::out_of_range（并写入 std::cerr）。
    // 注意：如果传入的是 pending token（isRegitsered==false），const/non-const non-const 版本会尝试解析为 pending（访问 pending_creates_）或使用 TryGetRegisteration 升级为真实 token。
    BaseObject& operator[](ObjToken& token);
    BaseObject& operator[](const ObjToken& token);
    const BaseObject& operator[](ObjToken& token) const;
    const BaseObject& operator[](const ObjToken& token) const;

	// 尝试把 token（可能是 pending）解析为已注册的真实 token：
	// - 非 const 版本会在成功时用真实 token 覆盖输入 token 并返回 true（caller 可继续用该 token 访问对象）
    bool TryGetRegisteration(ObjToken& token) const noexcept;
    bool TryGetRegisteration(const ObjToken& token) const noexcept;

    // 支持以 PendingToken 直接销毁（若已合并则转为销毁真实 ObjToken，否则销毁 pending）
    // - Destroy 会智能判断传入 token 是 pending 还是已注册 token，并采取相应路径
    void Destroy(const ObjToken& p) noexcept;

    // DestroyAll: 立即销毁所有对象并清理所有挂起队列，通常在程序退出或重置时调用。
    // - 会调用每个对象的 OnDestroy 并反注册物理系统，确保不会留下悬挂资源。
    void DestroyAll() noexcept;

    // UpdateAll: 每帧主更新入口，顺序：
    // 1) 为每个活跃对象调用 FramelyApply()（物理积分/调试绘制/记录 prev pos）
    // 2) 调用 PhysicsSystem::Step()（碰撞检测与回调）
    // 3) 为每个活跃对象调用 Update()
    // 4) 执行所有延迟销毁（在安全点处理，避免在遍历中删除）
    // 5) 提交本帧 pending 创建（将 pending_creates_ 合并到 objects_ 并注册到物理系统）
    void UpdateAll() noexcept;

    size_t Count() const noexcept { return alive_count_; }

private:
    ObjManager() noexcept;
    ~ObjManager() noexcept;

    struct Entry {
        std::unique_ptr<BaseObject> ptr;
        uint32_t generation = 0;
        bool alive = false;
        // 新增：创建当帧跳过 FramelyUpdate 的标志（用于合并时可能需要跳过本帧更新）
        bool skip_update_this_frame = false;
    };

    // pending create 的中间结构：在 CreateEntry 时只把对象放到这里（不直接扩展 objects_），
    // 在 UpdateAll 的提交阶段再把它们合并到 objects_（安全点，避免在更新循环中重分配）
    struct PendingCreate {
        std::unique_ptr<BaseObject> ptr;
    };

    // 为延迟创建保留 slot，并返回可用索引（复用 free_indices_ 或在尾部追加）
    uint32_t ReserveSlotForCreate() noexcept;

    // 内部立即销毁实现（按 index）。
    // - DestroyEntry 调用对象 OnDestroy、反注册 PhysicsSystem、清理映射并使 token 失效
    void DestroyEntry(uint32_t index) noexcept;
    // DestroyPending: 如果 pending 尚未合并，可直接销毁 pending 对象（用于 Destroy(pendingToken) 情形）
    void DestroyPending(const ObjToken& p) noexcept;
    // DestroyExisting: 将销毁请求入队（对于已合并对象），实际删除在下一次 UpdateAll 时执行；对于 pending 对象请使用 DestroyPending。
    void DestroyExisting(const ObjToken& token) noexcept;

    // 将 unique_ptr<BaseObject> 的对象纳入管理并在必要时调用 Start()，返回 PendingToken 表示创建请求。
    // 对象会被放入 pending_creates_（带 id），在 UpdateAll 的提交阶段合并到 objects_ 并完成物理注册。
    ObjToken CreateEntry(std::unique_ptr<BaseObject> obj);

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