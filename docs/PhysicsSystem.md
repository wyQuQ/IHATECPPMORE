# PhysicsSystem

## 概述  
`PhysicsSystem` 是 2D 碰撞检测与事件分发子系统，负责将所有注册的 `BasePhysics` 条目纳入 broadphase/narrowphase 流程，合并重复 contact，并按帧产生 Enter/Stay/Exit 回调给对应的 `BaseObject`。实现关注性能与数值稳定性（格子 broadphase、合并 contact、manifold 规范化等）。

## 主要职责
- 注册 / 反注册：通过 `Register(const ObjToken&, BasePhysics*)` / `Unregister` 管理参与碰撞检测的条目；重复注册会更新已有条目指针与 token。
- 每帧推进 `Step(cell_size)`：  
  - 构建/复用格子（grid）并索引每个条目的 world-space AABB；  
  - narrowphase：对格子邻域内候选对调用 `cf_collide`；  
  - 合并属于同一对的多个 contact（最多保留两个 contact，合并法线与深度）；  
  - 基于上一帧 `prev_collision_pairs_` 与本帧 `current_pairs_` 生成 Enter/Stay/Exit，并调用 `BaseObject::OnCollisionState`。

## 关键实现细节（影响行为的要点）
- world-space 形状来源：  
  - 若 `BasePhysics::is_world_shape_enabled()` 为 true，系统直接使用 `get_shape()` 返回的 shape（假定已是 world-space）。  
  - 否则系统会把 local shape 平移到 object.position（rotation/pivot 的旋转处理由 `BasePhysics` 在 `get_shape()`/转换路径中负责）。translate 操作不会对输入 shape 旋转，仅做位移，返回值为值拷贝。
- AABB 计算：通过 `shape_wrapper_to_aabb` 将任意形状映射为 world-space AABB（用于格子索引）。对于 capsule/circle/poly 会正确计算包围范围。
- Broadphase：使用固定格子（大小由 `cell_size` 参数控制，典型值 64.0f）。每个条目按其 world-space AABB 覆盖的格子入桶，桶用 `grid_` 存储，`grid_keys_used_` 用于复用/清空上帧容器以避免逐桶 reallocate。
- Narrowphase 与 manifold 规范化：  
  - 对候选对调用 `cf_collide`（通过封装函数 `shapes_collide_world`），返回 `CF_Manifold`。若 `m.count <= 0` 则认为未碰撞并跳过。  
  - 对得到的 manifold 做基础校验/归一化（`normalize_and_clamp_manifold` 的行为）：限制 contact count ∈ [0,2]、清理/夹紧穿透深度（非负、非 NaN/Inf）、清理非法 contact point 值、并归一化法线向量。
- 合并策略（同一对多次 contact）：  
  - 使用基于 token 的 pair_key 聚合同一对事件（内部使用 `merged_map_` / `merged_order_` 保持顺序）。  
  - 合并优先保留与物体速度方向更一致的法线：若一方物体有明显速度，会计算法线与速度的点积对齐度（alignment），当新事件对齐度明显更好时会替换当前事件。阈值与权重用于避免抖动。  
  - 对 contact 点合并：以位置距离阈值去重并最多保留两个接触点，深度取较大值。法线在合并时根据对齐度加权或直接平均，合并后再做规范化与夹紧。
- 事件过滤与回调顺序：  
  - 在触发回调前会对参与两端 token 调用 `ObjManager::IsValid` 验证仍然有效（防止使用已销毁/复用 token）。  
  - 使用 token-based 的 `operator[]` 获取 `BaseObject&` 并直接调用 `OnCollisionState`（Enter / Stay）；对于上一帧存在但本帧消失的对触发 Exit（传入空的 `CF_Manifold{}`）。系统不保证对 a/b 的回调先后顺序，回调应对可能的集合修改保持鲁棒性（例如避免立刻 delete，而应使用 `ObjManager::Destroy` 延迟销毁）。
- 内存/容器复用：实现通过复用 `grid_` 的桶容器、对 `entries_` 采用尾部移动删除来避免 O(n) 删除成本，并维护 `token_map_` 保持索引一致性。

## 可观察的成员与辅助行为（供调试与扩展参考）
- `world_shapes_`：每帧为 `entries_` 缓存的 world-space shape（长度与 entries_ 同步）。  
- `events_`：narrowphase 产生的原始碰撞事件列表（合并前）。  
- `merged_map_` / `merged_order_`：用于合并重复 contact 并保持输出顺序的临时结构。  
- `prev_collision_pairs_` / `current_pairs_`：用于比较前后帧碰撞对以产生 Exit 事件。  
- 在启用 `COLLISION_DEBUG` 时，会有额外的形状 dump 与接触点/法线的可视化绘制辅助。

## 使用建议与性能调优
- 调整 `cell_size` 改变 broadphase 粒度：过小会导致桶数量激增，过大会增加 narrowphase 对数；根据场景对象密度调优。  
- 若能在上层保证形状直接以 world-space 提供（例如固定不随每帧变化或由外部维护），可开启 `BasePhysics::enable_world_shape(true)` 减少每帧的坐标变换开销。  
- 在回调中避免立即删除对象；使用 `ObjManager::Destroy` 将销毁请求入队在安全点统一执行。  
- 若需深度调试，开启 `COLLISION_DEBUG` 可打印形状/接触点并绘制法线辅助排查。

## 常见边界/稳定性处理说明
- 所有由 `cf_collide` 返回的 manifold 都会经过守护性处理以消除 NaN/Inf 和负 depth，限制 contact 数量上限并归一化法线，从而避免上层逻辑受异常数值影响。  
- 对象在 narrowphase 参与前会通过 `ObjManager::IsValid` 验证，回调前也会再次验证，减少 token 有效性问题导致的 UB。