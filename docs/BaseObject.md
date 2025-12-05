# BaseObject

说明
- `BaseObject` 是面向游戏/应用开发者的通用对象基类，整合了渲染（`PngSprite`）与物理（`BasePhysics`）功能。
- 目标：提供一个易用的对象生命周期与行为入口（Start/Update/OnDestroy），同时暴露渲染、物理与碰撞回调接口。

主要职责
- 持有和管理精灵资源（`PngSprite`）用于渲染。
- 使用 `BasePhysics` 提供位置/速度/力与碰撞形状管理。
- 在每帧由 `FramelyUpdate()` 被驱动——该方法依次调用 `Update()`、应用力/速度、可选 Debug 绘制并记录上一帧位置。
- 提供碰撞事件分发：`OnCollisionState` 将 Enter / Stay / Exit 映射到 `OnCollisionEnter/Stay/Exit`。

关键方法（摘要）
- 生命周期：
  - `virtual void Start()`：对象建立后由系统调用（CreateImmediate/CreateDelayed 后的 Start）。
  - `virtual void Update()`：每帧逻辑。
  - `virtual void OnDestroy()`：销毁时回调。
  - `void FramelyUpdate()`：引擎每帧调用入口（内部会调用 Update、ApplyForce/ApplyVelocity 等）。

- 渲染相关（封装 `PngSprite`）：
  - `void SpriteSetSource(const std::string& path, int count, bool set_shape_aabb = true)`：设置贴图并 (可选) 根据当前帧设置 AABB。
  - `void SpriteClearPath()`、`bool SpriteHasPath(std::string* out_path = nullptr)`。
  - 动画控制：`SpriteSetUpdateFreq(int)`，`SpriteGetUpdateFreq()`。
  - 翻转/缩放/枢轴：`SpriteFlipX/Y`、`ScaleX/ScaleY/Scale`、`SetPivot`、`GetPivot()`。
  - 可见性/排序：`SetVisible(bool)`、`IsVisible()`、`SetDepth(int)`、`GetDepth()`。
  - `PngFrame SpriteGetFrame() const`：获取当前帧像素（供 `DrawingSequence` 上传/渲染使用）。

- 物理/形状相关（封装 `BasePhysics`）：
  - 位置/速度/力：`SetPosition`/`GetPosition`、`SetVelocity`、`ApplyVelocity` 等。
  - 形状：`SetShape(const CF_ShapeWrapper&)`、`GetShape()`、以及便捷的 `SetAabb`/`SetCircle`/`SetCapsule`/`SetPoly`。
  - 控制物理与精灵同步：
    - `IsColliderRotate()` / `SetColliderRotate(bool)`：是否让碰撞器随精灵旋转。
    - `IsColliderApplyPivot()` / `SetColliderApplyPivot(bool)`：是否将精灵 pivot 应用到碰撞器。
  - `const CF_V2& GetPrevPosition() const`：用于 CCD / debug。

- 碰撞回调：
  - `virtual void OnCollisionEnter(const ObjManager::ObjToken&, const CF_Manifold&)`
  - `virtual void OnCollisionStay(...)`
  - `virtual void OnCollisionExit(...)`
  - `OnCollisionState` 根据阶段分发到上述三个函数。

行为与注意事项
- 当设置 Sprite 源时，`BaseObject` 会向 `DrawingSequence` 注册/注销，从而参与统一的上传/渲染流程。
- 当启用 `IsColliderRotate` 或 `IsColliderApplyPivot` 时，会调用 `BasePhysics::enable_world_shape(...)` 来确保物理形状在读取时已按照旋转/枢轴处理。
- 为了使延迟创建/销毁安全，应通过 `ObjManager` 的 API 创建/销毁对象（以获得正确的 token 与系统注册/反注册流程）。

简短使用示例
```cpp
// 在头文件（.h）中
class MyObject : public BaseObject {
public:
    // 构造函数，如无特殊情况请调用默认基类构造
    MyObject() noexcept : BaseObject() {} 

    // 析构函数，如无特殊情况请调用默认析构
    ~MyObject() noexcept {} 

    // 生命周期开始时的初始化
    void Start() override ; 

    // 每帧更新逻辑
    void Update() override ; 

    // 要使用时声明覆写：碰撞进入回调/持续回调/退出回调
    void OnCollisionEnter(const ObjManager::ObjToken& other_token, const CF_Manifold& manifold) noexcept override ; 
    void OnCollisionStay(const ObjManager::ObjToken& other_token, const CF_Manifold& manifold) noexcept override ; 
    void OnCollisionExit(const ObjManager::ObjToken& other_token, const CF_Manifold& manifold) noexcept override ; 

    // 要使用时声明覆写：销毁时做出额外动作，通常用于资源释放
    void OnDestroy() noexcept override ; 
};
```
```cpp
// 在实现文件（.cpp）中

void MyObject::Start() {
    // 初始化代码，例如设置精灵和物理形状
    SpriteSetSource("assets/sprite.png", 4, true); // 设置精灵贴图
    SetAabb(CF_V2{ -16.0f, -16.0f }, CF_V2{ 16.0f, 16.0f }); // 设置碰撞盒
}

void MyObject::Update() {
    // 每帧更新逻辑，例如移动对象
    CF_V2 pos = GetPosition();
    pos.x += 1.0f; // 向右移动
    SetPosition(pos);
}

void MyObject::OnCollisionEnter(const ObjManager::ObjToken& other_token, const CF_Manifold& manifold) noexcept {
    // 碰撞进入时的处理逻辑
    printf("Collided with object token: %u\n", other_token.index);
}

void MyObject::OnCollisionStay(const ObjManager::ObjToken& other_token, const CF_Manifold& manifold) noexcept {
    // 碰撞持续时的处理逻辑
}

void MyObject::OnCollisionExit(const ObjManager::ObjToken& other_token, const CF_Manifold& manifold) noexcept {
    // 碰撞退出时的处理逻辑
}

void MyObject::OnDestroy() noexcept {
    // 销毁时的逻辑
}
```