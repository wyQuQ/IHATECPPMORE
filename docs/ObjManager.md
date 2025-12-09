# ObjManager

## 概述  
`ObjManager` 提供对象集中管理、生命周期控制与句柄（`ObjToken`）系统，保证在创建、销毁与每帧更新过程中对对象集合的安全操作（避免迭代时修改导致的不一致）。设计采用延迟创建/延迟销毁队列与 (index, generation) token 机制以防止悬挂引用。

## 设计要点与契约
- token 语义：`ObjToken` 使用 `(index, generation)`，当一个 slot 被回收并复用时会增加 generation，从而使旧 token 失效。
- 延迟操作：
  - 创建（CreateEntry）会把对象放入 `pending_creates_` 并立即调用对象的 `Start()`（处于 pending 状态）；真实槽位与 `ObjToken` 会在下一次 `UpdateAll()` 的提交阶段产生并写回 `pending_to_real_map_`。
  - 销毁分为 pending 与 existing：`DestroyPending` 直接销毁尚未合并的 pending 对象；`DestroyExisting` 会把已注册的 token 入队 `pending_destroys_`，实际销毁在 `UpdateAll()` 的安全点执行。
- UpdateAll 顺序：`FramelyApply()` → `PhysicsSystem::Step()` → `Update()` → 处理 pending 销毁 → 提交 pending 创建（合并并注册物理系统）。

## 主要 API（概览）
- Create：
  - `template <typename T, ...> ObjToken Create(Args&&... args)`：构造对象并返回 pending token。
  - `template <typename T, typename Init, ...> ObjToken Create(Init&& initializer, Args&&... args)`：允许在 `Start()` 前做初始化。
- 访问与验证：
  - `bool IsValid(const ObjToken& token) const noexcept`
  - `BaseObject& operator[](ObjToken& token)`（若 token 为 pending 会尝试解析或直接返回 pending 对象；无效时抛出 `std::out_of_range`）
  - `bool TryGetRegisteration(ObjToken& token) const noexcept`：尝试把 pending token 升级为已注册的真实 token（非 const 版本会覆盖输入 token）。
- 销毁：
  - `void Destroy(const ObjToken& p) noexcept`：自动分辨 pending / existing。
  - `void DestroyAll() noexcept`：立即销毁全部对象并清理 pending 队列。
- 其它：
  - `void UpdateAll() noexcept`：每帧主入口，负责所有对象的 FramelyApply/Update、物理 Step、pending 合并/销毁。

## 实现细节与注意事项
- 数据结构：
  - `objects_`：已合并对象槽，元素包含 `unique_ptr<BaseObject>`、generation、alive 标志。
  - `pending_creates_`：pending id -> PendingCreate（unique_ptr）映射。
  - `pending_to_real_map_`：pending id -> 真正的 ObjToken（合并后填充）。
  - `pending_destroys_` / `pending_destroy_set_`：延迟销毁队列与去重集合。
  - `object_index_map_`：从 `BaseObject*` 到 objects_ 索引的快速映射（仅已合并对象）。
- 生命周期：
  - `CreateEntry` 会立刻调用对象的 `Start()`，但对象不会立刻被加入 `objects_`（避免在遍历期间重分配）。
  - 合并时会注册到 `PhysicsSystem` 并把真实 token 写入对象（通过 `SetObjToken`，ObjManager 为 friend）。
- 并发：
  - `ObjManager` 设计为单线程使用（游戏主循环线程）。若跨线程访问需自行加锁并确保与 `UpdateAll()` 的调用序列一致。