#include "obj_manager.h"
#include "base_object.h" // 获得 BaseObject 的完整定义用于实现
#include <typeinfo>
#include <iostream>
#include <cstdint>

ObjManager & ObjManager::Instance() noexcept
{
    static ObjManager inst;
    return inst;
}

BaseObject* ObjManager::Get(const ObjToken& token) noexcept
{
    if (token.index >= objects_.size()) return nullptr;
    const Entry& e = objects_[token.index];
    if (!e.alive) return nullptr;
    if (e.generation != token.generation) return nullptr;
    return e.ptr.get();
}

bool ObjManager::IsValid(const ObjToken& token) const noexcept
{
    if (token.index >= objects_.size()) return false;
    const Entry& e = objects_[token.index];
    return e.alive && (e.generation == token.generation) && e.ptr;
}

// 非模板的创建实现：从 unique_ptr<BaseObject> 放入槽并 Start()
ObjManager::ObjToken ObjManager::CreateImmediateFromUniquePtr(std::unique_ptr<BaseObject> obj)
{
    if (!obj) {
        std::cerr << "[InstanceController] CreateImmediateFromUniquePtr: factory returned nullptr\n";
        return ObjToken::Invalid();
    }

    uint32_t index = ReserveSlotForCreate();
    Entry& e = objects_[index];
    e.ptr = std::move(obj);
    e.alive = true;
    ++e.generation; // 每次分配新的 generation
    object_index_map_[e.ptr.get()] = index;
    ++alive_count_;

    BaseObject* raw = e.ptr.get();
    raw->Start();

    // 注册到物理系统（在 Start 之后，确保碰撞形状等已设置）
    ObjManager::ObjToken tok{ index, e.generation };
    PhysicsSystem::Instance().Register(tok, raw);

    std::cerr << "[InstanceController] CreateImmediate: created object at " << static_cast<const void*>(raw)
        << " token(index=" << index << ", gen=" << e.generation << ")\n";

    return ObjToken{ index, e.generation };
}

// 非模板的延迟创建实现：记录 factory 和目标槽 index
ObjManager::ObjToken ObjManager::CreateDelayedFromFactory(std::function<std::unique_ptr<BaseObject>()> factory)
{
    if (!factory) {
        std::cerr << "[InstanceController] CreateDelayedFromFactory: null factory\n";
        return ObjToken::Invalid();
    }

    uint32_t index = ReserveSlotForCreate();
    Entry& e = objects_[index];
    ++e.generation; // generation 提前 bump，使得返回的 token 是唯一的
    ObjToken token{ index, e.generation };

    std::cerr << "[InstanceController] CreateDelayed: enqueue token(index=" << index << ", gen=" << e.generation << ")\n";

    pending_creates_.emplace_back(PendingCreate{
        index,
        std::move(factory)
        });

    return token;
}

void ObjManager::DestroyEntryImmediate(uint32_t index) noexcept
{
    if (index >= objects_.size()) return;
    Entry& e = objects_[index];
    if (!e.alive || !e.ptr) return;

    BaseObject* raw = e.ptr.get();
    std::cerr << "[InstanceController] DestroyEntryImmediate: destroying object at "
        << static_cast<const void*>(raw) << " (type: " << typeid(*raw).name() << ", index=" << index
        << ", gen=" << e.generation << ")\n";

    // 先从物理系统注销，避免回调触达已销毁对象
    ObjManager::ObjToken tok{ index, e.generation };
    PhysicsSystem::Instance().Unregister(tok);

    // 回调销毁钩子
    e.ptr->OnDestroy();

    // 从指针映射中移除
    object_index_map_.erase(raw);

    // 销毁对象并标记为可重用
    e.ptr.reset();
    e.alive = false;

    // 增加 generation 以使旧 token 失效
    ++e.generation;

    free_indices_.push_back(index);

    if (alive_count_ > 0) --alive_count_;
}

void ObjManager::DestroyImmediate(BaseObject* ptr) noexcept
{
    if (!ptr) return;
    auto it = object_index_map_.find(ptr);
    if (it != object_index_map_.end()) {
        uint32_t index = it->second;
        // Double-check pointer still matches (defensive)
        if (index < objects_.size() && objects_[index].ptr.get() == ptr) {
            DestroyEntryImmediate(index);
            return;
        }
    }

    // 兼容性：如果映射未命中，尝试线性查找（应该不常见）
    for (uint32_t i = 0; i < objects_.size(); ++i) {
        Entry& e = objects_[i];
        if (e.ptr && e.ptr.get() == ptr) {
            DestroyEntryImmediate(i);
            return;
        }
    }

    std::cerr << "[InstanceController] DestroyImmediate: target not found at "
        << static_cast<const void*>(ptr) << "\n";
}

void ObjManager::DestroyImmediate(const ObjToken& token) noexcept
{
    if (token.index >= objects_.size()) {
        std::cerr << "[InstanceController] DestroyImmediate(token): invalid index " << token.index << "\n";
        return;
    }
    Entry& e = objects_[token.index];
    if (!e.alive || e.generation != token.generation) {
        std::cerr << "[InstanceController] DestroyImmediate(token): token invalid or object not alive (index="
            << token.index << ", gen=" << token.generation << ")\n";
        return;
    }
    DestroyEntryImmediate(token.index);
}

