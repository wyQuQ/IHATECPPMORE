#pragma once
#include "base_physics.h"
#include "png_sprite.h"
#include <string>
#include <type_traits>
#include <utility>
#include <vector> // 新增：用于多边形便捷接口
#include <iostream> // 新增：用于调试输出
#include <cmath> // 新增：fabs 等

#include "obj_manager.h"
#include "debug_config.h"

// 前向声明：在类内会调用此函数，具体实现位于 src/base_object_debug.cpp
class BaseObject;
void RenderBaseObjectCollisionDebug(const BaseObject* obj) noexcept;

class BaseObject : public BasePhysics, private PngSprite {
public:
    BaseObject() noexcept
        : BasePhysics(), PngSprite("", 1, 1)
    {
        set_position(CF_V2{ 0.0f, 0.0f });
        set_velocity(CF_V2{ 0.0f, 0.0f });
        set_force(CF_V2{ 0.0f, 0.0f });
        // 确保 PngSprite 的帧延迟与对象层面的设置保持一致
        SetFrameDelay(m_sprite_update_freq);
    }

    void FramelyUpdate() noexcept
    {
        Update(); //调用派生类的 Update 方法

        //每帧自动应用物理积分
        ApplyForce();
        ApplyVelocity();

        // 编译时控制：若定义了 BASEOBJECT_DEBUG_DRAW，DebugDraw 会执行实际绘制，
        // 否则为内联空实现（编译期剔除）。
        DebugDraw();

        // 保存上一帧位置，供 CCD / TOI / debug 使用（在 Update 之前）
        m_prev_position = get_position();
    }

    virtual void Start(){}
    virtual void Update(){}

    // 返回当前帧，使用对象层面的竖排帧数切片 PNG。
    // 如果 m_vertical_frame_count <= 1 则退回到 PngSprite 的默认行为。
    PngFrame SpriteGetFrame() const
    {
        if (m_vertical_frame_count <= 1) {
            return GetCurrentFrame();
        }
        return GetCurrentFrameWithTotal(m_vertical_frame_count);
    }

    // ---- 竖排帧计数接口（可以在派生类或外部设置） ----
    int SpriteGetVerticalFrameCount() const noexcept { return m_vertical_frame_count; }

    // ---- 每多少个游戏帧切换一次动画帧（对象层面，可单独设置） ----
    // spriteUpdateFreq: 动画切换频率，单位为游戏帧数。值 <= 0 会被规范为 1。
    void SpriteSetUpdateFreq(int freq) noexcept
    {
        m_sprite_update_freq = (freq > 0 ? freq : 1);
        // 同步到 PngSprite 的帧延迟，以便 PngSprite 的 GetCurrentFrame* 系列使用该频率
        SetFrameDelay(m_sprite_update_freq);
    }
    int SpriteGetUpdateFreq() const noexcept { return m_sprite_update_freq; }

    // ---- 贴图路径管理（委托到 PngSprite） ----
    void SpriteSetSource(const std::string& path, int count, bool set_shape_aabb = true) noexcept;
    void SpriteClearPath() noexcept;

    bool SpriteHasPath(std::string* out_path = nullptr) const noexcept { return HasPath(out_path); }

    // ---- 可见性与绘制深度 ----
    // visible: 控制贴图与碰撞箱是否被视为可见（绘制/碰撞判定处需检查该值）
    void SetVisible(bool v) noexcept { m_visible = v; }
    bool IsVisible() const noexcept { return m_visible; }

    // depth: 绘图序列中的层次，数值越大通常代表绘制越靠上（或按项目约定使用）
    void SetDepth(int d) noexcept { m_depth = d; }
    int GetDepth() const noexcept { return m_depth; }

    // 旋转
    float SpriteGetRotation() noexcept { return GetRotation(); }
    void SpriteSetRotation(float rot) noexcept { SetRotation(rot); }
    void SpriteRotate(float drot) noexcept { Rotate(drot); }

    // 翻转
    bool SpriteGetFlipX() { return m_flip_x; }
    bool SpriteGetFlipY() { return m_flip_y; }
    void SpriteFlipX(bool x) { m_flip_x = x; }
    void SpriteFlipY(bool y) { m_flip_y = y; }
    void SpriteFlipX() { m_flip_x = !m_flip_x; }
    void SpriteFlipY() { m_flip_y = !m_flip_y; }

	// 枢轴点
	CF_V2 GetPivot() noexcept { return get_pivot(); }
	void SetPivot(const CF_V2& p) noexcept {
        TweakColliderWithPivot(p);
        set_pivot(p); 
    }

    // 统一接口：一次性设置贴图路径、竖排帧数、动画更新频率和绘制深度
    void SpriteSetStats(const std::string& path, int vertical_frame_count, int update_freq, int depth, bool set_shape_aabb = true) noexcept
    {
        SpriteSetSource(path, vertical_frame_count, set_shape_aabb);
        SpriteSetUpdateFreq(update_freq);
        SetDepth(depth);
    }

