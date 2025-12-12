# BaseObject

## 概述  
`BaseObject` 是面向使用者的游戏对象基类，整合渲染（`PngSprite`）与物理（`BasePhysics`）功能，并提供生命周期钩子、帧级物理入口与碰撞回调。设计假定在单线程主循环中使用，建议通过 `ObjManager` 管理对象生命周期以获得安全的延迟创建/销毁与 token 支持。构造函数会设置物体默认物理/渲染状态并为标签集合预留空间。

## 主要职责
- 提供生命周期钩子：`Start()`（初始化）、`FrameEnterApply()`（帧首物理子步与缓存上一帧位置）、`Update()`（每帧逻辑）、`FrameExitApply()`（帧尾位置合并/清理）与 `OnDestroy()`（销毁前清理）。此外提供 `StartFrame()` / `EndFrame()` 供派生类在帧首/帧尾扩展。
- 暴露渲染接口：控制贴图来源、帧率、翻转、旋转、枢轴与缩放，并能返回当前帧像素（`PngFrame`）供渲染器使用。
- 暴露物理接口（只读或受控写入）：位置/速度/力、碰撞形状设置与读取（`CF_ShapeWrapper`）、碰撞类型控制。
- 提供碰撞回调分发：统一的 `OnCollisionState` 会根据阶段分发至 `OnCollisionEnter/Stay/Exit`。
- 支持轻量标签（`AddTag`/`HasTag`/`RemoveTag`）以便快速分类或筛选（非线程安全）。

## 使用契约与要点
- 对象应通过 `ObjManager::Create(...)` 创建以获得 pending token；真实 token 会在下一次 `ObjManager::UpdateAll()` 的提交阶段生成。直接使用 `new`/`delete` 不会自动参与 ObjManager 管理与物理注册，通常不推荐。
- APPLIANCE 标记：若方法被标注为 `APPLIANCE`，说明该方法涉及每帧物理推进/内部完成的物理步骤，框架会在正确时机自动调用。除非需要在单帧内多次推进物理子步，否则不要手动调用这类接口。
- 帧级物理更新：
  - `FrameEnterApply()`：缓存上一帧位置（用于连续碰撞检测/插值/轨迹），然后按顺序调用 `ApplyForce()`、`ApplyVelocity()`（每步可传 dt）。
  - `FrameExitApply()`：如果在本帧调用过 `SetPosition(..., buffer=true)`，此处会合并目标位置并应用（策略：在 X/Y 上分别选择与当前坐标差绝对值最大的偏移）；随后调用 `EndFrame()` 并进行可选的调试绘制。
- SetPosition 的 `buffer=true` 模式可用于在一帧内多处设置目标位置而避免中途影响物理求解，合并策略如上所述。
- 精灵与碰撞同步：当启用 `IsColliderRotate()` 或 `IsColliderApplyPivot()` 时，`BaseObject` 会把精灵的旋转/枢轴同步到碰撞器，并根据开关调用 `BasePhysics::enable_world_shape(...)` 以控制是否维护 world-space 形状（性能/语义权衡）。

## 常用 API 概览
- 渲染相关
  - `void SpriteSetSource(const std::string& path, int count, bool set_shape_aabb = true) noexcept`：设置贴图路径与垂直帧数，`set_shape_aabb` 可在加载时基于图像大小设置默认 AABB（若实现支持）。
  - `void SpriteClearPath() noexcept`、`bool SpriteHasPath(std::string* out_path = nullptr) const noexcept`。
  - `PngFrame SpriteGetFrame() const`：获取当前帧像素（若精灵有垂直帧，会返回按垂直帧数选择的帧）。
  - `void SpriteSetUpdateFreq(int)` / `int SpriteGetUpdateFreq() const noexcept`：设置/获取帧更新频率（帧延迟，freq <= 0 会被设为 1）。
  - 变换/显示：`SetRotation(float)`（会将角度限定到 [-pi,pi] 并在 `IsColliderRotate()` 打开时同步到物理并标记 world shape 脏）、`Rotate(float)`、`SpriteFlipX/Y`、`ScaleX/ScaleY/Scale`（会同步到底层 `BasePhysics`）、`SetPivot(float x_rel, float y_rel)`（内部将缩放临时设为 1 以准确计算像素 pivot；若 `IsColliderApplyPivot()` 为真，会对碰撞体做偏移并同步到 `BasePhysics`）、`GetPivot()`、`SetVisible`/`IsVisible`、`SetDepth`/`GetDepth`。
  - `int SpriteWidth() const noexcept` / `int SpriteHeight() const noexcept`：返回当前帧像素级宽高（考虑缩放后并以 int 返回）。
