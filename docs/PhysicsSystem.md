# PhysicsSystem

## 概述  
`PhysicsSystem` 是 2D 碰撞检测与事件分发子系统，负责将所有注册的 `BasePhysics` 条目纳入 broadphase/narrowphase 流程，合并重复 contact，并按帧产生 Enter/Stay/Exit 回调给对应的 `BaseObject`。

## 主要职责
- 注册 / 反注册：通过 `Register(const ObjToken&, BasePhysics*)` / `Unregister` 管理参与碰撞检测的条目。
- 每帧推进 `Step(cell_size)`：
  - 将各对象的 world-space 形状（或由 `BasePhysics` 提供的 world-shape）放入 broadphase 网格（grid）；
  - 对每个格子及其邻域执行 narrowphase（调用 `cf_collide`）；
  - 合并属于同一对的多个 contact（最多保留两个 contact）；
  - 基于上一帧 `prev_collision_pairs_` 与本帧 `current_pairs_` 生成 Enter/Stay/Exit，并调用 `BaseObject::OnCollisionState`。

## 工作流程要点
- world-space 形状来源：
  - 若 `BasePhysics::is_world_shape_enabled()` 为 true，PhysicsSystem 直接使用 `get_shape()` 返回的 shape（假定已是 world-space）。
  - 否则 PhysicsSystem 会把 local shape 平移到 object.position（rotation/pivot 由 BasePhysics 的转换负责）。
- Broadphase：使用轴对齐包围盒（AABB）与固定格子大小（默认 cell_size = 64.0f）将对象索引入网格，减少 narrowphase 对比对数。
- Narrowphase：对候选对调用 `cf_collide`，并将返回的 `CF_Manifold` 做基础校验与归一（避免 NaN/Inf、负深度等）。
- 合并：同一对可能产生多个 contact，系统合并这些 manifold（合并 contact points、采用加权法线），并限制最终 count ≤ 2，便于上层处理。
- 事件分发：对每个合并后的事件，先根据 token 验证对象仍然有效（通过 `ObjManager::IsValid`），再用 `ObjManager::operator[]` 获取引用并调用 `OnCollisionState`，区分 Enter/Stay。对于上一帧存在但本帧消失的对，触发 Exit 回调（manifold 为空）。

## 数据结构（简要）
- `entries_`：存放注册条目（token + BasePhysics* + grid 坐标）。
- `grid_`：从 grid_key -> 条目索引列表（用于 broadphase）。
- `world_shapes_`：按 entries_ 缓存的 world-space shape（每帧刷新）。
- `events_`、`merged_map_`、`prev_collision_pairs_`：用于保存、合并与比较碰撞事件生成回调序列。

## 使用建议与注意
- `Register` / `Unregister` 的调用应配合 `ObjManager`：在对象合并到管理器后注册物理（`ObjManager` 在提交阶段自动注册）。
- 碰撞回调顺序：系统不保证对 a/b 调用的先后顺序；回调应对并发修改对象集合保持健壮（例如避免在回调中立即删除对象，而使用 `ObjManager::Destroy` 让其延迟处理）。
- 性能调优：调整 `cell_size` 可改变 broadphase 粒度，过小会增加 bucket 数量，过大会增加 narrowphase 对数。启用 `BasePhysics::enable_world_shape(true)` 可减少重复坐标变换开销（前提是上层能正确提供 world-space shape）。