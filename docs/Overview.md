# 总览：核心模块与典型帧流程

## 核心模块
- `ObjManager`：对象集中管理、延迟创建/延迟销毁与句柄（`ObjToken`）系统，负责在安全点合并 pending 创建并执行延迟销毁。
- `BaseObject`：游戏对象基类，整合 `CF_Sprite`（渲染）与 `BasePhysics`（物理），并提供帧级物理入口 `FrameEnterApply()` / `FrameExitApply()`、生命周期钩子与碰撞回调。
- `BasePhysics`：位置/速度/力 与 local↔world 形状转换与 world-shape 缓存。
- `PhysicsSystem`：broadphase/narrowphase、碰撞检测与 Enter/Stay/Exit 回调分发。
- `DrawingSequence`：渲染上传流水线，负责收集所有可见 `BaseObject` 并按深度/注册顺序推送到渲染批次。

## 典型帧流程（推荐顺序）
1. `ObjManager::UpdateAll()`（每帧主更新入口，含物理推进与 pending 合并）  
   - 对每个已合并且活跃的对象调用 `FrameEnterApply()`：清空本帧的 `m_collide_manifolds`、调用派生 `StartFrame()`、缓存上一帧位置以支持插值/调试、再调用 `ApplyForce()` 与 `ApplyVelocity()`（APPLIANCE 接口，框架会在适当时机自动调用；仅在需要子步时手动调用）。  
   - 调用物理系统步进（如 `PhysicsSystem::Step()`），执行碰撞检测并分发 `OnCollisionEnter/Stay/Exit`。  
   - 对每个已合并对象调用 `Update()`（游戏逻辑/行为）。  
   - 再次遍历活跃对象调用 `FrameExitApply()`：合并可能的 buffered 目标位置、调用 `EndFrame()`、记录上一帧位置并可选执行调试绘制（`DebugDraw()`）。  
   - 处理延迟销毁队列（调用对象 `OnDestroy()` 并从物理系统注销）；`skip_update_this_frame` 标志可用来让对象在本帧跳过以上更新/物理调用。  
   - 提交并合并本帧的 `pending_creates_`：为 pending 对象分配槽位、在物理系统注册、写回真实 `ObjToken`，并在注册后开始参与下一帧的 UpdateAll 调用。  

2. `DrawingSequence::DrawAll()`（上传阶段）  
   - 在 `DrawingSequence` 内部锁住列表，按 `BaseObject::GetDepth()` 与注册顺序排序。  
   - 对每个可见对象：复制其 `CF_Sprite`、调用 `cf_sprite_update`、设置 sprite 位置与 transform、通过 `PushFrameSprite` 提交到批次、调用 `ShapeDraw()`（若启用 debug）以及遍历 `m_collide_manifolds` 执行 `ManifoldDraw()`。  
   - 根据 `m_sprite_update_freq` 与全局帧计数更新 `m_sprite_current_frame_index`，实现垂直雪碧图的动画帧。
   - 不再有单独的 `BlitAll()`：`DrawingSequence::DrawAll()` 直接在内部准备好提交数据，最终由渲染管线在合适阶段消费。

3. 渲染阶段（由底层渲染器/引擎继续处理）  
   - 引擎按 `DrawingSequence` 汇总的批次在 GPU/画布上绘制所有纹理，无需文档中描述具体接口。  

## 设计理由与注意点
- 将 `UpdateAll()` 放在 `DrawAll()` 之前保证当帧逻辑变更（新建对象、位置/帧/贴图变更等）能在同一帧的上传阶段生效，而无需后移到下一帧。  
- `ObjManager` 采用 pending 创建（`CreateEntry` 将对象放入 `pending_creates_` 并立即调用 `Start()`），真实注册发生在下一次 `UpdateAll()` 的提交阶段；`ObjToken` 使用 `(index,generation)` 防止槽位复用导致悬挂引用。  
- `APPLIANCE` 标注的方法涉及每帧物理推进，框架会自动在合适时机调用；仅在需要手动子步时使用。  
- `BaseObject` 在旋转/缩放/pivot 与碰撞体之间提供同步开关（`IsColliderRotate()` / `IsColliderApplyPivot()`）；根据性能/语义权衡可选择关闭以手动维护 world-space 形状。  
- 建议所有创建/销毁与帧更新在主线程（游戏主循环）完成；跨线程访问资源加载需在适当同步点将资源与主线程关联与注册。  
- `DrawingSequence` 所有对象都会在 DrawAll 中处理 sprite 更新、碰撞调试绘制与帧动画，无需单独管理 `PngSprite` 或其他高层封装。  

## 典型误区（快速提示）
- 不要在碰撞回调中直接 `delete` 对象；应调用 `ObjManager::Destroy(token)` 以在安全点完成销毁。  
- `Create(...)` 返回 pending token；若需在创建后马上以真实 token 访问对象，需等到下一次 `UpdateAll()` 提交后使用 `TryGetRegisteration`。  