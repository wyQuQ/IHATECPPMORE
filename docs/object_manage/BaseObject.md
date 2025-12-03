# BaseObject

## 目录
- [概述](#概述)
- [类 `BaseObject`](#类-baseobject)
  - [继承关系与职责概述](#继承关系与职责概述)
  - [构造函数 `BaseObject()`](#构造函数-baseobject)
  - [方法 `FramelyUpdate()`](#方法-framelyupdate)
  - [虚方法 `Start()` / `Update()`](#虚方法-start--update)
  - [方法 `GetSpriteFrame()`](#方法-getspriteframe)
  - [贴图路径管理（实现细节）](#贴图路径管理实现细节)
  - [可见性与绘制深度](#可见性与绘制深度)
  - [物理接入（委托到 `BasePhysics`）](#物理接入委托到-basephysics)
  - [生命周期与销毁辅助接口（实现细节）](#生命周期与销毁辅助接口实现细节)
  - [源文件依赖与副作用](#源文件依赖与副作用)
  - [私有成员变量](#私有成员变量)
- [设计要点与边界情况（摘要）](#设计要点与边界情况摘要)

该文档详尽介绍 `BaseObject` 类的职责、内部成员与每个公开方法的参数与实现意图。

---

类 `BaseObject` {
- 继承关系与职责概述 {
  - 继承：`class BaseObject : private BasePhysics, private PngSprite`
  - 作用：封装常用的游戏对象行为与属性（物理积分、贴图帧管理、可见性与深度），并提供统一的创建/销毁辅助接口以便与 `InstanceController` 协作。同时管理与 `DrawingSequence` 的注册/注销以参与帧上传与绘制流程。
  - 设计要点：把物理与贴图两类基础功能组合在对象内部，派生类通过覆盖 `Start()`/`Update()` 实现具体行为；对象负责将有贴图路径的实例注册到 `DrawingSequence`，以便 `DrawAll`/`BlitAll` 处理其 per-owner canvas。
}

, 构造函数 `BaseObject()` {
  - 整体作用：初始化 `BasePhysics` 与 `PngSprite`，并把位置、速度、力向量清零；同步 `PngSprite` 的帧延迟到对象层面的动画频率。
  - 参数：无。
  - 关键动作：
    - 调用基类构造：`BasePhysics()`、`PngSprite("", 1, 1)`（默认空路径与帧设定）。
    - 初始化物理量为零：`set_position`、`set_velocity`、`set_force`。
    - 通过 `SetFrameDelay(m_sprite_update_freq)` 将对象层面的 `m_sprite_update_freq` 同步给 `PngSprite`。
}

, 方法 `FramelyUpdate()` {
  - 整体作用：每帧统一驱动对象——先调用派生类的 `Update()`，再执行自动物理积分（力→速度→位置）。
  - 参数：无。
  - 语义细节：
    - `Update()` 为虚方法，派生类在其中实现每帧逻辑。
    - 随后调用 `ApplyForce()` 与 `ApplyVelocity()` 以完成物理更新（默认 dt = 1）。
    - 标记为 `noexcept`，保证每帧调用不会抛异常影响主循环稳定性。
}

, 虚方法 `Start()` / `Update()` {
  - `Start()`：
    - 目的：对象加入世界后的一次性初始化钩子（派生类可覆盖）。
    - 实现细节：`src/base_object.cpp` 提供了一个空的默认实现（no-op），以避免构造链或模板派生时缺少实现导致的问题。
  - `Update()`：
    - 目的：每帧逻辑更新钩子（由 `FramelyUpdate()` 在每帧调用前调用）。
    - 实现细节：源文件中同样提供了空的默认实现（no-op）。
  - 注意：派生类应覆盖这些方法以实现具体行为。
}

, 方法 `GetSpriteFrame()` {
  - 整体作用：返回当前用于绘制的 `PngFrame`，支持基于对象层面竖排帧数切分（当 `m_vertical_frame_count > 1` 时）。
  - 参数：无。
  - 语义：
    - 若 `m_vertical_frame_count <= 1`，退回到 `PngSprite::GetCurrentFrame()` 的默认行为。
    - 否则使用 `GetCurrentFrameWithTotal(m_vertical_frame_count)` 以按竖排分片取子帧。
}

, 贴图路径管理（实现细节） {
  - `void SetSpritePath(const std::string& path) noexcept`：
    - 行为（依据 `src/base_object.cpp`）：
      1. 先检测当前是否已有路径（`HasPath()`）。
      2. 若已有，则先调用 `DrawingSequence::Instance().Unregister(this)` 将自己从绘制序列中移除（防止在路径替换期间被绘制）。
      3. 通过 `PngSprite::ClearPath()` 清理旧路径（实现委托给基类）。
      4. 调用 `PngSprite::SetPath(path)` 设定新路径。
      5. 若新路径有效（`HasPath()` 为真），调用 `DrawingSequence::Instance().Register(this)` 进行注册。
    - 要点：保证在路径更新期间 `DrawingSequence` 不会持有悬空或不一致的 canvas；只在拥有有效贴图路径时参与 `DrawingSequence`。
  - `void ClearSpritePath() noexcept`：
    - 行为（依据实现）：
      1. 记录调用前是否存在路径（`HasPath()`）。
      2. 调用 `PngSprite::ClearPath()` 清除路径。
      3. 若之前存在路径，则调用 `DrawingSequence::Instance().Unregister(this)` 从绘制序列移除。
    - 要点：确保清除贴图同时解除在 `DrawingSequence` 的注册，避免后续绘制访问空 canvas。
  - `bool HasSpritePath() const noexcept`：委托到 `HasPath()` 判断贴图路径是否存在。
  - 备注：`SetSpritePath`/`ClearSpritePath` 在实现中与 `DrawingSequence` 直接交互（见 `src/base_object.cpp`），因此文档应反映这一副作用。
}

, 可见性与绘制深度 {
  - `void SetVisible(bool v) noexcept` / `bool IsVisible() const noexcept`：控制对象在渲染与碰撞检测处是否被视为可见。
  - `void SetDepth(int d) noexcept` / `int GetDepth() const noexcept`：绘制深度（层级），用于渲染排序（值大小与项目约定相关）。
}

, 物理接入（委托到 `BasePhysics`） {
  - 只读访问与写入/操作接口同头文件描述：`GetPosition`/`SetPosition`、`GetVelocity`/`SetVelocity`、`GetForce`/`SetForce`、`GetShape` 等，以及 `AddVelocity`、`AddForce`、`ApplyVelocity`、`ApplyForce`。
  - 这些接口将物理实现封装到 `BasePhysics`，调用者无需直接与物理内部耦合。
}

, 生命周期与销毁辅助接口（实现细节） {
  - 析构：`~BaseObject() noexcept` 的实现（`src/base_object.cpp`）：
    - 调用 `OnDestroy()`（允许派生类在销毁前清理资源）。
    - 调用 `DrawingSequence::Instance().Unregister(this)` 以确保在析构时解除注册，避免 `DrawingSequence` 保留悬空 `owner` 指针。
  - 请求销毁接口（实现转发）：
    - `void DestroyImmediate() noexcept`：实现通过 `InstanceController::Instance().DestroyImmediate(this)` 转发请求。
    - `void DestroyDelayed() noexcept`：实现通过 `InstanceController::Instance().DestroyDelayed(this)` 转发请求。
  - 静态基于 token 的销毁：
    - `static void DestroyImmediate(const InstanceController::ObjectToken& token) noexcept`：转发到 `InstanceController::Instance().DestroyImmediate(token)`。
    - `static void DestroyDelayed(const InstanceController::ObjectToken& token) noexcept`：转发到 `InstanceController::Instance().DestroyDelayed(token)`。
  - 虚钩子：`virtual void OnDestroy() noexcept {}`：提供空默认实现，派生类可重写以释放资源；析构会调用该钩子。
  - 要点：`BaseObject` 本身不持有对象的唯一所有权（`InstanceController` 管理实际 `unique_ptr`），这些方法仅作为便捷转发与保证析构时清理的约定。
}

, 源文件依赖与副作用 {
  - `src/base_object.cpp` 包含 `drawing_sequence.h`，用于在贴图路径变更与析构时调用 `Register`/`Unregister`。
  - 该实现意味着调用 `SetSpritePath`/`ClearSpritePath` 会对 `DrawingSequence` 的注册状态产生副作用——这是预期行为，用以让带贴图的对象参与自动的帧上传/绘制流程。
  - `src/base_object.cpp` 也包含 `instance_controller.h`，以便实现销毁请求的转发。
}

, 私有成员变量 {
  - `int m_vertical_frame_count = 1;`：对象层面的竖排帧数，默认 1（表示无竖排分片）。
  - `int m_sprite_update_freq = 1;`：每隔多少游戏帧切换一次贴图帧，默认 1（每帧切换）。构造时同步给 `PngSprite`。
  - `bool m_visible = true;`：是否可见（默认 true）。
  - `int m_depth = 0;`：绘制深度（默认 0）。
}

}

---

## 设计要点与边界情况（摘要）
- `DrawingSequence` 注册管理：
  - `BaseObject` 的 `SetSpritePath` 与 `ClearSpritePath` 会在内部调用 `DrawingSequence::Instance().Register/Unregister(this)`，以保证只有拥有贴图路径的对象才被 `DrawingSequence` 跟踪。
  - 析构时也会主动 `Unregister`，以避免 `DrawingSequence` 中出现悬空 `owner` 指针。
- 行为副作用须知：
  - 修改贴图路径会导致注册状态变化；调用者在批量修改对象贴图时应意识到可能触发 `DrawingSequence` 的锁与容器操作。
- 错误/异常策略：
  - 关键路径（构造、`FramelyUpdate()`、析构、销毁钩子）以 `noexcept` 标注。实现应避免在这些路径抛出异常；若下游调用可能抛出，应在调用点捕获并记录（当前源文件实现未显式捕获，调用链应保证安全）。
- 生命周期约定：
  - `BaseObject` 不直接持有 `InstanceController` 的所有权语义，但提供便捷方法转发创建/销毁请求。对象的实际生命周期由 `InstanceController` 的 `unique_ptr` 管理。
- 并发注意：
  - `DrawingSequence` 可能在多处并发访问其注册表（头文件声明使用互斥量）。在 `SetSpritePath`/`ClearSpritePath` 中间状态（先 `Unregister` 再 `SetPath` 再 `Register`）会短暂改变注册状态；如果外部对注册顺序有假设，应谨慎设计调用时机。
- 文档一致性：
  - 本文档基于头文件与 `src/base_object.cpp` 的实现细节，反映了 `SetSpritePath`/`ClearSpritePath`、析构与销毁转发的真实行为。若实现变更（例如改为延迟注册/注销或改为由外部显式注册），应同步更新此文档。