void ObjManager::DestroyDelayed(BaseObject* ptr) noexcept
{
    if (!ptr) return;
    auto it = object_index_map_.find(ptr);
    if (it == object_index_map_.end()) {
        // 兼容线性查找
        for (uint32_t i = 0; i < objects_.size(); ++i) {
            if (objects_[i].ptr && objects_[i].ptr.get() == ptr) {
                ObjToken tok{ i, objects_[i].generation };
                DestroyDelayed(tok);
                return;
            }
        }
        std::cerr << "[InstanceController] DestroyDelayed: target not found at "
            << static_cast<const void*>(ptr) << "\n";
        return;
    }
    uint32_t index = it->second;
    const Entry& e = objects_[index];
    DestroyDelayed(ObjToken{ index, e.generation });
}

void ObjManager::DestroyDelayed(const ObjToken& token) noexcept
{
    if (token.index >= objects_.size()) {
        std::cerr << "[InstanceController] DestroyDelayed(token): invalid index " << token.index << "\n";
        return;
    }
    const Entry& e = objects_[token.index];
    if (!e.alive || e.generation != token.generation) {
        std::cerr << "[InstanceController] DestroyDelayed(token): token invalid or object not alive (index="
            << token.index << ", gen=" << token.generation << ")\n";
        return;
    }

    uint64_t key = (static_cast<uint64_t>(token.index) << 32) | token.generation;
    if (pending_destroy_set_.insert(key).second) {
        pending_destroys_.push_back(token);
        std::cerr << "[InstanceController] DestroyDelayed: enqueued destroy for index=" << token.index
            << " gen=" << token.generation << "\n";
    }
    else {
        std::cerr << "[InstanceController] DestroyDelayed: already enqueued for index=" << token.index
            << " gen=" << token.generation << "\n";
    }
}

void ObjManager::DestroyAll() noexcept
{
    std::cerr << "[InstanceController] DestroyAll: destroying all objects (" << alive_count_ << ")\n";

    // 清理延迟队列
    pending_creates_.clear();
    pending_destroys_.clear();
    pending_destroy_set_.clear();

    // 注销所有对象到物理系统
    for (uint32_t i = 0; i < objects_.size(); ++i) {
        Entry& e = objects_[i];
        if (e.alive && e.ptr) {
            PhysicsSystem::Instance().Unregister(ObjToken{ i, e.generation });
        }
    }

    // 销毁所有条目
    for (uint32_t i = 0; i < objects_.size(); ++i) {
        Entry& e = objects_[i];
        if (e.alive && e.ptr) {
            e.ptr->OnDestroy();
            object_index_map_.erase(e.ptr.get());
            e.ptr.reset();
            e.alive = false;
            ++e.generation; // 使旧 token 失效
            free_indices_.push_back(i);
        }
    }

    objects_.clear();
    free_indices_.clear();
    object_index_map_.clear();
    alive_count_ = 0;
}

void ObjManager::UpdateAll() noexcept
{
    // 1) FramelyUpdate 对现有对象
    for (auto& e : objects_) {
        if (e.alive && e.ptr) {
            e.ptr->FramelyUpdate();
        }
    }

    // 在对象更新后运行物理系统的 Step（broadphase + narrowphase + 派发）
    PhysicsSystem::Instance().Step();

    // 2) 处理延迟删除
    if (!pending_destroys_.empty()) {
        for (const ObjToken& token : pending_destroys_) {
            if (token.index >= objects_.size()) {
                std::cerr << "[InstanceController] UpdateAll: pending destroy invalid index " << token.index << "\n";
                continue;
            }
            Entry& e = objects_[token.index];
            if (!e.alive || e.generation != token.generation) {
                std::cerr << "[InstanceController] UpdateAll: pending destroy target not found or token mismatch (index="
                    << token.index << ", gen=" << token.generation << ")\n";
                continue;
            }

            std::cerr << "[InstanceController] UpdateAll: executing destroy for object at index=" << token.index
                << " gen=" << token.generation << " (type: " << typeid(*e.ptr).name() << ")\n";

            // 调用销毁并释放槽位
            DestroyEntryImmediate(token.index);
        }

        pending_destroys_.clear();
        pending_destroy_set_.clear();
    }

    // 3) 处理延迟创建：按队列顺序将工厂创建的对象放回预留的槽，并 Start()
    if (!pending_creates_.empty()) {
        for (auto& pc : pending_creates_) {
            uint32_t index = pc.index;
            if (index >= objects_.size()) {
                std::cerr << "[InstanceController] UpdateAll: pending create target index out of range: " << index << "\n";
                continue;
            }

            Entry& e = objects_[index];
            if (e.alive) {
                std::cerr << "[InstanceController] UpdateAll: pending create target slot already occupied (index="
                    << index << ")\n";
                continue;
            }

            std::unique_ptr<BaseObject> obj;
            try {
                obj = pc.factory();
            }
            catch (...) {
                std::cerr << "[InstanceController] UpdateAll: exception while creating deferred object for index="
                    << index << "\n";
                // 使 token 失效并回收槽位
                ++e.generation;
                free_indices_.push_back(index);
                continue;
            }

            if (!obj) {
                std::cerr << "[InstanceController] UpdateAll: factory returned nullptr for index=" << index << "\n";
                ++e.generation;
                free_indices_.push_back(index);
                continue;
            }

            // 放入槽位并启动
            BaseObject* raw = obj.get();
            e.ptr = std::move(obj);
            e.alive = true;
            object_index_map_[raw] = index;
            ++alive_count_;

            std::cerr << "[InstanceController] UpdateAll: creating deferred object at " << static_cast<const void*>(raw)
                << " (type: " << typeid(*raw).name() << ", index=" << index << ", gen=" << e.generation << ")\n";

            raw->Start();

            // 注册到物理系统（在 Start 之后，确保碰撞形状等已设置）
            ObjManager::ObjToken tok{ index, e.generation };
            PhysicsSystem::Instance().Register(tok, raw);
        }

        pending_creates_.clear();
    }
}