    // ---- 公开物理相关访问器，便于外部使用 ----
    // 只读访问（委托到 BasePhysics）
    const CF_V2& GetPosition() const noexcept { return get_position(); }
    const CF_V2& GetVelocity() const noexcept { return get_velocity(); }
    const CF_V2& GetForce() const noexcept { return get_force(); }
    const CF_ShapeWrapper& GetShape() const noexcept { return get_shape(); }
    ColliderType GetColliderType() const noexcept { return get_collider_type(); }

    // 可选写入/操作接口（委托到 BasePhysics）
    void SetPosition(const CF_V2& p) noexcept { set_position(p); }
    void SetVelocity(const CF_V2& v) noexcept { set_velocity(v); }
    void SetForce(const CF_V2& f) noexcept { set_force(f); }
    void AddVelocity(const CF_V2& dv) noexcept { add_velocity(dv); }
    void AddForce(const CF_V2& df) noexcept { add_force(df); }
    void ApplyVelocity(float dt = 1) noexcept { apply_velocity(dt); }
    void ApplyForce(float dt = 1) noexcept { apply_force(dt); }

    // 允许获取上一帧位置（用于简单防穿透逻辑或调试）
    const CF_V2& GetPrevPosition() const noexcept { return m_prev_position; }

    // 允许派生类在 Start() 中设置碰撞形状与碰撞类型（包装 BasePhysics 的低级 API）
    void SetShape(const CF_ShapeWrapper& s) noexcept { set_shape(s); }
    void SetColliderType(ColliderType t) noexcept { set_collider_type(t); }

    // ---- 为形状添加便捷初始化接口 ----
    // 直接通过已有 CF_* 类型设置（最可靠）
    void SetAabb(const CF_Aabb& a) noexcept { set_shape(CF_ShapeWrapper::FromAabb(a)); }
    void SetCircle(const CF_Circle& c) noexcept { set_shape(CF_ShapeWrapper::FromCircle(c)); }
    void SetCapsule(const CF_Capsule& c) noexcept { set_shape(CF_ShapeWrapper::FromCapsule(c)); }
    void SetPoly(const CF_Poly& p) noexcept { set_shape(CF_ShapeWrapper::FromPoly(p)); }

    // 便捷方法：以对象当前位置为中心设置 AABB（宽度 = 2*half_w, 高度 = 2*half_h）
    // 修改：改为创建“局部坐标”的 AABB（相对于对象中心），不再把 get_position() 写入 shape。
    void SetCenteredAabb(float half_w, float half_h) noexcept
    {
        CF_Aabb aabb{};
        aabb.min.x = -half_w;
        aabb.min.y = -half_h;
        aabb.max.x = half_w;
        aabb.max.y = half_h;
        set_shape(CF_ShapeWrapper::FromAabb(aabb));
#if SHAPE_DEBUG
        std::cerr << "[BaseObject] SetCenteredAabb: half_w=" << half_w << " half_h=" << half_h
            << " -> AABB local min=(" << aabb.min.x << "," << aabb.min.y << ")"
            << " max=(" << aabb.max.x << "," << aabb.max.y << ")" << std::endl;
#endif
    }

    // 便捷：以当前位置为中心设置 Circle（改为局部坐标）
    void SetCenteredCircle(float radius) noexcept
    {
        CF_Circle c{};
        c.p = CF_V2{ 0.0f, 0.0f };
        c.r = radius;
        set_shape(CF_ShapeWrapper::FromCircle(c));
#if SHAPE_DEBUG
        std::cerr << "[BaseObject] SetCenteredCircle: radius=" << radius << std::endl;
#endif
    }

    // 便捷：以当前位置为中心设置 Capsule（方向通过 dir 指定，需为单位向量）
    // 修改：生成以对象中心为基准的局部 capsule（a,b 为局部坐标）
    // 这里做了端点顺序归一化：对于主要水平轴，保证 a.x <= b.x；对于主要竖直轴，保证 a.y <= b.y
    void SetCenteredCapsule(const CF_V2& dir, float half_length, float radius) noexcept
    {
		CF_V2 dir_normalized = v2math::normalized(dir);
        CF_V2 a{ -dir_normalized.x * half_length, -dir_normalized.y * half_length };
        CF_V2 b{ dir_normalized.x * half_length,  dir_normalized.y * half_length };

        // 端点排序：让 a 在“负方向”，b 在“正方向”（对齐数轴）
        if (std::fabs(dir_normalized.x) >= std::fabs(dir_normalized.y)) {
            // 主要为水平向量：确保 a.x <= b.x
            if (a.x > b.x) std::swap(a, b);
        }
        else {
            // 主要为竖直向量：确保 a.y <= b.y
            if (a.y > b.y) std::swap(a, b);
        }

        CF_Capsule c = cf_make_capsule(a, b, radius);
        set_shape(CF_ShapeWrapper::FromCapsule(c));
#if SHAPE_DEBUG
        std::cerr << "[BaseObject] SetCenteredCapsule: dir=(" << dir_normalized.x << "," << dir_normalized.y
            << ") half_length=" << half_length << " radius=" << radius
            << " -> a=(" << a.x << "," << a.y << ") b=(" << b.x << "," << b.y << ")" << std::endl;
#endif
    }

