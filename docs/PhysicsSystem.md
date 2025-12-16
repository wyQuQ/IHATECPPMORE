# PhysicsSystem

## 概述  
独立单例的碰撞子系统，负责 broadphase 网格划分、narrowphase 碰撞检测、contact 合并、Enter/Stay/Exit 事件分发。`ObjManager::UpdateAll` 会在 Step 的合适阶段调用 `Step()`，使 `BaseObject::OnCollisionState` 收到每帧碰撞通知。

## 主要数据
- `Entry`：记录 token、`BasePhysics*` 指针、grid 坐标与 dirty 标志，分为 dynamic/static 两类以支持不同生命周期。  
- `grid_` 使用 `grid_key(x,y)` 生成桶，`grid_keys_used_` 用于清理每帧用过的 bucket。  
- `world_shapes_`、`events_`、`merged_map_`/`merged_order_`、`current_pairs_` 等临时容器用于缓存世界空间形状、合并 manifold 与跟踪当前碰撞对。  
- `prev_collision_pairs_` 记录上一帧 pairs（用于 Exit），“pair key” 基于 token 编码。  

## Step 函数执行流程
1. `events_` 清理后；若没有动态/静态条目直接返回。  
2. 清空 `grid_keys_used_` 中记录的 bucket，并 resize `world_shapes_` 以容纳所有条目。  
3. 通过 `update_entry_in_grid` 将 dynamic/static 条目遍历一次：若 `Entry::dirty` 则从 `BasePhysics::get_shape()` 获取本地形状，依据 `is_world_shape_enabled()` 决定是否需变换到 world space；之后调用 `shape_wrapper_to_aabb` 计算 AABB，将对象加入覆盖的 grid bucket 并记录 grid 坐标，同时在 world_shapes 缓存中保留最终 shape，最后清除 position dirty 标志。  
4. 对所有动态格子的邻区执行 narrowphase：遍历 candidate pair，调用 `shapes_collide_world`（内部执行 `cf_collide` 后再运行 `normalize_and_clamp_manifold`）获得 `CF_Manifold`；若产生碰撞则填充 `CollisionEvent`（计算 `distance_a/b` 便于排序）并推送 `events_`。  
5. `events_` 去重与排序：先以 `pair_key` 消除重复，对于 repeat pair 会通过 `merge_manifold_contact_points` 维持最多两个不同 contact；随后按照距离排序以便在回调顺序上更稳定。  
6. 遍历 `events_` 生成当前 pairs map，同时调用 `ObjManager::Instance().IsValid` 证明 token 有效；用 token-based 的 `operator[]` 获取对应 `BaseObject`，再使用 `orient_manifold` 让法线朝向接触对象，并依赖 `current_pairs_` 与 `prev_collision_pairs_` 判断调用 `OnCollisionState` 时的 `Enter`/`Stay` 相位。  
7. `prev_collision_pairs_` 中存在但 `current_pairs_` 缺失的 pair 将触发 `BaseObject::OnCollisionState` 的 `Exit` 回调；退出逻辑也验证 token 仍有效。  
8. `prev_collision_pairs_` 与 `current_pairs_` 交换，循环结束。  

## 注册与注销
- `Register(token, BasePhysics*)`/`Unregister(token)` 支持重复注册（更新指针），使用 `dynamic_token_map_` / `static_token_map_` 跟踪索引。  
- `make_key(token)` 将 `(index, generation)` 编码为 `uint64_t`，确保与 `ObjManager` token 匹配。  

## World-shape 与调试
- 若 `BasePhysics::is_world_shape_enabled()` 为 true，则直接使用 world-space 形状；否则 Step 会根据 position/scale/rotation/pivot 计算。  
- `normalize_and_clamp_manifold`、`merge_manifold_contact_points` 保证 manifold 数值稳定。  
- `COLLISION_DEBUG` 编译时可打印详细 shape/Exit 信息，`world_shapes_` 与 `grid_keys_used_` 等容器在 `Step` 内反复复用以减少分配。  - `CollisionEvent::distance_a/distance_b` 记录 penetration 信息，方便后续扩展（e.g. 物理反馈）。