# 总览：核心模块与典型帧流程

## 核心模块
- `ObjManager`：对象集中管理、延迟创建/延迟销毁与句柄（`ObjToken`）系统，负责在安全点合并 pending 创建并执行延迟销毁。
- `BaseObject`：游戏对象基类，整合 `CF_Sprite`（渲染）与 `BasePhysics`（物理），并提供帧级物理入口 `FrameEnterApply()` / `FrameExitApply()`、生命周期钩子与碰撞回调。
- `BasePhysics`：位置/速度/力 与 local↔world 形状转换与 world-shape 缓存。
- `PhysicsSystem`：broadphase/narrowphase、碰撞检测与 Enter/Stay/Exit 回调分发。
- `DrawingSequence`：渲染上传流水线，负责收集所有可见 `BaseObject` 并按深度/注册顺序推送到渲染批次，内部使用缓存批量提交 `CF_Command` 以抑制瞬时大量 `spritebatch` 条目导致的 `Cute::Array` 容量爆炸。
- `GlobalPlayer`：全局玩家状态管理，包括复活点/出现点记录、实体创建与 `Hurt` 血迹生成等，供主循环、重生逻辑与 UI 查询。

## 典型帧流程（推荐顺序）
1. `ObjManager::UpdateAll()`（每帧主更新入口，含物理推进与 pending 合并）  
   - 对每个已合并且活跃的对象调用 `FrameEnterApply()`：清空本帧的 `m_collide_manifolds`、调用派生 `StartFrame()`、缓存上一帧位置以支持插值/调试、再调用 `ApplyForce()` 与 `ApplyVelocity()`（APPLIANCE 接口，框架会在适当时机自动调用；仅在需要子步时手动调用）。  
   - 调用物理系统步进（如 `PhysicsSystem::Step()`），执行碰撞检测并分发 `OnCollisionEnter/Stay/Exit`。  
   - 对每个已合并对象调用 `Update()`（游戏逻辑/行为）。  
   - 再次遍历活跃对象调用 `FrameExitApply()`：合并可能的 buffered 目标位置、调用 `EndFrame()`、记录上一帧位置并可选执行调试绘制（`DebugDraw()`）。  
   - 处理延迟销毁队列（调用对象 `OnDestroy()` 并从物理系统注销）；`skip_update_this_frame` 标志可用来让对象在本帧跳过以上更新/物理调用。  
   - 提交并合并本帧的 `pending_creates_`：为 pending 对象分配槽位、在物理系统注册、写回真实 `ObjToken`，并在注册后开始参与下一帧的 UpdateAll 调用。  

2. `DrawingSequence::DrawAll()`（帧图资源上传与渲染准备）  
   - `DrawAll()` 先加锁、重置 `last_image_id` 及 `s_pending_sprites` 缓存，确保每帧上下文干净。  
   - 按深度 + `reg_index` 对活跃对象排序，保持渲染顺序确定性。  
   - 每个可见对象：更新动画、同步位置、触发 UI 形状/碰撞回调，并调用 `PushFrameSprite()` 生成 `spritebatch_sprite_t`。  `PushFrameSprite` 使用当前 `s_draw->mvp` 计算几何，累积到 `s_pending_sprites`，当缓存达到 `kSpriteChunkSize` 时就通过 `FlushPendingSprites()` 封装为一个新的 `CF_Command`。  
   - `FlushPendingSprites()` 会在 `s_pending_sprites` 非空时创建 `CF_Command`、将条目逐个写入 `cmd.items`，然后清空缓存，为下一帧或下一个批次做好准备。  
   - 帧遍历完毕后再次调用 `FlushPendingSprites()`，确保残留条目被提交；最终，`app_draw_onto_screen` 会读取 `s_draw->cmds`，由 Cute 渲染管线遍历 `cmd.items` 并最终向屏幕提交图元。  

## 房间管理与对象生命周期
- `RoomLoader` 负责房间的加载/卸载逻辑，主程序通过该管理器与房间系统解耦，仅需告知 `StartRoom()` 所需房间名即可进行初始化。  
- 每个房间（派生自 `BaseRoom`）在编译期通过静态注册或 `rooms/*.cpp` 中的构造函数确保 `RoomLoader` 拥有对应的工厂，避免运行时反射或配置描述。  
- `RoomLoader::LoadRoom()` 会根据注册的类型分配房间实例、调用 `BaseRoom::OnEnter()` 及内部 `ObjManager::Create()`，由 `ObjManager` 的 pending 机制批量注册该房间内的游戏对象。  
- 房间中 `BaseObject` 派生类通过 `Start()`/`Update()`/`OnDestroy()` 生命周期钩子配合 `ObjManager` 与 `PhysicsSystem` 协同更新，更新与绘制逻辑依旧在每帧的 `ObjManager::UpdateAll()` 与 `DrawingSequence::DrawAll()` 中统一执行。  
- `RoomLoader::UnloadRoom()` 触发当前房间对象的统一销毁，委托 `ObjManager::Destroy()` 将需要在安全点完成的销毁排入队列，同时 `BaseRoom::OnExit()` 可执行资源释放或预设状态清理。  
- 通过 `RoomLoader` 提供的接口，主程序无需掌握具体房间类与对象细节，保持了解耦；房间切换仅需调整调用顺序与传参，而底层创建/更新/销毁仍受 `ObjManager` 与 `PhysicsSystem` 管理。  

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