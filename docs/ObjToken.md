# ObjToken 说明

## 概述
- `ObjToken` 是对受 `ObjManager` 管理对象的轻量句柄（handle）。它采用 `(index, generation)` 的组合来安全引用对象槽位，从而避免裸指针在槽被复用时产生悬挂访问。
- 本文档重点说明 `ObjToken` 的字段语义、生命周期语义（包括 pending/registered 区别），以及它与 `ObjManager`、`PhysicsSystem`、`BaseObject` 等模块的交互关系与使用约定。

## 结构体定义（简要）
- 定义位于：`head/object_token.h`
- 主要字段：
  - `uint32_t index`：槽索引或 pending id（默认：`std::numeric_limits<uint32_t>::max()` 表示无效）。
  - `uint32_t generation`：槽的版本号，由 `ObjManager` 在回收/复用槽时递增。
  - `bool isRegitsered`：标识该 token 是否为“已注册/真实” token（true 表示 `index` 指向 `ObjManager::objects_` 中的真实槽；false 常表示 pending token）。
- 帮助方法：
  - `isValid()`：仅检查 `index` 是否为 sentinel（不表示对象仍活着）。
  - `operator==/!=`：比较 `index` 与 `generation`，用于判等。
  - `static Invalid()`：构造一个无效 token。

## 语义与生命周期
- 两类语义状态
  - Pending token（通常 `isRegitsered == false`）：
    - 在 `ObjManager::CreateEntry` 创建对象时返回。此时对象已构造并已执行 `Start()`，但尚未合并到 `objects_`，其 `index` 存储为 pending id（非真实槽索引）。
    - 在下一次 `ObjManager::UpdateAll()` 的提交阶段，pending 对象会被合并为真实槽，并在 `pending_to_real_map_` 中记录 pending id -> 真实 `ObjToken` 的映射。
    - 可通过 `ObjManager::TryGetRegisteration` 将 pending token 升级为真实 token。
  - Registered token（`isRegitsered == true`）：
    - 表示该 token 已经被写回为真实 (index, generation)，并且 `ObjManager` 的 `objects_[index]` 在该 generation 上应当是活跃的对象。
    - Registered token 仍可能因对象被销毁而失效，此时 `generation` 与 `objects_[index].generation` 不匹配，`ObjManager::IsValid` 返回 false。
- Generation 的作用
  - 当 `ObjManager` 回收某一 slot 并重新用于新对象时，会增加该 slot 的 `generation`，使所有指向旧 `(index, old_generation)` 的旧 token 失效，避免错误访问。
- 为什么不只用 index？
  - 仅用 index 无法区分旧 token 与新对象（槽被复用），generation 是防止“ABA”类错误的简单而高效手段。

## 与 `ObjManager` 的关系（核心）
- `ObjManager` 管理 `objects_`、`pending_creates_`、`pending_to_real_map_`、`pending_destroys_` 等结构，是 `ObjToken` 的发行者与验证器。
- 推荐调用模式：
  - 获取 token：`auto tok = objs.Create<MyType>(...);`（返回 pending token）
  - 在下一帧或合适时机通过 `objs.TryGetRegisteration(tok)` 将其解析为真实 token，或在访问前使用 `objs.IsValid(tok)` / `objs.operator[]`（operator[] 对 pending 有额外处理逻辑）。
- `ObjManager::operator[]` 的行为：
  - 对 pending token：非 const 版本会先在 `pending_creates_` 中查找并直接返回未合并对象引用；否则尝试 `TryGetRegisteration` 后再访问真实对象。
  - 对 registered token：会验证 `index`/`generation` 并在非法时抛出 `std::out_of_range`（同时写入诊断日志）。
- 销毁交互：
  - 传入 pending token 到 `ObjManager::Destroy` 会触发 `DestroyPending`（直接销毁 pending 实例）。
  - 传入 registered token 会将其入队 `pending_destroys_`，并在 `UpdateAll()` 的安全点执行实际销毁（`DestroyEntry`）。

