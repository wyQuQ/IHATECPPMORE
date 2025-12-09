# BaseObject

## 概述  
`BaseObject` 是面向使用者的游戏对象基类，整合渲染（`PngSprite`）与物理（`BasePhysics`）功能，并提供生命周期钩子与碰撞回调。设计假定在单线程主循环中使用，建议通过 `ObjManager` 管理对象生命周期以获得安全的延迟创建/销毁与 token 支持。

## 主要职责
- 提供生命周期钩子：`Start()`（初始化）、`FramelyApply()`（每帧物理积分与调试）、`Update()`（每帧逻辑）、`OnDestroy()`（销毁前清理）。
- 暴露渲染接口：控制贴图来源、帧率、翻转、旋转、枢轴与缩放，并能返回当前帧像素（`PngFrame`）供渲染器使用。
- 暴露物理接口：位置/速度/力、碰撞形状设置与读取（`CF_ShapeWrapper`）、碰撞类型控制。
- 提供碰撞回调分发：统一的 `OnCollisionState` 会根据阶段分发至 `OnCollisionEnter/Stay/Exit`。
- 支持对象标签（`AddTag`/`HasTag`/`RemoveTag`）以便快速分类或筛选。

## 使用契约与要点
- 对象应通过 `ObjManager::Create(...)` 创建以获得 pending token；真实 token 会在下一次 `ObjManager::UpdateAll()` 的提交阶段生成。直接使用 `new`/`delete` 不会自动参与 ObjManager 管理与物理注册，通常不推荐。
- `FramelyApply()` 在每帧安全点被调用：调用顺序通常由 `ObjManager::UpdateAll()` 控制，行为包括调用 `ApplyForce()`、`ApplyVelocity()`、可选调试绘制与记录上一帧位置。
- 精灵与碰撞同步：
  - 若启用 `IsColliderRotate()` 或 `IsColliderApplyPivot()`，`BaseObject` 会把精灵的旋转/枢轴同步到碰撞器，并调用 `BasePhysics::enable_world_shape(true)` 以减少重复变换开销。
  - 可根据性能/行为需要选择关闭这些同步开关，从而由上层手动维护 world-space 形状。

## 常用 API 概览
- 渲染相关
  - `void SpriteSetSource(const std::string& path, int count, bool set_shape_aabb = true)`：设置贴图（垂直帧数为 `count`），可选基于贴图设置默认 AABB 形状。
  - `void SpriteClearPath()`、`bool SpriteHasPath(std::string* out_path = nullptr)`。
  - `PngFrame SpriteGetFrame() const`：获取当前帧像素。
  - `void SpriteSetUpdateFreq(int)` / `int SpriteGetUpdateFreq() const`：设置动画更新间隔（以游戏帧为单位）。
  - 变换/显示：`SetRotation` / `Rotate`、`SpriteFlipX/Y`、`ScaleX/ScaleY/Scale`、`SetPivot` / `GetPivot`、`SetVisible` / `IsVisible`、`SetDepth` / `GetDepth`。
- 物理与形状
  - 位置/运动：`GetPosition`/`SetPosition`、`GetVelocity`/`SetVelocity`、`GetForce`/`SetForce`、`ApplyVelocity(dt)`、`ApplyForce(dt)`。
  - 形状：`SetShape(const CF_ShapeWrapper&)`、`GetShape()`（返回 world-space，可能触发转换）。
  - 便捷形状：`SetAabb`、`SetCircle`、`SetCapsule`、`SetPoly`、`SetCenteredAabb`、`SetCenteredCircle`、`SetCenteredCapsule`、`SetPolyFromLocalVerts`。
  - 碰撞类型：`SetColliderType(ColliderType)`、`GetColliderType()`。
  - 上一帧位置：`GetPrevPosition()`（用于连续碰撞检测或运动插值）。
- 碰撞回调
  - `virtual void OnCollisionEnter(const ObjManager::ObjToken& other, const CF_Manifold& manifold) noexcept`
  - `virtual void OnCollisionStay(const ObjManager::ObjToken& other, const CF_Manifold& manifold) noexcept`
  - `virtual void OnCollisionExit(const ObjManager::ObjToken& other, const CF_Manifold& manifold) noexcept`
  - `OnCollisionState` 会根据阶段（Enter/Stay/Exit）分发到上述方法；`Exit` 阶段的 manifold 可能为空。

## 示例用法（概念性）
- 在派生类中覆写 `Start()` 初始化资源与碰撞形状，在 `Update()` 中实现行为逻辑，在 `OnCollisionEnter` 中处理碰撞事件。在创建对象时通过 `ObjManager::Create<T>(...)` 取得 token 并交由 ObjManager 管理。

## 实现备注与最佳实践
- `BaseObject` 公开继承 `BasePhysics`（以直接复用物理接口），私有继承 `PngSprite`（以封装渲染细节并统一对外 API）。因此当修改精灵的旋转/枢轴/缩放时应考虑是否同步到物理层。
- 建议：
  - 在主循环中按顺序调用 `ObjManager::UpdateAll()` → `DrawingSequence::DrawAll()` → 渲染 `BlitAll()`，以确保当帧的逻辑变更被及时上传并显示。
  - 在碰撞回调中避免直接 delete 对象；应使用 `ObjManager::Destroy(token)` 以在安全点完成销毁。
  - 在性能敏感路径下，可以关闭 `IsColliderRotate()` / `IsColliderApplyPivot()` 并手动维护 world-space 形状以避免重复计算。