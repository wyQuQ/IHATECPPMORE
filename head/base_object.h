#pragma once
#include "base_physics.h"
#include "png_sprite.h"
#include <string>
#include <utility>
#include <vector> // 用于多边形顶点数据传递
#include <iostream> // 错误/诊断输出
#include <cmath> // fabs
#include <unordered_set> // 用于 tags

#include "obj_manager.h"
#include "debug_config.h"
#include "input.h"

// BaseObject 是面向使用者的游戏对象基类，整合渲染（PngSprite）与物理（BasePhysics）。
// 设计目标与契约：
// - 作为游戏对象的基类，为上层提供统一的渲染/物理接口与生命周期钩子（Start/Update/OnDestroy/碰撞回调）。
// - 不直接管理对象的创建/销毁生命周期（建议通过 ObjManager 进行管理以获得 ObjToken 与安全的延迟创建/销毁语义）。
// - 将 PngSprite 作为私有基类以复用贴图/帧逻辑，但对上层以 PascalCase 暴露便捷方法（例如 GetPosition/SetPosition）。
// - 将 BasePhysics 公开继承以直接复用位置/速度/形状逻辑，但对外暴露的 setter/getter 使用 PascalCase 进行封装以便脚本层调用。
// 使用建议：
// - 在派生类中覆盖 Start()、Update() 实现行为。在需要响应碰撞时覆写 OnCollisionEnter/Stay/Exit（使用 ObjToken 标识碰撞对象）。
// - 通过 ObjManager::Create(...) 获取 pending token；若需要访问尚未提交的对象可通过 ObjManager 的 pending token 访问（参见 ObjManager 行为）。
// - 当需要渲染与碰撞保持一致时，可启用 IsColliderRotate/IsColliderApplyPivot，使碰撞形状随精灵旋转/枢轴自动更新。
// 注意事项：
// - BaseObject 默认启用 m_isColliderRotate = true, m_isColliderApplyPivot = true，二者用于决定是否将 local shape 处理为 world-shape（性能权衡）。
// - 构造函数不会把对象注册到 ObjManager；请使用 ObjManager 创建对象以获得正确的 token 与物理注册。
class BaseObject;
void RenderBaseObjectCollisionDebug(const BaseObject* obj) noexcept;

