#pragma once
#include "base_physics.h"
#include "png_sprite.h"
#include <string>
#include <type_traits>
#include <utility>
#include <vector> // 用于多边形顶点数据传递
#include <iostream> // 错误/诊断输出
#include <cmath> // fabs
#include <algorithm> // std::remove, std::find
#include <unordered_set> // 用于 tags

#include "obj_manager.h"
#include "debug_config.h"

// BaseObject 是面向使用者的游戏对象基类，整合渲染（PngSprite）与物理（BasePhysics）。
// 使用说明：
// - 继承 BaseObject 并重写 Start()/Update()/OnCollisionEnter/Stay/Exit/OnDestroy 以实现对象行为。
// - 通过 ObjManager 创建/销毁对象以获得 ObjToken；也可以直接使用指针 API（注意生命周期管理）。
// - 提供统一的 Sprite 接口，方便设置贴图源、帧控制、翻转、枢轴及自动基于贴图设置碰撞箱。
// - 提供碰撞回调机制（Enter/Stay/Exit）以便在物理系统检测到碰撞事件时被通知。
class BaseObject;
void RenderBaseObjectCollisionDebug(const BaseObject* obj) noexcept;

class BaseObject : public BasePhysics, private PngSprite {
public:
    BaseObject() noexcept
        : BasePhysics(), PngSprite("", 1, 1)
    {
        // 初始化位置/速度/力与精灵帧率，确保默认状态可直接使用
        set_position(CF_V2{ 0.0f, 0.0f });
        set_velocity(CF_V2{ 0.0f, 0.0f });
        set_force(CF_V2{ 0.0f, 0.0f });
        SetFrameDelay(m_sprite_update_freq);
        update_world_shape_flag();
		tags.reserve(5); // 预留一些空间以减少动态分配
    }

    // 每帧引擎调用点：包含 Update()、物理运动与 debug 绘制
    void FramelyUpdate() noexcept
    {
        Update(); // 用户实现的每帧逻辑

        // 应用力与速度更新位置
        ApplyForce();
        ApplyVelocity();

        // 可选的调试绘制（由编译选项控制）
        DebugDraw();

        // 记录上一帧位置（便于 CCD / 调试）
        m_prev_position = get_position();
    }

    virtual void Start(){}
    virtual void Update(){}

    // 获取当前用于渲染的帧（考虑垂直图集的多行帧）
    PngFrame SpriteGetFrame() const
    {
        if (m_vertical_frame_count <= 1) {
            return GetCurrentFrame();
        }
        return GetCurrentFrameWithTotal(m_vertical_frame_count);
    }

    int SpriteGetVerticalFrameCount() const noexcept { return m_vertical_frame_count; }

    // 控制精灵动画更新频率（以游戏帧为单位）
    void SpriteSetUpdateFreq(int freq) noexcept
    {
        m_sprite_update_freq = (freq > 0 ? freq : 1);
        SetFrameDelay(m_sprite_update_freq);
    }
    int SpriteGetUpdateFreq() const noexcept { return m_sprite_update_freq; }

    // 精灵资源接口：设置/清除路径并自动在有资源时注册到 DrawingSequence
    void SpriteSetSource(const std::string& path, int count, bool set_shape_aabb = true) noexcept;
    void SpriteClearPath() noexcept;

    bool SpriteHasPath(std::string* out_path = nullptr) const noexcept { return HasPath(out_path); }

    // 可见性/深度控制（用于统一渲染排序）
    void SetVisible(bool v) noexcept { m_visible = v; }
    bool IsVisible() const noexcept { return m_visible; }

    void SetDepth(int d) noexcept { m_depth = d; }
    int GetDepth() const noexcept { return m_depth; }

