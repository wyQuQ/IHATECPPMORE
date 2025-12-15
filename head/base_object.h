#pragma once
#include "base_physics.h"
#include "cute_sprite.h" // 使用 CF_Sprite
#include "cute_math.h"   // For cf_sincos, cf_atan2
#include <string>
#include <utility>
#include <vector> 
#include <iostream> 
#include <cmath> 
#include <unordered_set> 

#include "obj_manager.h"
#include "debug_config.h"
#include "input.h"

/*
 * BaseObject.h — 对场景中可实例化对象的高层封装。
 *
 * 说明（概览）：
 * - `BaseObject` 将可渲染的 `CF_Sprite` 与物理表示 `BasePhysics` 组合，提供常用的安全公开接口：
 *   - 渲染/精灵控制：设置图片路径、帧率、翻转、缩放、旋转、可见性、深度等；
 *   - 物理控制（受限暴露）：位置/速度/力的安全 getter/setter、应用物理更新（Apply*）等；
 *   - 碰撞回调：`OnCollisionEnter/Stay/Exit` 及 `OnCollisionState`；
 *   - 碰撞形状设置：推荐使用 `SetCentered*` 系列构造常见形状；低级 `SetShape` 等标记为 BARE_SHAPE_API（不推荐，危险）；
 *   - 标签系统：`AddTag/HasTag/RemoveTag` 用于轻量标识对象；
 *   - 生命周期钩子：`Start/Update/EndFrame/OnDestroy`。
 *
 * 安全与使用指导（总体）：
 * - 推荐通过 `SetCenteredAabb/SetCenteredCircle/SetCenteredCapsule/SetCenteredPoly` 来设置碰撞体，
 *   这些方法会在局部坐标系下构造合理的形状，并与 BasePhysics 协同工作。
 * - 对涉及“每帧物理更新”的方法（被 `APPLIANCE` 标记）应谨慎调用：这些方法在框架主循环里已经被调用一次，
 *   仅当你确实需要在单帧内多次手动推进物理状态（例如子步模拟）时才手动调用，否则可能会导致非预期物理行为。
 * - 低级形状 API（被 `BARE_SHAPE_API` 标记）绕过了某些安全/一致性检查；仅在你从外部构建好 `CF_ShapeWrapper`
 *   并确实想把它原样复制到对象时使用。否则请使用 `SetCentered*`。
 * - `BasePhysics` 的常用成员被在 `BaseObject` 中降为私有 using，目的是防止派生类未经显式限定名直接调用导致误用。
 *   如果确实需要访问底层 `BasePhysics` 的方法，须显式限定（例如 `BasePhysics::set_position(...)`）。
 */

/* 警告/弃用宏说明（保留在头部以便阅读）：
 * - BARE_SHAPE_API：标注低级形状接口为弃用，提示使用 SetCentered* 系列替代。
 * - APPLIANCE：标注那些会在每帧中改变物理量的方法（如 ApplyForce/ApplyVelocity），提示谨慎使用。
 */
#ifndef BARE_SHAPE_API
#define BARE_SHAPE_API [[deprecated("Bare Shape API: 低级接口，请在 已在外部获取ShapeWrapper并需要直接复制到该物体 的情况下再使用，否则推荐使用SetCentered*来设置碰撞箱形状。")]]
#endif

#ifndef APPLIANCE
#define APPLIANCE [[deprecated("APPLIANCE: 涉及物理量的每帧更新，已在类内部完成。除非你需要单帧内多次更新，否则请勿使用该接口。")]]
#endif

class BaseObject;
void RenderBaseObjectCollisionDebug(const BaseObject* obj) noexcept;
void ManifoldDrawDebug(const CF_Manifold& m) noexcept;

inline ObjManager& objs = ObjManager::Instance();

