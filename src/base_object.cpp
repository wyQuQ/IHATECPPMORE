#include "base_object.h"
#include "drawing_sequence.h" // 在 C++ 文件中引用以便使用 DrawingSequence 接口
#include <iostream>
#include <cmath>

// BaseObject 的精灵资源与绘制注册相关逻辑：
// - SpriteSetSource 在设置新路径时会注册/注销 DrawingSequence 以纳入统一的上传与绘制流程。
// - SpriteClearPath 会卸载资源并从 DrawingSequence 中注销。
// - TweakColliderWithPivot 用于在用户改变 pivot 时同步调整碰撞形状。

void BaseObject::SpriteSetSource(const std::string& path, int count, bool set_shape_aabb) noexcept
{
    std::string current_path = "";
    bool had = HasPath(&current_path);
    if (had && current_path == path)return;
    if (had) {
        // 注销旧资源的绘制注册，释放 per-owner canvas 的引用
        DrawingSequence::Instance().Unregister(this);
    }
    PngSprite::ClearPath();
    PngSprite::SetPath(path);
    SpriteSetVerticalFrameCount(count);
    bool has = HasPath();
    if (has) {
        // 注册到绘制序列，便于 DrawAll/BlitAll 处理
        DrawingSequence::Instance().Register(this);
        if (set_shape_aabb) {
            // 如果需要，根据当前帧自动设置碰撞 AABB（以贴图中心为基准）
            PngFrame frame = SpriteGetFrame();
            if (frame.w > 0 && frame.h > 0) {
                SetCenteredAabb(static_cast<float>(frame.w) * 0.5f, static_cast<float>(frame.h) * 0.5f);
            }
        }
    }
}

void BaseObject::SpriteClearPath() noexcept
{
    bool had = HasPath();
    PngSprite::ClearPath();
    if (had) {
        DrawingSequence::Instance().Unregister(this);
    }
}

// 当用户想要将 pivot 应用于碰撞器时，调整本地 shape 以将枢轴偏移应用到形状（便于渲染/碰撞对齐）
void BaseObject::TweakColliderWithPivot(const CF_V2& pivot) noexcept
{
    CF_ShapeWrapper shape = get_shape();
    switch (shape.type) {
    case CF_ShapeType::CF_SHAPE_TYPE_AABB:
        shape.u.aabb.min -= pivot;
        shape.u.aabb.max -= pivot;
        break;
    case CF_ShapeType::CF_SHAPE_TYPE_CIRCLE:
        shape.u.circle.p -= pivot;
        break;
    case CF_ShapeType::CF_SHAPE_TYPE_CAPSULE:
        shape.u.capsule.a -= pivot;
        shape.u.capsule.b -= pivot;
        break;
    case CF_ShapeType::CF_SHAPE_TYPE_POLY:
        for (int i = 0; i < shape.u.poly.count; ++i) {
            shape.u.poly.verts[i] -= pivot;
        }
        break;
    default:
        std::cerr << "[BaseObject] TweakColliderWithPivot: Unsupported shape type " << static_cast<int>(shape.type) << std::endl;
        break;
    }
    set_shape(shape);
}

bool BaseObject::IsCollidedWith(const BaseObject& other, CF_Manifold& out_m) noexcept
{
    CF_ShapeWrapper A = this->GetShape();
    CF_ShapeWrapper B = other.GetShape();
    bool res = cf_collided(
        &A.u, nullptr, A.type,
        &B.u, nullptr, B.type
    ) != 0;
    if (res) {
        cf_collide(
            &A.u, nullptr, A.type,
            &B.u, nullptr, B.type,
            &out_m
        );
    }
    else {
        out_m = CF_Manifold{};
    }
    return res;
}

CF_Manifold BaseObject::ExclusionWithSolid(const ObjManager::ObjToken& oth, const CF_Manifold& m) noexcept
{
    if (!oth.isValid() || !objs.IsValid(oth) || v2math::length(m.contact_points[0] - m.contact_points[1]) < 1e-3f) return m;

	BaseObject& other = objs[oth];
    CF_V2 vel = GetVelocity();
    CF_Manifold result = m;

    float max_d = m.count == 2 ? std::max(m.depths[0], m.depths[1]) : m.depths[0];
	float dot = v2math::dot(vel, m.n);
    if (dot > 1e-3f && max_d - dot > 1e-3f) {
        FindContactPos(GetPosition(), m.n * max_d, other, result);
    }
    else {
        SetPosition(GetPosition() - vel);
        constexpr int pieces = 16;
        CF_V2 step = vel / pieces;
        for (int i = 1; i <= pieces; i++) {
            CF_V2 current = GetPosition();
            SetPosition(current + step);
            if (IsCollidedWith(other, result)) 
				FindContactPos(GetPosition(), result.n * v2math::dot(result.n, step), other, result);
        }
    }
	IsCollidedWith(other, result);
    return result;
}

void BaseObject::FindContactPos(CF_V2 current, CF_V2 offset, const BaseObject& other, CF_Manifold& res) {
	CF_V2 cur = current;
	CF_V2 side = cur - offset;
    for (int it = 1; it <= 16; it++) {
        CF_V2 mid = (cur + side) / 2.0f;
        SetPosition(mid);
        if (!IsCollidedWith(other, res)) {
            SetPosition(cur);
            break;
        }
        else cur = mid;
    }
}

APPLIANCE void BaseObject::OnCollisionState(const ObjManager::ObjToken& other, const CF_Manifold& manifold, CollisionPhase phase) noexcept
{
    CF_Manifold m = manifold;
    if (!IsCollidedWith(objs[other], m)) return;
    // 处理与固体对象的排斥逻辑（如果启用）:
    // 将碰撞体沿着速度方向逐步回退1像素直到与other刚好接触而不重叠
    if (m_exclude_with_solid && 
        objs[other].GetColliderType() == ColliderType::SOLID) 
    {
        m = ExclusionWithSolid(other, manifold);
        OnExclusionSolid(other, m);
    }

    switch (phase) {
    case CollisionPhase::Enter:
        OnCollisionEnter(other, m);
        break;
    case CollisionPhase::Stay:
        OnCollisionStay(other, m);
        break;
    case CollisionPhase::Exit:
        OnCollisionExit(other, m);
        break;
    }

    ManifoldDraw(m);
}

BaseObject::~BaseObject() noexcept
{
    // 在销毁时通知 OnDestroy 并确保从绘制序列注销，释放与绘制相关的所有资源引用。
    OnDestroy();
    DrawingSequence::Instance().Unregister(this);
}