    // 便捷：从局部顶点集合创建多边形（顶点以对象中心为基准）
    // 已改为直接使用 localVerts（不要再加 get_position）
    void SetPolyFromLocalVerts(const std::vector<CF_V2>& localVerts) noexcept
    {
        CF_Poly p{};
        int n = static_cast<int>(localVerts.size());
        p.count = n;
        for (int i = 0; i < n; ++i) {
            p.verts[i] = CF_V2{ localVerts[i].x, localVerts[i].y };
        }
        cf_make_poly(&p);
        set_shape(CF_ShapeWrapper::FromPoly(p));
#if SHAPE_DEBUG
        std::cerr << "[BaseObject] SetPolyFromLocalVerts: count=" << n << " verts(local):";
        for (int i = 0; i < n; ++i) {
            std::cerr << " (" << p.verts[i].x << "," << p.verts[i].y << ")";
        }
        std::cerr << std::endl;
#endif
    }

    // 新增：碰撞回调分化
    // 建议碰撞分发器（Collider / InstanceController）在检测到碰撞时调用 OnCollisionState 或直接调用 OnCollisionEnter/Stay/Exit。
    enum class CollisionPhase : int { Enter = 1, Stay = 0, Exit = -1 };

    // 新增：状态形式的统一调度接口（便于从外部一次性传入状态）
    virtual void OnCollisionState(const ObjManager::ObjToken& other, const CF_Manifold& manifold, CollisionPhase phase) noexcept
    {
        switch (phase) {
        case CollisionPhase::Enter:
            OnCollisionEnter(other, manifold);
            break;
        case CollisionPhase::Stay:
            OnCollisionStay(other, manifold);
            break;
        case CollisionPhase::Exit:
            OnCollisionExit(other, manifold);
            break;
        }
    }

    // 新增：分阶段回调（派生类直接覆写所需阶段）
    virtual void OnCollisionEnter(const ObjManager::ObjToken& other, const CF_Manifold& manifold) noexcept {}
    virtual void OnCollisionStay(const ObjManager::ObjToken& other, const CF_Manifold& manifold) noexcept {}
    virtual void OnCollisionExit(const ObjManager::ObjToken& other, const CF_Manifold& manifold) noexcept {}

    // ---- Debug 绘制相关 API（由编译宏控制） ----
    // 现在绘制控制由宏 BASEOBJECT_DEBUG_DRAW 决定（编译时启用/禁用），不再使用运行时 bool 开关。
#if BASEOBJECT_DEBUG_DRAW
    void DebugDraw() const noexcept
    {
        RenderBaseObjectCollisionDebug(this);
    }
#else
    inline void DebugDraw() const noexcept {}
#endif

    virtual void OnDestroy() noexcept {}

    ~BaseObject() noexcept;

private:
    // 隐藏 BasePhysics 的原始小写 API，强制调用者使用 BaseObject 的 PascalCase 封装接口，统一命名风格。
    // 在 private 部分使用 using 声明会将这些基类名的访问权限降为私有。
    using BasePhysics::get_position;
    using BasePhysics::set_position;
    using BasePhysics::get_velocity;
    using BasePhysics::set_velocity;
    using BasePhysics::get_force;
    using BasePhysics::set_force;
    using BasePhysics::add_velocity;
    using BasePhysics::add_force;
    using BasePhysics::apply_velocity;
    using BasePhysics::apply_force;
    using BasePhysics::get_shape;
    using BasePhysics::set_shape;
    using BasePhysics::set_collider_type;
    using BasePhysics::get_collider_type;

	void TweakColliderWithPivot(const CF_V2& pivot) noexcept;

    // ---- 对象层面的贴图与动画控制参数 ----

    // 竖排帧数，用于从 PNG 中切片当前帧
    int m_vertical_frame_count = 1;
    void SpriteSetVerticalFrameCount(int count) noexcept { m_vertical_frame_count = (count > 0 ? count : 1); }

    // 每多少个游戏帧切换一次动画帧
    int m_sprite_update_freq = 1;

    // 贴图可见性
    bool m_visible = true;

    // 绘制深度
    int m_depth = 0;

    // （已移除）运行时开关 m_debug_draw_enabled —— 改为编译时宏控制

    // 新增：上一帧位置（用于简单防穿透/调试）
    CF_V2 m_prev_position = CF_V2{ 0.0f, 0.0f };
};