    // 旋转/翻转/枢轴：精灵层与碰撞层可以选择性同步
    float GetRotation() const noexcept { return PngSprite::GetSpriteRotation(); }
    void SetRotation(float rot) noexcept
    {
        PngSprite::SetSpriteRotation(rot);
        if (IsColliderRotate()) {
            BasePhysics::set_rotation(PngSprite::GetSpriteRotation());
            BasePhysics::mark_world_shape_dirty();
        }
    }
    void Rotate(float drot) noexcept
    {
        PngSprite::RotateSprite(drot);
        if (IsColliderRotate()) {
            BasePhysics::set_rotation(PngSprite::GetSpriteRotation());
            BasePhysics::mark_world_shape_dirty();
        }
    }

    bool SpriteGetFlipX() { return m_flip_x; }
    bool SpriteGetFlipY() { return m_flip_y; }
    void SpriteFlipX(bool x) { m_flip_x = x; }
    void SpriteFlipY(bool y) { m_flip_y = y; }
    void SpriteFlipX() { m_flip_x = !m_flip_x; }
    void SpriteFlipY() { m_flip_y = !m_flip_y; }
	float GetScaleX() const noexcept { return m_scale_x; }
    void ScaleX(float sx) noexcept { PngSprite::set_scale_x(sx); BasePhysics::scale_x(sx); m_scale_x = sx; }
	float GetScaleY() const noexcept { return m_scale_y; }
    void ScaleY(float sy) noexcept { PngSprite::set_scale_y(sy); BasePhysics::scale_y(sy); m_scale_y = sy; }
    void Scale(float s) noexcept {
		PngSprite::set_scale_x(s); BasePhysics::scale_x(s); m_scale_x = s;
		PngSprite::set_scale_y(s); BasePhysics::scale_y(s); m_scale_y = s;
	}

    CF_V2 GetPivot() const noexcept { return PngSprite::get_pivot(); }
	// 设置相对于贴图中心的枢轴点，例如 (0,0) 为中心，(1,1) 为右上角，(-1,-1) 为左下角
    void SetPivot(float x_rel, float y_rel) noexcept {
		float scale_x = GetScaleX();
		float scale_y = GetScaleY();
        Scale(1.0f);
        auto frame = SpriteGetFrame();
        float hw = frame.w / 2.0f;
        float hh = frame.h / 2.0f;
		CF_V2 p = CF_V2{ x_rel * hw, y_rel * hh };
        PngSprite::set_pivot(p);
        if (IsColliderApplyPivot()) {
            TweakColliderWithPivot(p);
            BasePhysics::set_pivot(p);
            BasePhysics::mark_world_shape_dirty();
        }
		ScaleX(scale_x);
		ScaleY(scale_y);
    }

    // 组合的设置函数，便捷初始化精灵资源与属性
    void SpriteSetStats(const std::string& path, int vertical_frame_count, int update_freq, int depth, bool set_shape_aabb = true) noexcept
    {
        SpriteSetSource(path, vertical_frame_count, set_shape_aabb);
        SpriteSetUpdateFreq(update_freq);
        SetDepth(depth);
    }

    // 控制碰撞器是否随精灵旋转与枢轴设置同步
    bool IsColliderRotate() const noexcept { return m_isColliderRotate; }
    void SetColliderRotate(bool v) noexcept { m_isColliderRotate = v; update_world_shape_flag(); }

    bool IsColliderApplyPivot() const noexcept { return m_isColliderApplyPivot; }
    void SetColliderApplyPivot(bool v) noexcept { m_isColliderApplyPivot = v; update_world_shape_flag(); }

    // 将 BasePhysics 的常用接口以 PascalCase 暴露给上层，便于脚本/外部调用
    const CF_V2& GetPosition() const noexcept { return get_position(); }
    const CF_V2& GetVelocity() const noexcept { return get_velocity(); }
    const CF_V2& GetForce() const noexcept { return get_force(); }
    const CF_ShapeWrapper& GetShape() const noexcept { return get_shape(); }
    ColliderType GetColliderType() const noexcept { return get_collider_type(); }