- 物理与形状
  - 位置/运动：`const CF_V2& GetPosition() const noexcept`、`void SetPosition(const CF_V2& p, bool buffer = false) noexcept`、`const CF_V2& GetPrevPosition() const noexcept`、`GetVelocity/SetVelocity`、`GetForce/SetForce`、`AddVelocity/AddForce`、`APPLIANCE void ApplyVelocity(float dt = 1) noexcept`、`APPLIANCE void ApplyForce(float dt = 1) noexcept`。
  - 形状：受控低级 API（标注 `BARE_SHAPE_API`，不推荐常用）：`SetShape(const CF_ShapeWrapper&)`、`SetAabb`、`SetCircle`、`SetCapsule`、`SetPoly`。
  - 另外提供便捷的以中心为原点的形状构造器：`SetCenteredAabb`、`SetCenteredCircle`、`SetCenteredCapsule`、`SetCenteredPoly`。
  - 碰撞类型：`void SetColliderType(ColliderType) noexcept`、`ColliderType GetColliderType() const noexcept`。
- 标签（非线程安全）
  - `void AddTag(const std::string& tag) noexcept`、`bool HasTag(const std::string& tag) const noexcept`、`void RemoveTag(const std::string& tag) noexcept`。
  - 内部在构造时对标签容器做了 `reserve(5)` 以降低小对象分配开销。
- 其他
  - `const ObjManager::ObjToken& GetObjToken() const noexcept`（受保护：派生类可读取自身 token）。
  - `void OnCollisionState(const ObjManager::ObjToken& other, const CF_Manifold& manifold, CollisionPhase phase) noexcept`：统一分发 collision phase（Enter/Stay/Exit）到对应回调。

## 碰撞回调
- `virtual void OnCollisionEnter(const ObjManager::ObjToken& other, const CF_Manifold& manifold) noexcept`
- `virtual void OnCollisionStay(const ObjManager::ObjToken& other, const CF_Manifold& manifold) noexcept`
- `virtual void OnCollisionExit(const ObjManager::ObjToken& other, const CF_Manifold& manifold) noexcept`
- `OnCollisionState` 会根据 `CollisionPhase`（Enter=1, Stay=0, Exit=-1）分发到上述方法。注意：`Exit` 阶段传入的 manifold 可能为空/不完全，具体语义由碰撞系统决定。

## 实现备注与最佳实践
- 继承模型：`BaseObject` 公开继承自 `BasePhysics`（以复用物理实现），私有继承 `PngSprite`（封装渲染实现细节）。类内部通过 `using` 将 `BasePhysics` 的若干方法设为私有以防派生类直接误用底层接口（以保证同步与脏标记逻辑一致性）。
- Pivot/缩放/碰撞协同：
  - `SetPivot` 会临时把缩放设为 1 以精确计算像素级 pivot，若启用 `IsColliderApplyPivot()`，会调用 `TweakColliderWithPivot` 并同步 pivot 到 `BasePhysics`，随后恢复缩放并标记 world shape 脏。
  - `ScaleX/ScaleY/Scale` 会同时调用 `PngSprite` 与 `BasePhysics` 的缩放接口，保持渲染与碰撞一致。
- 旋转策略：
  - `SetRotation` 会将角度正规化到 [-pi,pi]。若 `IsColliderRotate()` 为真，会把 sprite 的旋转同步到底层物理并标记 world shape 脏。
- 性能建议：
  - 在性能敏感路径下，可以关闭 `IsColliderRotate()` 和/或 `IsColliderApplyPivot()`，手动维护 world-space 形状以避免重复计算。
  - 使用 `SetPosition(..., buffer=true)` 在一帧内多次设置目标位置以减少对中间物理状态的干扰。
- 资源与销毁：
  - 在碰撞回调或游戏逻辑中避免直接 `delete` 对象；应通过 `ObjManager::Destroy(token)` 在安全点销毁对象。
  - `OnDestroy()` 提供派生类在被销毁前释放资源的钩子。

## 示例用法（概念性）
- 在派生类中覆写 `Start()` 初始化资源与默认碰撞形状，在 `Update()` 中实现行为逻辑，在 `OnCollisionEnter` 中处理碰撞事件。在创建对象时通过 `ObjManager::Create<T>(...)` 取得 pending token 并交由 `ObjManager` 管理，必要时使用 `TryGetRegisteration` / token 解析来获取真实 token。具体例子参看objects/目录下的一些实现。