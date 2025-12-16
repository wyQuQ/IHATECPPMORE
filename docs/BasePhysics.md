# BasePhysics

## 概述  
提供了物理实体的基础状态（位置、速度、力）、形状缓存与变换管理。设计为单线程、无锁、noexcept，强调 world-space 缓存的惰性更新与版本检测。

## 状态接口
- `set_position/velocity/force` 与 `add_*` 直接修改状态，部分修改会把 world shape 与 `position_dirty_` 标记为脏。
- `apply_velocity(dt)` 依据当前速度积分位置；`apply_force(dt)` 把力积累到速度。

## 形状与变换
- `set_shape` 保存 local-space wrapper，`get_shape` 在 `world_shape_dirty` 时调用 `tweak_shape_with_rotation` 生成 world-space shape，并维护版本号。  
- `set_rotation/scale_x/scale_y/set_pivot` 会把缓存标记为脏；角度归一到 [-π,π]。  
- `enable_world_shape(true)` 表示上层直接维护 world-space shape，可以跳过转换，`force_update_world_shape` 强制刷新。  
- `world_shape_version` + `mark_world_shape_dirty` 供上层缓存一致性检测。

## 位置/脏标记
- `is_position_dirty`/`clear_position_dirty` 便于管理移动过的实体；`get_local_shape` 在不需要 world 转换时直接访问。

## 内部结构
- `tweak_shape_with_rotation`（定义在 cpp）负责根据 position/rotation/pivot/scale 生成最终 world shape，并递增 `world_shape_version_`。  
- `cached_world_shape_` 使用 mutable，以便在 const 上层接口中 lazy update。