    void SetPosition(const CF_V2& p) noexcept { set_position(p); }
    void SetVelocity(const CF_V2& v) noexcept { set_velocity(v); }
    void SetForce(const CF_V2& f) noexcept { set_force(f); }
    void AddVelocity(const CF_V2& dv) noexcept { add_velocity(dv); }
    void AddForce(const CF_V2& df) noexcept { add_force(df); }
    void ApplyVelocity(float dt = 1) noexcept { apply_velocity(dt); }
    void ApplyForce(float dt = 1) noexcept { apply_force(dt); }

    const CF_V2& GetPrevPosition() const noexcept { return m_prev_position; }

    void SetShape(const CF_ShapeWrapper& s) noexcept { set_shape(s); }
    void SetColliderType(ColliderType t) noexcept { set_collider_type(t); }

    // 便捷创建碰撞形状的接口
    void SetAabb(const CF_Aabb& a) noexcept { set_shape(CF_ShapeWrapper::FromAabb(a)); }
    void SetCircle(const CF_Circle& c) noexcept { set_shape(CF_ShapeWrapper::FromCircle(c)); }
    void SetCapsule(const CF_Capsule& c) noexcept { set_shape(CF_ShapeWrapper::FromCapsule(c)); }
    void SetPoly(const CF_Poly& p) noexcept { set_shape(CF_ShapeWrapper::FromPoly(p)); }

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

    void SetCenteredCapsule(const CF_V2& dir, float half_length, float radius) noexcept
    {
		CF_V2 dir_normalized = v2math::normalized(dir);
        CF_V2 a{ -dir_normalized.x * half_length, -dir_normalized.y * half_length };
        CF_V2 b{ dir_normalized.x * half_length,  dir_normalized.y * half_length };

        if (std::fabs(dir_normalized.x) >= std::fabs(dir_normalized.y)) {
            if (a.x > b.x) std::swap(a, b);
        }
        else {
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

    enum class CollisionPhase : int { Enter = 1, Stay = 0, Exit = -1 };

    // 统一的碰撞状态回调分发：上层可以覆写具体阶段的处理函数
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

    virtual void OnCollisionEnter(const ObjManager::ObjToken& other, const CF_Manifold& manifold) noexcept {}
    virtual void OnCollisionStay(const ObjManager::ObjToken& other, const CF_Manifold& manifold) noexcept {}
    virtual void OnCollisionExit(const ObjManager::ObjToken& other, const CF_Manifold& manifold) noexcept {}

#if BASEOBJECT_DEBUG_DRAW
    void DebugDraw() const noexcept
    {
        RenderBaseObjectCollisionDebug(this);
    }
#else
    inline void DebugDraw() const noexcept {}
#endif

    void AddTag(const std::string& tag) noexcept
    {
        tags.insert(tag);
    }

    bool HasTag(const std::string& tag) const noexcept
    {
        return tags.find(tag) != tags.end();
    }

    void RemoveTag(const std::string& tag) noexcept
    {
        tags.erase(tag);
    }

    virtual void OnDestroy() noexcept {}

    ~BaseObject() noexcept;

private:
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

    int m_vertical_frame_count = 1;
    void SpriteSetVerticalFrameCount(int count) noexcept { m_vertical_frame_count = (count > 0 ? count : 1); }

    int m_sprite_update_freq = 1;
    bool m_visible = true;
    int m_depth = 0;
    bool m_flip_x = false;
    bool m_flip_y = false;
	float m_scale_x = 1.0f;
	float m_scale_y = 1.0f;
    CF_V2 m_prev_position = CF_V2{ 0.0f, 0.0f };

    bool m_isColliderRotate = true;
    bool m_isColliderApplyPivot = true;

	std::unordered_set<std::string> tags; // 对象标签集合（无序，不重复）

    void update_world_shape_flag() noexcept
    {
        BasePhysics::enable_world_shape(m_isColliderRotate || m_isColliderApplyPivot);
    }
};