class BaseObject : public BasePhysics {
public:
    // 构造：初始化物理量与精灵默认值，确保对象处于有效初始状态。
    BaseObject() noexcept
        : BasePhysics()
    {
        // 物理基础值：位置/速度/力 清零
        set_position(CF_V2{ 0.0f, 0.0f });
        set_velocity(CF_V2{ 0.0f, 0.0f });
        set_force(CF_V2{ 0.0f, 0.0f });

        // 保证上一帧位置与当前位置一致，避免首帧大位移引发错误碰撞修正
        m_prev_position = get_position();

        // 帧延迟/world-shape 标志初始化
        update_world_shape_flag();

        // 强制把 world-shape 与当前物理状态同步，避免初始碰撞形状基于未同步的缓存数据
        force_update_world_shape();

        tags.reserve(5);
        m_sprite = cf_sprite_defaults();
    }

    // 生命周期钩子：在对象被添加到场景/管理器后调用（引擎层面）。派生类可覆盖以初始化资源。
    virtual void Start() {}

    /*
     * FrameEnterApply
     * 场景主循环中“进入帧物理更新”时调用（框架会在正确时机调用）。
     * 功能：
     * - 缓存上一帧位置（用于碰撞检测/插值/轨迹计算）
     * - 先应用力（ApplyForce），再应用速度（ApplyVelocity）
     * 注意：
     * - 该方法被标注为 APPLIANCE（弃用提示）——意味着引擎会自动调用，通常不应由用户手动在每帧调用，
     *   除非你确实需要在单帧中进行多次物理子步推进。
     */
    APPLIANCE void FrameEnterApply() noexcept
    {
		m_collide_manifolds.clear();
        m_collide_manifolds.reserve(4);
		StartFrame();
        ApplyForce();
        ApplyVelocity();
    }

    // 碰撞回调：派生类按需重载，都是 noexcept，建议不要抛异常
    // - OnCollisionEnter：第一次检测到碰撞时调用（Enter）
    // - OnCollisionStay：持续发生碰撞的帧中调用（Stay）
    // - OnCollisionExit：碰撞结束时调用（Exit）
    // 参数说明：
    // - other：对方对象的 ObjToken（注意：token 可能失效，若需保证有效请在 ObjManager 中检查）
    // - manifold：碰撞信息（接触点、法线、穿透深度等）
    virtual void OnCollisionEnter(const ObjManager::ObjToken& other, const CF_Manifold& manifold) noexcept {}
    virtual void OnCollisionStay(const ObjManager::ObjToken& other, const CF_Manifold& manifold) noexcept {}
    virtual void OnCollisionExit(const ObjManager::ObjToken& other, const CF_Manifold& manifold) noexcept {}

	// 排斥碰撞回调：派生类按需重载
	virtual void OnExclusionSolid(const ObjManager::ObjToken& other, const CF_Manifold& manifold) noexcept {}

    // 每帧更新钩子：由场景主循环调用，派生类在这里执行行为逻辑/AI/输入响应等。
    virtual void Update() {}

    /*
     * FrameExitApply
     * 场景主循环中“退出帧处理”时调用。
     * 功能：
     * - 如果调用者在本帧对位置使用了缓冲设置（SetPosition(..., buffer=true)），此处会合并目标位置并应用
     *   （选择与当前坐标差绝对值最大的偏移分别应用到 x/y）。
     * - 调用 EndFrame（供派生类扩展）
     * - 根据配置绘制调试（DebugDraw）
     *
     * 注意：
     * - 缓冲位置机制用于收集在一帧内的多个目标位置并在帧尾一次性合并应用，避免中途干扰物理求解。
     */
    APPLIANCE void FrameExitApply() noexcept
    {
        EndFrame();
        m_prev_position = get_position();
	}

	// 供派生类在帧首执行初始化/状态准备，默认为空
	virtual void StartFrame() {}
	// 供派生类在帧尾执行清理/状态同步，默认为空
	virtual void EndFrame() {}

    // 精灵像素宽/高（考虑缩放），注意返回值是 int（像素级）
    int SpriteWidth() const noexcept {
        return m_sprite.w;
	}
    int SpriteHeight() const noexcept {
        return m_sprite.h / m_sprite_vertical_frame_count;
    }

    // 可见性控制：用于渲染层判断是否跳过绘制（不会影响物理/碰撞）
    void SetVisible(bool v) noexcept { m_visible = v; }
    bool IsVisible() const noexcept { return m_visible; }

