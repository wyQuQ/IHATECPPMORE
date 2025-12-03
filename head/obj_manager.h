#pragma once

#include <memory>
#include <vector>
#include <algorithm>
#include <type_traits>
#include <functional>
#include <unordered_set>
#include <unordered_map>
#include <tuple>
#include <utility>
#include <cstdint>
#include <limits>

#include "object_token.h"

// 前向声明以打破头文件循环依赖
class BaseObject;

class ObjManager {
public:
    static ObjManager& Instance() noexcept;

    ObjManager(const ObjManager&) = delete;
    ObjManager& operator=(const ObjManager&) = delete;

    using ObjToken = ::ObjToken;

    // 立即创建并 Start，返回 token（控制器持有所有权）
    // 精简：转发到 CreateInit，避免重复实现
    template <typename T, typename... Args>
    ObjToken CreateImmediate(Args&&... args)
    {
        static_assert(std::is_base_of<BaseObject, T>::value, "T must derive from BaseObject");
        // 使用空 initializer 转发到 CreateInit（保持向后兼容，同时把核心逻辑集中到 CreateInit）
        return CreateInit<T>([](T*) {}, std::forward<Args>(args)...);
    }

    // 高频友好：创建并在 Start() 之前运行 initializer（模板内联，避免 std::function 抽象开销）
    // 使用示例：
    // InstanceController::Instance().CreateInit<MyObj>([](MyObj* o){ o->SetPosition(...); }, ctorArg1, ctorArg2);
    template <typename T, typename Init, typename... Args>
    ObjToken CreateInit(Init&& initializer, Args&&... args)
    {
        static_assert(std::is_base_of<BaseObject, T>::value, "T must derive from BaseObject");
        static_assert(std::is_invocable_v<Init, T*>, "initializer must be callable with T*");
        auto obj = std::make_unique<T>(std::forward<Args>(args)...);
        if (initializer) {
            std::forward<Init>(initializer)(static_cast<T*>(obj.get()));
        }
        return CreateImmediateFromUniquePtr(std::move(obj));
    }

    // 延迟创建：预留 slot 并返回 token，实际创建在本帧 Update 后执行（删除之后）
    template <typename T, typename... Args>
    ObjToken CreateDelayed(Args&&... args)
    {
        static_assert(std::is_base_of<BaseObject, T>::value, "T must derive from BaseObject");
        auto factory = [args_tuple = std::make_tuple(std::forward<Args>(args)...)]() mutable -> std::unique_ptr<BaseObject> {
            return std::apply([](auto&&... unpacked) {
                return std::make_unique<T>(std::forward<decltype(unpacked)>(unpacked)...);
            }, std::move(args_tuple));
        };
        return CreateDelayedFromFactory(std::move(factory));
    }

    // 通过 token 访问
    BaseObject* Get(const ObjToken& token) noexcept;
    template <typename T>
    T* GetAs(const ObjToken& token) noexcept { return static_cast<T*>(Get(token)); }

    bool IsValid(const ObjToken& token) const noexcept;

    // 立即销毁（存在即删除），支持按指针或按 token
    void DestroyImmediate(BaseObject* ptr) noexcept;
    void DestroyImmediate(const ObjToken& token) noexcept;

    // 延迟销毁：把对象标记为待删除，实际删除在 UpdateAll 的删除阶段执行（支持指针或 token）
    void DestroyDelayed(BaseObject* ptr) noexcept;
    void DestroyDelayed(const ObjToken& token) noexcept;

    // 销毁所有对象（程序退出时调用）
    void DestroyAll() noexcept;

    // 每帧调用：按顺序执行
    // 1) 对现有对象执行 FramelyUpdate()
    // 2) 处理延迟删除队列（销毁对象）
    // 3) 处理延迟创建队列（创建并 Start）
    void UpdateAll() noexcept;

    size_t Count() const noexcept { return alive_count_; }

private:
    ObjManager() noexcept = default;
    ~ObjManager() noexcept = default;

    struct Entry {
        std::unique_ptr<BaseObject> ptr;
        uint32_t generation = 0;
        bool alive = false;
    };

    struct PendingCreate {
        uint32_t index;
        std::function<std::unique_ptr<BaseObject>()> factory;
    };

    // 为创建预留槽（从空闲列表复用或扩展数组）
    uint32_t ReserveSlotForCreate() noexcept
    {
        if (!free_indices_.empty()) {
            uint32_t idx = free_indices_.back();
            free_indices_.pop_back();
            // 清理旧状态（ptr 已为 nullptr）
            Entry& e = objects_[idx];
            e.ptr.reset();
            e.alive = false;
            // generation 保持调用方对 generation 的 bump（CreateImmediate/Delayed 中负责）
            return idx;
        } else {
            objects_.emplace_back();
            return static_cast<uint32_t>(objects_.size() - 1);
        }
    }

    // 内部：按索引销毁（立即），不检查外部并发
    void DestroyEntryImmediate(uint32_t index) noexcept;

    // 非模板实现接口（在 cpp 中实现，避免头文件依赖 BaseObject 的完整定义）
    ObjToken CreateImmediateFromUniquePtr(std::unique_ptr<BaseObject> obj);
    ObjToken CreateDelayedFromFactory(std::function<std::unique_ptr<BaseObject>()> factory);

    // 对象容器：每个条目包含 ptr、generation、alive 标记
    std::vector<Entry> objects_;

    // 空闲槽索引（可复用）
    std::vector<uint32_t> free_indices_;

    // 指针 -> 索引 映射，加速按指针查找
    std::unordered_map<BaseObject*, uint32_t> object_index_map_;

    // 延迟删除集合（避免重复标记）
    std::vector<ObjToken> pending_destroys_;
    std::unordered_set<uint64_t> pending_destroy_set_; // compact key: ((uint64_t)index<<32)|generation

    // 延迟创建：把工厂和目标 index 存起来
    std::vector<PendingCreate> pending_creates_;

    // 存活计数（比 objects_.size() 更准确）
    size_t alive_count_ = 0;
};