inline ObjManager& objs = ObjManager::Instance();

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

    // 每帧引擎调用点（由 ObjManager 的 UpdateAll 在合适时机触发）：
    // - ApplyForce/ApplyVelocity（物理积分） -> DebugDraw -> 记录上一帧位置（用于连续碰撞检测/调试）
    // FramelyApply 不进行异常抛出（noexcept），保证在遍历过程中安全。
    void FramelyApply() noexcept
    {
        // 应用力与速度更新位置（基于 BasePhysics 的实现）
        ApplyForce();
        ApplyVelocity();

        // 可选的调试绘制（由编译选项控制）
        DebugDraw();

        // 记录上一帧位置（便于 CCD / 调试）
        m_prev_position = get_position();
    }

    // 生命周期钩子：在对象被创建并 Start() 被调用（通过 ObjManager::CreateEntry 在 pending 阶段执行）
    // - 覆写 Start 用于初始化资源、注册事件等（应确保异常安全）
    virtual void Start(){}
    // 每帧更新：在 ObjManager::UpdateAll 的 Update 阶段被调用
    virtual void Update(){}

    // 获取当前用于渲染的帧（考虑垂直图集的多行帧）
    // - 返回值为 PngFrame，包含像素与尺寸信息，供渲染层使用
    PngFrame SpriteGetFrame() const
    {
        if (m_vertical_frame_count <= 1) {
            return GetCurrentFrame();
        }
        return GetCurrentFrameWithTotal(m_vertical_frame_count);
    }

    // 垂直帧数（图集多行时使用）
    int SpriteGetVerticalFrameCount() const noexcept { return m_vertical_frame_count; }

    // 控制精灵动画更新频率（以游戏帧为单位）
    // - freq 必须大于 0（否则自动设为 1）
    void SpriteSetUpdateFreq(int freq) noexcept
    {
        m_sprite_update_freq = (freq > 0 ? freq : 1);
        SetFrameDelay(m_sprite_update_freq);
    }
    int SpriteGetUpdateFreq() const noexcept { return m_sprite_update_freq; }

    // 精灵资源接口：设置/清除路径并自动在有资源时注册到 DrawingSequence（实现位于 cpp / 渲染层）
    // - set_shape_aabb 为 true 时会自动基于贴图尺寸设置一个中心对齐的 AABB（便于快速调试/默认碰撞）
    void SpriteSetSource(const std::string& path, int count, bool set_shape_aabb = true) noexcept;
    void SpriteClearPath() noexcept;

    // 查询是否设置了资源路径（可选输出当前路径）
    bool SpriteHasPath(std::string* out_path = nullptr) const noexcept { return HasPath(out_path); }

    int SpriteWidth() const noexcept {
        auto frame = SpriteGetFrame();
		return frame.w * GetScaleX();
	}
    int SpriteHeight() const noexcept {
        auto frame = SpriteGetFrame();
        return frame.h * GetScaleY();
    }

    // 可见性/深度控制（用于统一渲染排序）
    // - 渲染器应使用 IsVisible()/GetDepth() 决定是否绘制与绘制顺序
    void SetVisible(bool v) noexcept { m_visible = v; }
    bool IsVisible() const noexcept { return m_visible; }

    void SetDepth(int d) noexcept { m_depth = d; }
    int GetDepth() const noexcept { return m_depth; }

    // 旋转/翻转/枢轴：精灵层与碰撞层可以选择性同步
    // - SetRotation/Rotate 会更新 PngSprite 的旋转，并在 IsColliderRotate() 启用时同步到 BasePhysics（触发 world-shape 重计算）
    float GetRotation() const noexcept { return PngSprite::GetSpriteRotation(); }
    void SetRotation(float rot) noexcept
    {
        if(rot > pi) rot -= 2 * pi;
		else if (rot < -pi) rot += 2 * pi;
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

    // 精灵翻转 API：仅修改标志，实际渲染层需读取 m_flip_x/m_flip_y 或通过 SpriteGetFlipX/Y 获取
    bool SpriteGetFlipX() { return m_flip_x; }
    bool SpriteGetFlipY() { return m_flip_y; }
    void SpriteFlipX(bool x) { m_flip_x = x; }
    void SpriteFlipY(bool y) { m_flip_y = y; }
    void SpriteFlipX() { m_flip_x = !m_flip_x; }
    void SpriteFlipY() { m_flip_y = !m_flip_y; }
	// 缩放：同步 PngSprite 与 BasePhysics（影响 world-shape 计算）
	float GetScaleX() const noexcept { return m_scale_x; }
    void ScaleX(float sx) noexcept { PngSprite::set_scale_x(sx); BasePhysics::scale_x(sx); m_scale_x = sx; }
	float GetScaleY() const noexcept { return m_scale_y; }
    void ScaleY(float sy) noexcept { PngSprite::set_scale_y(sy); BasePhysics::scale_y(sy); m_scale_y = sy; }
    void Scale(float s) noexcept {
		PngSprite::set_scale_x(s); BasePhysics::scale_x(s); m_scale_x = s;
		PngSprite::set_scale_y(s); BasePhysics::scale_y(s); m_scale_y = s;
	}

    // 枢轴（pivot）相关：
    // - GetPivot 返回像素坐标（相对于贴图中心）
    // - SetPivot 接受相对值（例如 (0,0)=中心, (1,1)=右上），并在 IsColliderApplyPivot 启用时对碰撞器进行微调
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
    // - 返回当前标志；设置标志后会调用 update_world_shape_flag() 来决定是否开启 world-shape 优化
    bool IsColliderRotate() const noexcept { return m_isColliderRotate; }
    bool IsColliderRotate(bool v) noexcept { 
        m_isColliderRotate = v; 
        update_world_shape_flag(); 
		return m_isColliderRotate;
    }

    bool IsColliderApplyPivot() const noexcept { return m_isColliderApplyPivot; }
    bool IsColliderApplyPivot(bool v) noexcept { 
        m_isColliderApplyPivot = v; 
        update_world_shape_flag(); 
		return m_isColliderApplyPivot;
    }

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

    // 获取上一帧位置（用于连续碰撞检测或插值）
    const CF_V2& GetPrevPosition() const noexcept { return m_prev_position; }

    void SetShape(const CF_ShapeWrapper& s) noexcept { set_shape(s); }
    void SetColliderType(ColliderType t) noexcept { set_collider_type(t); }

    // 便捷创建碰撞形状的接口
    void SetAabb(const CF_Aabb& a) noexcept { set_shape(CF_ShapeWrapper::FromAabb(a)); }
    void SetCircle(const CF_Circle& c) noexcept { set_shape(CF_ShapeWrapper::FromCircle(c)); }
    void SetCapsule(const CF_Capsule& c) noexcept { set_shape(CF_ShapeWrapper::FromCapsule(c)); }
    void SetPoly(const CF_Poly& p) noexcept { set_shape(CF_ShapeWrapper::FromPoly(p)); }

    // 快速创建以中心为原点的 AABB / Circle / Capsule 帮助函数（常用于基于 sprite 大小自动设置碰撞） 
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

	// 便捷创建居中 circle
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

    // 便捷按方向创建居中 capsule（dir 将被正规化）
    void SetCenteredCapsule(const CF_V2& dir, float half_length, float radius) noexcept
    {
		CF_V2 dir_normalized = v2math::normalized(dir);
        CF_V2 a{ -dir_normalized.x * half_length, -dir_normalized.y * half_length };
        CF_V2 b{ dir_normalized.x * half_length,  dir_normalized.y * half_length };

        // 确保 a,b 在坐标排序上一致，便于后续处理或调试输出
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

    // 使用局部顶点数组创建多边形（local space）并自动调用 cf_make_poly 以保证合法性
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

    // 碰撞回调阶段枚举：Enter（进入）、Stay（持续）、Exit（退出）
    enum class CollisionPhase : int { Enter = 1, Stay = 0, Exit = -1 };

    // 统一的碰撞状态回调分发：上层可以覆写具体阶段的处理函数
    // - 默认实现将根据 phase 调用 OnCollisionEnter/Stay/Exit，派生类通常重写这些方法
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

    // 逐阶段碰撞回调：默认空实现（no-op），派生类按需实现
    // - 参数 other 为对方的 ObjToken（可以用于通过 ObjManager 查询对方对象或对比 token）
    // - manifold 为碰撞信息（contact points、normal、penetration depths）；Exit 阶段可能收到空的 manifold
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

    // 标签系统：用于快速标记/查询对象（例如 "player", "enemy", "projectile"）
    // - 使用 unordered_set 保证标签唯一，set 操作为 noexcept
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

    // 对象销毁钩子：在对象被 ObjManager 销毁前会调用（包含 pending 被销毁的情况）
    // - 覆写 OnDestroy 用于释放外部资源或取消订阅
    virtual void OnDestroy() noexcept {}

    ~BaseObject() noexcept;

protected:
    // 受保护的 getter，供派生类访问自身的 token（只读）
    // - token 在对象被合并到 ObjManager 后由 ObjManager 设置
    const ObjManager::ObjToken& GetObjToken() const noexcept { return m_obj_token; }

private:
    // 将 BasePhysics 的一组方法私有化别名以便在本类中以小写形式直接调用（减少命名冲突）
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

	// 根据 pivot 调整碰撞器（实现文件内定义），用于当精灵枢轴改变时同步碰撞形状
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

    // 默认将精灵旋转/枢轴应用到碰撞器（可关闭以换取性能或特殊行为）
    bool m_isColliderRotate = true;
    bool m_isColliderApplyPivot = true;

	std::unordered_set<std::string> tags; // 对象标签集合（无序，不重复）

    // 对象在 ObjManager 中的句柄（默认无效）
    ObjManager::ObjToken m_obj_token = ObjManager::ObjToken::Invalid();

    // 仅供 ObjManager 设置/清除 token（友元保证只有 ObjManager 可以直接设置）
    void SetObjToken(const ObjManager::ObjToken& t) noexcept { m_obj_token = t; }

    // 允许 ObjManager 在创建/销毁时设置 token
    friend class ObjManager;

    // 根据是否启用旋转或枢轴决定是否将 local shape 视为 world shape，从而避免重复转换（性能开关）
    void update_world_shape_flag() noexcept
    {
        BasePhysics::enable_world_shape(m_isColliderRotate || m_isColliderApplyPivot);
    }
};