    // 深度控制（渲染顺序），数值越大/小的语义由渲染器决定
    void SetDepth(int d) noexcept { m_depth = d; }
    int GetDepth() const noexcept { return m_depth; }

    // 旋转与旋转策略：
    // - GetRotation 返回当前精灵旋转（弧度，[-pi,pi]）
    // - SetRotation 会将传入角度限定到 [-pi,pi] 范围内，并在 IsColliderRotate 为 true 时同步到碰撞体并标记 world shape 脏
    // - Rotate 增量旋转，同样会在 IsColliderRotate 为 true 时更新碰撞体旋转
    float GetRotation() const noexcept { return cf_atan2(m_sprite.transform.r.s, m_sprite.transform.r.c); }
    void SetRotation(float rot) noexcept
    {
        if(rot > 3.1415926535f) rot -= 2 * 3.1415926535f;
		else if (rot < -3.1415926535f) rot += 2 * 3.1415926535f;
        m_sprite.transform.r = cf_sincos(rot);
        if (IsColliderRotate()) {
            BasePhysics::set_rotation(rot);
            BasePhysics::mark_world_shape_dirty();
        }
    }
    void Rotate(float drot) noexcept
    {
        float new_rot = GetRotation() + drot;
        SetRotation(new_rot);
    }

    // 精灵翻转控制（仅影响渲染，若需要影响碰撞请自行处理）
    bool SpriteGetFlipX() const { return m_sprite.scale.x < 0; }
    bool SpriteGetFlipY() const { return m_sprite.scale.y < 0; }
    void SpriteFlipX(bool x) {
        float current_scale_x = std::abs(m_sprite.scale.x);
        m_sprite.scale.x = x ? -current_scale_x : current_scale_x;
    }
    void SpriteFlipY(bool y) {
        float current_scale_y = std::abs(m_sprite.scale.y);
        m_sprite.scale.y = y ? -current_scale_y : current_scale_y;
    }

    // 缩放控制：ScaleX/ScaleY 会同时同步到底层 BasePhysics（影响碰撞形状缩放）
	float GetScaleX() const noexcept { return m_sprite.scale.x; }
    void ScaleX(float sx) noexcept { 
        m_sprite.scale.x = sx;
        BasePhysics::scale_x(sx); 
    }
	float GetScaleY() const noexcept { return m_sprite.scale.y; }
    void ScaleY(float sy) noexcept { 
        m_sprite.scale.y = sy;
        BasePhysics::scale_y(sy); 
    }
    void Scale(float s) noexcept {
		m_sprite.scale = cf_v2(s, s);
        BasePhysics::scale_x(s);
        BasePhysics::scale_y(s);
	}

    // 枢轴（pivot）控制：以相对（-1..1）或绝对像素值设置中心点
    CF_V2 GetPivot() const noexcept { return m_sprite.offset; }
    /*
     * SetPivot
     *  - x_rel, y_rel：相对于精灵中心的比例（乘以半宽/半高得到局部坐标）
     *  - 为保证 collider 与 pivot 协同工作：函数内部先将缩放归 1 以便正确计算像素 pivot，
     *    若 IsColliderApplyPivot 为真，会调用 TweakColliderWithPivot 对碰撞体进行相应偏移，
     *    并将 pivot 值同步给 BasePhysics（随后恢复原缩放）。
     *
     * 注意：
     * - SetPivot 影响渲染锚点和（在设置了 IsColliderApplyPivot 时）碰撞体位置的局部偏移，
     *   若你只想改变渲染对齐而不改变碰撞体，请禁用 IsColliderApplyPivot。
     */
    void SetPivot(float x_rel, float y_rel) noexcept {
		float scale_x = GetScaleX();
		float scale_y = GetScaleY();
        Scale(1.0f);
        float hw = SpriteWidth() / 2.0f;
        float hh = SpriteHeight() / 2.0f;
		CF_V2 p = CF_V2{ x_rel * hw, y_rel * hh };
        m_sprite.offset = -p;
        if (IsColliderApplyPivot()) {
            TweakColliderWithPivot(p);
            BasePhysics::set_pivot(p);
            BasePhysics::mark_world_shape_dirty();
        }
		ScaleX(scale_x);
		ScaleY(scale_y);
    }