## 与 `PhysicsSystem` / `BaseObject` 的关系
- `PhysicsSystem::Register` / `Unregister` 使用 `ObjToken` 将 `BasePhysics` 条目与 `ObjManager` 的对象句柄关联起来。通常这一注册在 pending 合并为真实 token 时由 `ObjManager` 自动完成。
- 碰撞回调流程（高层）：
  1. `PhysicsSystem::Step()` 发现碰撞，产生 `CollisionEvent`，其中保存的是参与碰撞对象的 `ObjToken`（来自 entries_）。
  2. 在触发回调前，`PhysicsSystem` 会通过 `ObjManager::IsValid` 确认 token 仍然有效（对象未在本帧中被销毁或被重用）。
  3. 使用 `ObjManager::operator[]`（或通过 TryGetRegisteration/IsValid 验证后）获取 `BaseObject&`，并调用 `BaseObject::OnCollisionState` / `OnCollisionEnter/Stay/Exit`。
- 因为 `ObjToken` 用于跨模块引用（物理系统持有 token），它必须保持轻量且可序列化（index/generation）。当对象被销毁或槽复用时，旧 token 的无害失效保证了回调阶段的安全。

## 使用建议与最佳实践
- 永远不要仅凭 `token.index` 判断对象是否存在；使用 `ObjManager::IsValid(token)` 或 `TryGetRegisteration`。
- 若需要在回调或外部保存 token 并在后续帧访问，请：
  - 保存 token，调用前先 `TryGetRegisteration` 或 `IsValid`；若 `TryGetRegisteration` 返回 true，替换为真实 token（非 const 版本会覆盖输入 token）。
- 当从 `Create` 获得 pending token 并立即需要使用对象（例如立即初始化或查询），可：
  - 直接通过 `ObjManager::operator[]` 访问 pending（非 const 版本支持访问 pending_creates_），但应注意对象尚未成为“registered”，并且在下一次 `UpdateAll()` 合并时其真实 token 才会产生。
- 在碰撞回调或复杂生命周期操作中，避免直接在回调里立即 `delete` 对象；应调用 `ObjManager::Destroy(token)` 由 `ObjManager` 在安全点处理。

## 简短示例
```cpp

// 创建并保存 token（可能是 pending） 
ObjToken tok = objs.Create<MyObject>(/* args */);

// 下一帧或在合适位置尝试解析为真实 token 
if (objs.TryGetRegisteration(tok)) { 
    // tok 已被更新为真实 token，且 tok.isRegitsered==true 
    if (objs.IsValid(tok)) { 
        BaseObject& obj = objs[tok]; // 通过operator[](tok)安全访问已注册对象
        // 使用 obj
    } 
    else {
        // 仍为 pending：可通过 operatornon-const 直接访问 pending 实例（非合并状态） 
        // 或等待下一帧 UpdateAll 提交 
    }
}
```

## 常见陷阱
- 误用 `isValid()`：`ObjToken::isValid()` 仅判断 index 字段是否为 sentinel，不代表对象当前仍存活。应使用 `ObjManager::IsValid()` 进行实际验证。
- 忽略 generation：直接以 `index` 访问 `objects_` 会导致读取被复用槽的全新对象，从而产生严重逻辑错误。
- 在多线程环境中无同步地保存/传递 token：`ObjManager` 设计为单线程（主循环）使用，跨线程读写必须加锁或通过消息/同步在主线程完成注册/销毁/访问。

## 总结
- `ObjToken` 是连接 `ObjManager`、`PhysicsSystem`、`BaseObject` 等模块的轻量句柄，它通过 `(index, generation, isRegitsered)` 表达 pending/registered/invalid 等多种状态。
- 使用 `ObjToken` 时应依赖 `ObjManager` 提供的验证与解析函数来确保安全访问；避免直接以 `index` 推断对象状态或做并发访问。  