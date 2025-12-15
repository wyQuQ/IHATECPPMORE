#include "base_object.h"
#include "drawing_sequence.h" // 在 C++ 文件中引用以便使用 DrawingSequence 接口
#include "cute_sprite.h"      // 包含以使用 CF_Sprite 和相关函数
#include <iostream>
#include <cmath>

// BaseObject 的精灵资源与绘制注册相关逻辑：
// - SpriteSetSource 在设置新路径时会注册/注销 DrawingSequence 以纳入统一的上传与绘制流程。
// - TweakColliderWithPivot 用于在用户改变 pivot 时同步调整碰撞形状。

void BaseObject::SpriteSetStats(const std::string& path, int vertical_frame_count, int update_freq, int depth, bool set_shape_aabb) noexcept
{
    SpriteSetSource(path, vertical_frame_count, set_shape_aabb);
    SpriteSetUpdateFreq(update_freq);
    SetDepth(depth);
}

void BaseObject::SpriteSetSource(const std::string& path, int vertical_frame_count, bool set_shape_aabb) noexcept
{
    // 如果路径未改变，则不执行任何操作
    if (m_sprite_path == path) {
        return;
    }

    CF_V2 preserved_scale = m_sprite.scale;
    auto restore_scale = [this, preserved_scale]() noexcept {
        m_sprite.scale = preserved_scale;
        BasePhysics::scale_x(preserved_scale.x);
        BasePhysics::scale_y(preserved_scale.y);
    };

    // 如果之前有有效的精灵路径，先从绘制序列中注销
    if (!m_sprite_path.empty()) {
        DrawingSequence::Instance().Unregister(this);
        cf_easy_sprite_unload(&m_sprite);
    }

    // 更新路径和帧数
    m_sprite_path = path;
    m_sprite_vertical_frame_count = vertical_frame_count;
    m_sprite_current_frame_index = 0;

    // 如果新路径为空，则重置精灵并返回
    if (m_sprite_path.empty()) {
        m_sprite = cf_sprite_defaults();
        restore_scale();
        return;
    }

    // 始终使用 cf_make_easy_sprite_from_png 加载 PNG 文件。
    // cute_sprite 将整个文件加载为单个大图像。
    // 多帧动画的分割逻辑需要由您的渲染器（DrawingSequence）根据 m_sprite_vertical_frame_count 处理。
    m_sprite = cf_make_easy_sprite_from_png(m_sprite_path.c_str(), nullptr);
    if (!m_sprite.easy_sprite_id) {
        OUTPUT({ "Sprite" }, "Failed to load sprite:", m_sprite_path.c_str());
        m_sprite = cf_sprite_defaults();
        restore_scale();
        m_sprite_path.clear();
        return;
    }
    restore_scale();

    // 注册到绘制序列并更新碰撞体
    DrawingSequence::Instance().Register(this);
    if (set_shape_aabb) {
        // 从 CF_Sprite 获取帧尺寸。
        // 对于雪碧图，宽度是整个图像的宽度，高度是单帧的高度。
        if (m_sprite.w > 0 && m_sprite.h > 0 && m_sprite_vertical_frame_count > 0) {
            float frame_height = static_cast<float>(m_sprite.h) / m_sprite_vertical_frame_count;
            SetCenteredAabb(static_cast<float>(m_sprite.w) * 0.5f, frame_height * 0.5f);
        }
    }
}

void BaseObject::SpriteSetUpdateFreq(int update_freq) noexcept
{
	m_sprite_update_freq = update_freq > 0 ? update_freq : 1;
}

// 当用户想要将 pivot 应用于碰撞器时，调整本地 shape 以将枢轴偏移应用到形状（便于渲染/碰撞对齐）
void BaseObject::TweakColliderWithPivot(const CF_V2& pivot) noexcept
{
    CF_ShapeWrapper shape = get_local_shape();
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
    OUTPUT({"Player"}, "set to ", GetPosition().x, ",", GetPosition().y);
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

    m_collide_manifolds.emplace_back(m);
}

BaseObject::~BaseObject() noexcept
{
    // 在销毁时通知 OnDestroy 并确保从绘制序列注销，释放与绘制相关的所有资源引用。
    OnDestroy();
    DrawingSequence::Instance().Unregister(this);
    if (!m_sprite_path.empty()) {
        cf_easy_sprite_unload(&m_sprite);
    }
}