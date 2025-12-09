# 总览：核心模块与典型帧流程

## 核心模块
- `ObjManager`：对象的集中管理、延迟创建/销毁与句柄 (`ObjToken`) 系统。
- `BaseObject`：游戏对象基类，整合 `PngSprite`（渲染）与 `BasePhysics`（物理）。
- `PngSprite`：PNG 资源加载、帧拆分与动画时序。
- `BasePhysics`：位置/速度/力 与 local↔world 形状转换（带缓存）。
- `PhysicsSystem`：broadphase/narrowphase、碰撞合并与 Enter/Stay/Exit 回调分发。
- `DrawingSequence`：渲染流水线管理（上传 DrawAll / 绘制 BlitAll）。

## 典型帧流程（推荐顺序）
1. ObjManager::UpdateAll()
   - 对每个活跃对象调用 `FramelyApply()`（执行 `ApplyForce` / `ApplyVelocity`、调试绘制、记录 prev position）。
   - 调用 `PhysicsSystem::Step()`：产生碰撞事件并分发到对象回调。
   - 为每个对象调用 `Update()`（游戏逻辑）。
   - 执行 pending 销毁。
   - 提交 pending 创建：合并 pending 对象到 `objects_`、注册到 `PhysicsSystem` 并写回真实 token。
2. DrawingSequence::DrawAll()（上传阶段）
   - 遍历注册对象并基于最新状态（可见性、帧、pivot、rotation、scale、depth）提取像素并上传到 per-owner canvas / GPU。
3. Rendering / DrawingSequence::BlitAll()（呈现阶段）
   - 按深度与注册顺序绘制已上传的 canvas 到屏幕。

## 设计理由
- 把 `UpdateAll()` 放在 `DrawAll()` 之前能保证当帧的逻辑更改（例如新创建对象、贴图或帧变化）会被包含在当帧的像素上传中，避免延后一帧才显示。
- world-shape 缓存（`BasePhysics`）与 broadphase 网格（`PhysicsSystem`）用于控制每帧的计算成本。
- `ObjToken` 的 (index, generation) 机制防止槽复用导致的悬挂引用。

## 注意事项
- 按约定，所有对象创建/销毁与帧更新应在主线程（游戏主循环）完成以避免并发问题。
- 若需要跨线程资源加载（如图片），需在上传阶段或合适的同步点整合资源并在主线程完成关联与注册。