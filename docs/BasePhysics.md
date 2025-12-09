# BasePhysics

## 概述  
`BasePhysics` 提供基础的物理状态与形状管理功能，供可碰撞实体（例如 `BaseObject`）继承使用。重点逻辑是 local-space ↔ world-space 的形状处理（包含旋转、枢轴、缩放），并以惰性缓存方式保存 world-space 形状以降低每帧计算开销。

## 主要职责
- 管理基础物理量：位置（position）、速度（velocity）、力（force），并提供积分辅助 `apply_velocity(dt)` / `apply_force(dt)`。
- 存储本地形状（`CF_ShapeWrapper`），提供 `get_shape()` 返回 world-space 形状（由 cache 驱动，必要时调用内部转换函数 `tweak_shape_with_rotation()`）。
- 提供变换控制：`set_rotation(float)`、`set_pivot(CF_V2)`、`scale_x/scale_y`。
- 提供 world-shape 优化开关：`enable_world_shape(bool)`。开启时表示上层已经提供 world-space 形状，可以跳过重复转换。

## 关键接口
- 状态控制：
  - `void set_position(const CF_V2&)`, `const CF_V2& get_position() const`
  - `void set_velocity(const CF_V2&)`, `const CF_V2& get_velocity() const`
  - `void set_force(const CF_V2&)`, `const CF_V2& get_force() const`
  - `void apply_velocity(float dt)`, `void apply_force(float dt)`
- 形状与变换：
  - `void set_shape(const CF_ShapeWrapper&)`（设置本地 shape，并标记脏）
  - `const CF_ShapeWrapper& get_shape() const`（返回 world-space shape，必要时做转换并更新缓存）
  - `void set_rotation(float)`, `float get_rotation() const`
  - `void set_pivot(const CF_V2&)`, `CF_V2 get_pivot() const`
  - `void scale_x(float)`, `void scale_y(float)`
  - `void enable_world_shape(bool)`、`bool is_world_shape_enabled() const`
  - `void force_update_world_shape()`（立即更新缓存）
  - `uint64_t world_shape_version() const`（版本号，用于上层缓存一致性检测）

## 实现与语义说明
- 惰性缓存：`get_shape()` 只有在 `world_shape_dirty_` 为 true 时才触发 `tweak_shape_with_rotation()` 更新 `cached_world_shape_` 与 `world_shape_version_`。
- 旋转/枢轴/缩放 的任何修改都会标记 world-shape 脏，从而保证下一次访问时得到正确的 world-space 形状。
- `enable_world_shape(true)` 可由上层在性能关键路径中使用，表示该层已直接维护 world-space 形状，物理系统将直接使用此 shape 而不做额外变换。
- 线程与异常：类设计为无锁且面向单线程游戏主循环使用；成员函数不抛异常以保证主循环稳定。