    // 组合设置：路径 + 垂直帧数 + 更新频率 + 深度（便捷）
    void SpriteSetStats(const std::string& path, int vertical_frame_count, int update_freq, int depth, bool set_shape_aabb = true) noexcept;
    // 新增：设置精灵源（向后兼容）
    void SpriteSetSource(const std::string& path, int vertical_frame_count, bool set_shape_aabb = true) noexcept;
    // 新增：设置精灵更新频率（向后兼容）
    void SpriteSetUpdateFreq(int update_freq) noexcept;

    // 碰撞体旋转/应用 pivot 的策略开关：
    // - IsColliderRotate(true/false)：若为 true，同步 sprite 的旋转到物理碰撞体（常用于角色随朝向旋转时碰撞体也跟随）
    // - IsColliderApplyPivot(true/false)：若为 true，pivot 改变会影响碰撞体的局部位置
    // 说明：切换这些标志会调用 update_world_shape_flag，决定是否启用 world shape 计算（性能相关）。
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

    // 物理/碰撞状态访问（只读）
    const CF_V2& GetPosition() const noexcept { return get_position(); }
    const CF_V2& GetPrevPosition() const noexcept { return m_prev_position; }
    const CF_V2& GetVelocity() const noexcept { return get_velocity(); }
    const CF_V2& GetForce() const noexcept { return get_force(); }
    const CF_ShapeWrapper& GetShape() const noexcept { return get_shape(); }
    ColliderType GetColliderType() const noexcept { return get_collider_type(); }

    /*
     * SetPosition
     * - buffer=false（默认）：立刻设置位置（直接调用 BasePhysics::set_position）
     * - buffer=true：将位置放入 m_target_position 列表，直到 FrameExitApply 在帧尾统一合并并应用
     * 场景下的使用建议：
     * - 若你在一帧内多处逻辑可能设置对象位置，使用 buffer=true 可避免中间状态对碰撞/物理的即时影响，
     *   框架会在帧尾按策略合并（选择 x/y 上最大的偏移）。
     */
    void SetPosition(const CF_V2& p) noexcept { set_position(p); }
    void SetVelocity(const CF_V2& v) noexcept { set_velocity(v); }
    void SetVelocityX(float vx) noexcept { set_velocity_x(vx); }
	void SetVelocityY(float vy) noexcept { set_velocity_y(vy); }
    void SetForce(const CF_V2& f) noexcept { set_force(f); }
	void SetForceX(float fx) noexcept { set_force_x(fx); }
	void SetForceY(float fy) noexcept { set_force_y(fy); }
    void AddVelocity(const CF_V2& dv) noexcept { add_velocity(dv); }
    void AddForce(const CF_V2& df) noexcept { add_force(df); }

    // ApplyVelocity/ApplyForce：直接将力/速度应用到物理步进（APPLIANCE 标记）
    // 使用场景：仅当需要手动推进物理（例如子步、调试、特殊时间缩放）时调用，常规帧不需要主动调用。
    APPLIANCE void ApplyVelocity(float dt = 1) noexcept { apply_velocity(dt); }
    APPLIANCE void ApplyForce(float dt = 1) noexcept { apply_force(dt); }

    // 碰撞类型设置（影响如何参与碰撞分组/判定）
    void SetColliderType(ColliderType t) noexcept { set_collider_type(t); }

    /*
     * SetCentered*
     * 推荐使用的碰撞体构造器：在对象局部坐标系以中心为原点创建形状。
     * 这些方法会创建并调用内部的 set_shape（同步到底层 BasePhysics）。
     *
     * 使用建议与注意事项：
     * - 参数使用局部尺寸（半宽/半高、半长、半径、顶点局部坐标等）
     * - 若需要基于精灵大小自动设置 AABB，可在调用 SpriteSetSource 时传入 set_shape_aabb = true
     * - SetCenteredCapsule 中 `dir` 会被归一化；函数内部对端点顺序做了处理以确保多边形/胶囊定义的一致性
     */
    void SetCenteredAabb(float half_w, float half_h) noexcept
    {
        CF_Aabb aabb{};
        aabb.min.x = -half_w;
        aabb.min.y = -half_h;
        aabb.max.x = half_w;
        aabb.max.y = half_h;
        set_shape(CF_ShapeWrapper::FromAabb(aabb));
    }

