# BaseObject

## 概述
`BaseObject` 将渲染表示 `CF_Sprite` 与物理表示 `BasePhysics` 组合，负责每帧的物理推进、碰撞缓存与渲染状态同步。文档按功能分类列出所有未标记为“不推荐使用”的公开接口。

## 生命周期与帧控制
- `Start()`：对象添加到场景后由引擎调用，供派生类加载资源或初始化状态。
- `StartFrame()`：每帧 `FrameEnterApply` 之前调用，用于派生类执行前置逻辑。
- `FrameEnterApply()`：引擎在每帧物理子阶段调用，清空 `m_collide_manifolds`、执行 `StartFrame`，依次应用力与速度。
- `ApplyForce(float dt = 1)`：将当前力按照给定 `dt` 乘量应用到速度，主要供手动子步或调试使用。
- `ApplyVelocity(float dt = 1)`：将当前速度按照给定 `dt` 推进位置，同样为可选手动推进用法。
- `Update()`：每帧逻辑更新入口，由场景主循环调用。
- `EndFrame()`：每帧 `FrameExitApply` 之前调用，供派生类同步状态或清理。
- `FrameExitApply()`：引擎在帧尾调用，合并 buffered 位置、记录 `m_prev_position` 并调用 `EndFrame()`。

## 精灵与渲染控制
- `SpriteSetSource(const std::string& path, int vertical_frame_count, bool set_shape_aabb = true)`：切换纹理路径、帧数并可选依据帧尺寸更新 AABB。
- `SpriteSetStats(const std::string& path, int vertical_frame_count, int update_freq, int depth, bool set_shape_aabb = true)`：组合设置精灵资源、帧率与深度。
- `SpriteSetUpdateFreq(int update_freq)`：设置精灵播放频率（每若干帧切换一次动画帧）。
- `SpriteWidth()`：返回当前精灵宽度（像素）。
- `SpriteHeight()`：返回当前精灵单帧高度（像素）。
- `SetVisible(bool v)`：控制渲染可见性标志。
- `IsVisible()`：查询当前是否应参与渲染。
- `SetDepth(int d)`：设置渲染深度。
- `GetDepth()`：获取当前渲染深度。
- `SpriteGetFlipX()`：询问精灵是否水平翻转。
- `SpriteGetFlipY()`：询问精灵是否垂直翻转。
- `SpriteFlipX(bool x)`：按照参数显式设置水平翻转。
- `SpriteFlipY(bool y)`：按照参数显式设置垂直翻转。
- `SpriteFlipX()`：切换当前水平翻转状态。
- `SpriteFlipY()`：切换当前垂直翻转状态。
- `GetScaleX()`：获取精灵 X 方向缩放。
- `GetScaleY()`：获取精灵 Y 方向缩放。
- `ScaleX(float sx)`：设置 X 方向缩放并同步到物理。
- `ScaleY(float sy)`：设置 Y 方向缩放并同步到物理。
- `Scale(float s)`：以均匀比例设置缩放并同步到物理。
- `GetRotation()`：返回当前渲染旋转（弧度，保持在 `[-π,π]`）。
- `SetRotation(float rot)`：设置渲染旋转并根据 `IsColliderRotate` 同步碰撞体。
- `Rotate(float drot)`：在当前旋转基础上增量旋转。
- `GetPivot()`：返回当前渲染 pivot（偏移）。
- `SetPivot(float x_rel, float y_rel)`：按相对中心比例设置 pivot，必要时通过 `TweakColliderWithPivot` 与物理同步。
- `const CF_Sprite& GetSprite() const`：获取精灵的只读引用。
- `CF_Sprite& GetSprite()`：获取精灵的可写引用。

## 位移与物理状态
- `SetPosition(const CF_V2& p)`：立即更新位置。
- `SetVelocity(const CF_V2& v)`：立即设置速度。
- `SetVelocityX(float vx)`：设置 X 分量速度。
- `SetVelocityY(float vy)`：设置 Y 分量速度。
- `SetForce(const CF_V2& f)`：设置作用力。
- `SetForceX(float fx)`：设置 X 分量力。
- `SetForceY(float fy)`：设置 Y 分量力。
- `AddVelocity(const CF_V2& dv)`：在当前速度上叠加增量。
- `AddForce(const CF_V2& df)`：在当前力上叠加增量。
- `GetPosition()`：读取当前位置。
- `GetPrevPosition()`：读取上一帧位置。
- `GetVelocity()`：读取当前速度。
- `GetForce()`：读取当前力。
- `ApplyForce(float dt = 1)`：详见前文，每帧默认自动调用。
- `ApplyVelocity(float dt = 1)`：详见前文，每帧默认自动调用。

## 碰撞与排斥控制
- `SetColliderType(ColliderType t)`：设置对象的碰撞类别（SOLID/ACTOR 等）。
- `IsColliderRotate()`：查询是否同步角度给碰撞体。
- `IsColliderRotate(bool v)`：设置是否同步角度给碰撞体并更新 world shape 标志。
- `IsColliderApplyPivot()`：查询是否应用 pivot 到碰撞体。
- `IsColliderApplyPivot(bool v)`：设置是否应用 pivot 并更新 world shape 标志。
- `ExcludeWithSolids(bool v)`：开启/关闭与 SOLID 的排斥处理逻辑。
- `IsCollidedWith(const BaseObject& other, CF_Manifold& out_m)`：直接测试两个对象当前 shape 是否重叠，并输出碰撞信息。
- `CollisionPhase`：枚举 `Enter/Stay/Exit`，用于 `OnCollisionState` 的状态分发。
- `OnCollisionState(const ObjManager::ObjToken& other, const CF_Manifold& manifold, CollisionPhase phase)`：统一处理碰撞阶段，必要时先执行 `ExcludeWithSolid` 逃避，再调用相应回调并缓存 manifold。
- `OnCollisionEnter(const ObjManager::ObjToken& other, const CF_Manifold& manifold)`：首次碰撞回调。
- `OnCollisionStay(const ObjManager::ObjToken& other, const CF_Manifold& manifold)`：维持碰撞回调。
- `OnCollisionExit(const ObjManager::ObjToken& other, const CF_Manifold& manifold)`：碰撞结束回调。
- `OnExclusionSolid(const ObjManager::ObjToken& other, const CF_Manifold& manifold)`：排斥逻辑执行后的通知，供派生类处理后续状态。

## 碰撞形状构造
- `SetCenteredAabb(float half_w, float half_h)`：以局部中心为原点创建轴对齐包围盒并同步形状。
- `SetCenteredCircle(float radius)`：创建局部圆形形状。
- `SetCenteredCapsule(const CF_V2& dir, float half_length, float radius)`：以方向向量定义的胶囊形状，支持自动归一化与端点排序。
- `SetCenteredPoly(const std::vector<CF_V2>& localVerts)`：通过局部顶点数据构造多边形。

## 标签与管理
- `AddTag(const std::string& tag)`：为对象添加标签，重复添加无影响。
- `HasTag(const std::string& tag)`：检查标签是否存在。
- `RemoveTag(const std::string& tag)`：移除已有标签。
- `GetObjToken()`：派生类可调用获取在 `ObjManager` 中注册的 token，用于安全访问。

## 调试辅助
- `ShapeDraw()`：在启用 `SHAPE_DEBUG` 时调用全局调试绘制函数。
- `ManifoldDraw(const CF_Manifold& m)`：在启用 `COLLISION_DEBUG` 时绘制碰撞 `manifold`。