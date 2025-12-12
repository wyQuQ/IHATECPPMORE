# ObjManager

## 概述  
`ObjManager` 提供对象集中管理、生命周期控制与句柄（`ObjToken`）系统，保证在创建、销毁与每帧更新过程中对对象集合的安全操作（避免迭代时修改导致的不一致）。设计采用延迟创建/延迟销毁队列与 (index, generation) token 机制以防止悬挂引用。构造/销毁的实际注册与注销在安全点（`UpdateAll()` 的提交阶段）完成。

## 设计要点与契约
- token 语义：`ObjToken` 使用 `(index, generation)`，当某个 slot 被回收并复用时会增加 generation，从而让旧 token 失效。
- 延迟创建/销毁：
  - 创建：`CreateEntry` 会把刚构造的对象放入 `pending_creates_`（pending 区），并会立即调用对象的 `Start()`。真实槽位与最终的 `ObjToken` 会在下一次 `UpdateAll()` 的提交阶段合并（并在 `pending_to_real_map_` 中写回对应关系）。
  - 销毁：区分 pending 与 existing。`DestroyPending` 可直接销毁尚未合并的 pending 对象；`DestroyExisting` 会将已注册的 token 入队 `pending_destroys_`，实际销毁在 `UpdateAll()` 的安全点执行以避免在遍历期间删除对象。
- 单线程契约：`ObjManager` 的绝大多数接口非线程安全，应在主线程（游戏循环）使用；跨线程需由调用方自行同步。

## 每帧流程与 UpdateAll 顺序
`UpdateAll()`（被标记为 `APPLIANCE`，表示涉及每帧物理更新并由框架自动在正确时机调用）的大致执行顺序：
1. 为每个已合并且活跃的对象调用 `FrameEnterApply()`（缓存上一帧位置并执行 `ApplyForce()`/`ApplyVelocity()` 等物理子步）。
2. 调用物理系统步进（例如 `PhysicsSystem::Step()`，负责碰撞检测并触发碰撞回调）。
3. 为每个已合并对象调用 `Update()`（游戏逻辑/行为）。
4. 处理 `pending_destroys_` 中的销毁请求（调用 `DestroyEntry` 等内部实现，触发 `OnDestroy()` 并反注册物理系统）。
5. 提交本帧 `pending_creates_`：为 pending 对象分配 slots、写回真实 `ObjToken`（并更新 `pending_to_real_map_`）、在物理系统中注册对象。  
注意：条目中存在 `skip_update_this_frame` 标志，合并/注册时可能用到以控制是否跳过本帧的帧级更新。

## 主要 API（概览与语义）
- Create：
  - `template <typename T, ...> ObjToken Create(Args&&... args)`：构造对象并返回 pending token（对象处于 pending 状态且已调用 `Start()`）。
  - `template <typename T, typename Init, ...> ObjToken Create(Init&& initializer, Args&&... args)`：允许在 `Start()` 前对对象做初始设置；该重载受约束，仅在 `initializer` 可对 `T*` 调用时才参与重载决议。
- 访问与验证：
  - `bool IsValid(const ObjToken& token) const noexcept`：检查 token 是否指向当前已合并且仍然存活的对象。
  - `BaseObject& operator[](ObjToken& token)` / `BaseObject& operator[](const ObjToken& token)` / const 版本：通过 token 获取对象引用。语义：若 token 无效会抛出 `std::out_of_range`（并写入 `std::cerr`）。对于 pending token，实施层会尝试解析 pending（或使用 `TryGetRegisteration` 升级为真实 token）；非 const 版本在成功解析后会修改传入 token（将 pending token 替换为真实 token）。
  - `bool TryGetRegisteration(ObjToken& token) const noexcept` / `bool TryGetRegisteration(const ObjToken& token) const noexcept`：尝试将可能的 pending token 升级为已注册的真实 token。非 const 版本在成功时会用真实 token 覆盖输入 token 并返回 true，方便后续直接用该 token 访问对象。
- 销毁：
  - `void Destroy(const ObjToken& p) noexcept`：智能判断并处理 pending / existing token；对于已注册对象会将其入队延迟销毁，对于 pending 直接销毁 pending 记录。
  - `void DestroyAll() noexcept`：立即销毁所有对象并清理所有 pending 列表（会调用每个对象的 `OnDestroy()` 并反注册物理系统）。
- 其它：
  - `APPLIANCE void UpdateAll() noexcept`：每帧主入口，负责 `FrameEnterApply()`/物理步进/`Update()`/pending 销毁/pending 创建的合并与注册。
  - `size_t Count() const noexcept`：返回当前活跃对象计数（包含 pending 创建）。

## 内部数据结构说明（对使用者有参考价值）
- `objects_`：已合并对象槽（vector of Entry），Entry 包含 `unique_ptr<BaseObject>`、`generation`、`alive`、以及 `skip_update_this_frame` 标志。
- `free_indices_`：空闲槽索引池，用于复用槽位。
- `object_index_map_`：仅包含已合并对象的从 `BaseObject*` 到 `objects_` 索引的映射，便于快速查找。
- `pending_creates_`（map：pending id -> PendingCreate）与 `pending_ptr_to_id_`（从 ptr 到 pending id 的快速映射）：保存本帧刚创建但尚未合并的对象。
- `pending_to_real_map_`：合并完成后记录 pending id -> 真正的 `ObjToken`，便于上层或调用方解析 pending。
- `pending_destroys_` / `pending_destroy_set_`：延迟销毁队列及其去重集合（用于避免重复入队）。
- `next_pending_id_` 从 1 开始单调递增用于生成 pending id。
- `alive_count_`：当前存活对象计数（包含 pending）。

## 实现备注与注意事项（对调用方的建议）
- Create 后对象会立即执行 `Start()`，但直到下一次 `UpdateAll()` 才会被合并为真正的已注册对象并获得可在 `object_index_map_` 中查找的真实 token。
- 在遍历/更新循环中不要直接 delete 对象；应使用 `Destroy()` 将销毁请求放入队列或销毁 pending，以确保在安全点完成清理。
- 若需要在创建后立刻以真实 token 访问对象，调用方应在下一次 `UpdateAll()` 提交后使用 `TryGetRegisteration` 或检查 `pending_to_real_map_`（由 `ObjManager` 写入）来解析真实 token。
- `ObjManager` 假定在主线程使用；跨线程访问必须由调用方保证同步，否则可能出现数据竞争或不一致行为。