    void SetCenteredCircle(float radius) noexcept
    {
        CF_Circle c{};
        c.p = CF_V2{ 0.0f, 0.0f };
        c.r = radius;
        set_shape(CF_ShapeWrapper::FromCircle(c));
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
    }

    void SetCenteredPoly(const std::vector<CF_V2>& localVerts) noexcept
    {
        CF_Poly p{};
        int n = static_cast<int>(localVerts.size());
        p.count = n;
        for (int i = 0; i < n; ++i) {
            p.verts[i] = CF_V2{ localVerts[i].x, localVerts[i].y };
        }
        cf_make_poly(&p);
        set_shape(CF_ShapeWrapper::FromPoly(p));
    }

    /*
     * 低级形状 API（BARE_SHAPE_API 标注：不推荐常用）
     * - SetShape / SetAabb / SetCircle / SetCapsule / SetPoly
     * 用途：
     * - 当你已经在外部完整构造了一个 `CF_ShapeWrapper`/原始形状并希望直接赋值到对象时使用。
     * 风险：
     * - 可能绕过某些便捷的坐标/缩放/pivot 同步逻辑，导致碰撞体与渲染不同步或 world-shape 标识失效。
     */
    BARE_SHAPE_API    void SetShape(const CF_ShapeWrapper& s) noexcept { set_shape(s); }
    BARE_SHAPE_API    void SetAabb(const CF_Aabb& a) noexcept { set_shape(CF_ShapeWrapper::FromAabb(a)); }
    BARE_SHAPE_API    void SetCircle(const CF_Circle& c) noexcept { set_shape(CF_ShapeWrapper::FromCircle(c)); }
    BARE_SHAPE_API    void SetCapsule(const CF_Capsule& c) noexcept { set_shape(CF_ShapeWrapper::FromCapsule(c)); }
    BARE_SHAPE_API    void SetPoly(const CF_Poly& p) noexcept { set_shape(CF_ShapeWrapper::FromPoly(p)); }

	void ExcludeWithSolids(bool v) noexcept { m_exclude_with_solid = v; }

	// 碰撞检测：检测与另一个对象是否碰撞，若碰撞则输出碰撞信息到 out_m，否则 out_m 置空
    bool IsCollidedWith(const BaseObject& other, CF_Manifold& out_m) noexcept;


    // 碰撞相位枚举：用于统一的 OnCollisionState 调用
    enum class CollisionPhase : int { Enter = 1, Stay = 0, Exit = -1 };

    /*
     * OnCollisionState
     * 统一的碰撞状态分发器，根据相位调用相应的回调函数。
     * 使用建议：
     * - 引擎或管理器会调用此函数；派生类一般只需实现 OnCollisionEnter/Stay/Exit。
     */
    APPLIANCE void OnCollisionState(const ObjManager::ObjToken& other, const CF_Manifold& manifold, CollisionPhase phase) noexcept;

#if SHAPE_DEBUG
    // 调试绘制：如果启用宏则调用全局调试渲染函数
    void ShapeDraw() const noexcept
    {
        RenderBaseObjectCollisionDebug(this);
    }
#else
    inline void ShapeDraw() const noexcept {}
#endif
#if COLLISION_DEBUG
	// 调试绘制：如果启用宏则调用全局调试渲染函数
    void ManifoldDraw(const CF_Manifold& m) const noexcept
    {
        ManifoldDrawDebug(m);
	}
#else
	inline void ManifoldDraw(const CF_Manifold& m) const noexcept {}
#endif

    // 轻量标签 API：用于快速标识/查询对象（非线程安全）
    // - AddTag：添加标记（重复添加无效）
    // - HasTag：判断是否存在标记
    // - RemoveTag：移除标记
    void AddTag(const std::string& tag) noexcept { tags.insert(tag); }

