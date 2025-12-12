# 总览：核心模块与典型帧流程

## 核心模块
- `ObjManager`：对象集中管理、延迟创建/延迟销毁与句柄（`ObjToken`）系统，负责在安全点合并 pending 创建并执行延迟销毁。
- `BaseObject`：游戏对象基类，整合 `PngSprite`（渲染）与 `BasePhysics`（物理），并提供帧级物理入口 `FrameEnterApply()` / `FrameExitApply()`、生命周期钩子与碰撞回调。
- `PngSprite`：PNG 资源加载、帧拆分与动画时序。
- `BasePhysics`：位置/速度/力 与 local↔world 形状转换与 world-shape 缓存。
- `PhysicsSystem`：broadphase/narrowphase、碰撞检测与 Enter/Stay/Exit 回调分发。
- `DrawingSequence`：渲染上传与呈现流水线（`DrawAll()` / `BlitAll()`）。

## 典型帧流程（推荐顺序）
1. `ObjManager::UpdateAll()`（每帧主更新入口，含物理推进与 pending 合并）  
   - 对每个已合并且活跃的对象调用 `FrameEnterApply()`：缓存上一帧位置（供连续碰撞检测/插值）、调用 `ApplyForce()` 与 `ApplyVelocity()`（APPLIANCE 接口，框架会在适当时机自动调用；仅在需要子步时手动调用）。  
   - 调用物理系统步进（例如 `PhysicsSystem::Step()`），执行碰撞检测并分发 `OnCollisionEnter/Stay/Exit`。  
   - 对每个已合并对象调用 `Update()`（游戏逻辑/行为）。  
   - 处理延迟销毁队列（调用对象 `OnDestroy()` 并从物理系统注销）。  
   - 提交并合并本帧的 `pending_creates_`：为 pending 对象分配槽位、在物理系统中注册并写回真实 `ObjToken`（`Create` 返回的是 pending token；`Start()` 已在创建时被调用，真实 token 在提交阶段产生）。  
   - 在合并与销毁之后（进入帧尾阶段）对对象调用 `FrameExitApply()`：合并 `SetPosition(..., buffer=true)` 的缓冲目标位置、调用 `EndFrame()`、以及可选的调试绘制（`DebugDraw()`）。  

2. `DrawingSequence::DrawAll()`（上传阶段）  
   - 遍历已注册对象，基于最新状态（可见性、当前帧像素、pivot、rotation、scale、depth）准备并上传渲染数据。  

3. Rendering / `DrawingSequence::BlitAll()`（呈现阶段）  
   - 按深度/顺序将已上传的画布渲染到屏幕。

## 设计理由与注意点
- 将 `UpdateAll()` 放在 `DrawAll()` 之前保证当帧逻辑变更（新建对象、位置/帧/贴图变更等）能在同一帧的上传阶段生效，避免延后一帧显示。
- `ObjManager` 采用 pending 创建（`CreateEntry` 将对象放入 `pending_creates_` 并立即调用 `Start()`），真实注册发生在下一次 `UpdateAll()` 的提交阶段；`ObjToken` 使用 `(index,generation)` 防止槽位复用导致悬挂引用。
- `APPLIANCE` 标注的方法涉及每帧物理推进，框架会自动在合适时机调用；仅在需要手动子步时使用。
- `BaseObject` 在旋转/缩放/pivot 与碰撞体之间提供同步开关（`IsColliderRotate()` / `IsColliderApplyPivot()`）；根据性能/语义权衡可关闭以手动维护 world-space 形状。
- 建议所有创建/销毁与帧更新在主线程（游戏主循环）完成；跨线程访问资源加载需在适当同步点将资源与主线程关联与注册。

## 典型误区（快速提示）
- 不要在碰撞回调中直接 `delete` 对象；应调用 `ObjManager::Destroy(token)` 以在安全点完成销毁。  
- 如果在一帧内多处设置位置，请使用 `SetPosition(..., buffer=true)`，以避免中间状态影响物理求解；合并在 `FrameExitApply()` 执行。  
- `Create(...)` 返回 pending token；若需在创建后马上以真实 token 访问对象，需等到下一次 `UpdateAll()` 提交后使用 `TryGetRegisteration` 。