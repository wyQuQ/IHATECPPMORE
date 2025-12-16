# ObjManager

## 概述
`ObjManager` 维护一个 BaseObject 的单例容器，使用 ObjToken 将延迟创建的 pending 对象与已注册对象区分开来，并通过 UpdateAll 完成每帧的物理/碰撞与生命周期调度。

## 接口说明
- `Create<T>(Args&&...)`：构建派生自 BaseObject 的对象并立即执行 Start()，返回 pending ObjToken（`isRegitsered == false`），对象会被置入 `pending_creates_`，在下一帧 UpdateAll 提交后升级为真实 token 并参与物理系统。
- `Create<T>(Init&&, Args&&...)`：同上，但可以在 Start 前通过 `initializer(T*)` 调整对象状态；`Init` 仅在可调用时参与重载决议。
- `IsValid(const ObjToken&)`：验证一个已注册 token 是否仍然指向活跃对象（检查 index、generation 与 alive 标志），不展开 pending token。
- `operator[](ObjToken&)`：非 const 版在 pending token 情况下直接检索 pending_creates_ 并返回 BaseObject，若已提交则通过 TryGetRegisteration 更新 token 后委托 const 版；抛出异常时会记录到 std::cerr。
- `operator[](const ObjToken&)`：const 版本仅接受已注册 token，确保 index/generation/alive/pointer 通过后返回 BaseObject&。
- `TryGetRegisteration(ObjToken&)`：非 const 版本会尝试使用 `pending_to_real_map_` 将 pending token 替换为已注册 token（或验证已有 token），返回是否有效；对 pending 阶段的访问必要时会修改 token。
- `TryGetRegisteration(const ObjToken&)`：const 版本只查询映射或验证，**不**修改输入 token；常用于需要在只读上下文确认 token 状态时调用。
- `Destroy(const ObjToken&)`：对 pending token 会走 DestroyPending，立即销毁 pending BaseObject；对已注册 token 会将其入队 `pending_destroys_`，等待 UpdateAll 安全地调用 DestroyEntry、OnDestroy 与 PhysicsSystem::Unregister。
- `DestroyAll()`：清空 pending 和 registered 所有对象，逐个调用 BaseObject::OnDestroy、让 ObjToken 失效、同时反注册 PhysicsSystem 并重置索引池，适合退出或场景重置时使用。
- `UpdateAll()`：每帧调度入口，顺序为 FrameEnterApply（可清理 `m_collide_manifolds` 并应用物理）、PhysicsSystem::Step（触发 OnCollisionState）、Update、FrameExitApply、处理 pending 销毁、提交 pending 创建并为新对象注册 PhysicsSystem、支持 skip_update_this_frame 使某些对象在本帧跳过上述调用。
- `FindTokensByTag(const std::string&)`：遍历 registered `objects_`，返回第一个拥有指定 tag 的对象 token（可用于快速查找 Active BaseObject）。
- `Count()`：返回包含 pending 的当前 alive 对象数量。

## 底层结构要点
- `pending_creates_` 与 `pending_ptr_to_id_` 保存尚未合并的 BaseObject，Create 立即调用 Start 但只在 UpdateAll 提交后完成物理注册并写入 `pending_to_real_map_`；operator[] 可访问 pending 创建的对象。
- `objects_` 维护已注册对象条目，带 `generation`、`alive` 与 `skip_update_this_frame` 标志；`free_indices_` 可复用已销毁 slot。
- `pending_destroys_` 和 `pending_destroy_set_` 避免重复销毁，一旦 UpdateAll 执行 DestroyEntry，就会调用 BaseObject::OnDestroy 并使对应 ObjToken 失效。
- `object_index_map_` 允许 BaseObject* 反查所在 index，用于物理系统与 DestroyEntry。

## 使用约定
- 以上接口均非线程安全，应在主线程的游戏循环中调用。
- APPLIANCE 标记的方法（如 UpdateAll）涉及每帧物理/碰撞推进，通常由框架驱动，不要手动在每帧重复调用。