    bool HasTag(const std::string& tag) const noexcept { return tags.find(tag) != tags.end(); }

    void RemoveTag(const std::string& tag) noexcept { tags.erase(tag); }

    // 对象销毁钩子：在对象被销毁前由管理器调用，派生类可重载以释放资源
    virtual void OnDestroy() noexcept {}

    ~BaseObject() noexcept;

    // 新增：获取底层 CF_Sprite 的 const 引用
    const CF_Sprite& GetSprite() const { return m_sprite; }
    // 新增：获取底层 CF_Sprite 的可变引用
    CF_Sprite& GetSprite() { return m_sprite; }

protected:
    // 获取当前对象在 ObjManager 中的 token（受保护，仅供派生类安全读取）
    const ObjManager::ObjToken& GetObjToken() const noexcept { return m_obj_token; }

private:
    friend class ObjManager;
    friend class DrawingSequence;

    // 说明：将 BasePhysics 的常用方法在 BaseObject 中设为私有，阻止派生类未限定名调用。
    // 目的：
    // - 防止派生类显式调用底层方法，避免误用自动同步/脏标记机制。
    // - BaseObject 内部仍可直接使用（private 可见）。
    using BasePhysics::set_position;
    using BasePhysics::get_position;
    using BasePhysics::set_velocity;
    using BasePhysics::get_velocity;
    using BasePhysics::set_force;
    using BasePhysics::get_force;
    using BasePhysics::add_velocity;
    using BasePhysics::add_force;
    using BasePhysics::apply_velocity;
    using BasePhysics::apply_force;

    using BasePhysics::set_shape;
    using BasePhysics::get_shape;
    using BasePhysics::get_local_shape;

    using BasePhysics::set_collider_type;
    using BasePhysics::get_collider_type;

    using BasePhysics::set_rotation;
    using BasePhysics::get_rotation;
    using BasePhysics::set_pivot;
    using BasePhysics::get_pivot;
    using BasePhysics::scale_x;
    using BasePhysics::get_scale_x;
    using BasePhysics::scale_y;
    using BasePhysics::get_scale_y;

    using BasePhysics::enable_world_shape;
    using BasePhysics::is_world_shape_enabled;
    using BasePhysics::force_update_world_shape;
    using BasePhysics::world_shape_version;
    using BasePhysics::mark_world_shape_dirty;

	// 内部工具：根据 pivot 微调碰撞体；实现细节在 cpp 文件中（供 SetPivot 调用）
	void TweakColliderWithPivot(const CF_V2& pivot) noexcept;

    CF_Sprite m_sprite{}; // 使用框架的 CF_Sprite
    bool m_visible = true;
    int m_depth = 0;
    // 新增：用于支持 SpriteSetUpdateFreq
    std::string m_sprite_path;
    int m_sprite_vertical_frame_count = 1;
    int m_sprite_current_frame_index = 0;
	int m_sprite_update_freq = 1; // 每多少帧更新一次
    int m_sprite_last_update_frame = 0;

    CF_V2 m_prev_position = CF_V2{ 0.0f, 0.0f };

    bool m_isColliderRotate = true;
    bool m_isColliderApplyPivot = true;
	bool m_exclude_with_solid = false;
    CF_Manifold ExclusionWithSolid(const ObjManager::ObjToken& oth, const CF_Manifold& m) noexcept;
    void FindContactPos(CF_V2 current, CF_V2 offset, const BaseObject& other, CF_Manifold& res);
    std::vector<CF_Manifold> m_collide_manifolds;

	std::unordered_set<std::string> tags;

    ObjManager::ObjToken m_obj_token = ObjManager::ObjToken::Invalid();

    void SetObjToken(const ObjManager::ObjToken& t) noexcept { m_obj_token = t; }

    // 根据 collider 的旋转 / pivot 应用标志决定是否启用 world shape 计算（性能相关）
    void update_world_shape_flag() noexcept
    {
        BasePhysics::enable_world_shape(m_isColliderRotate || m_isColliderApplyPivot);
    }
};