#pragma once
#include <cute.h>
#include <vector>
#include <unordered_map>
#include <cstdint>

#include "obj_manager.h"
#include "v2math.h"

// CF_ShapeWrapper 封装了不同类型的碰撞形状（AABB, Circle, Capsule, Poly），
// 并提供静态工厂函数便于创建对应的包装类型。
struct CF_ShapeWrapper
{
	CF_ShapeType type;
	union
	{
		CF_Aabb aabb;
		CF_Circle circle;
		CF_Capsule capsule;
		CF_Poly poly;
	} u;

	// 工厂函数：从具体的 CF_* 结构创建包装类型
	static CF_ShapeWrapper FromAabb(const CF_Aabb& a) { CF_ShapeWrapper s{}; s.type = CF_SHAPE_TYPE_AABB; s.u.aabb = a; return s; }
	static CF_ShapeWrapper FromCircle(const CF_Circle& c) { CF_ShapeWrapper s{}; s.type = CF_SHAPE_TYPE_CIRCLE; s.u.circle = c; return s; }
	static CF_ShapeWrapper FromCapsule(const CF_Capsule& c) { CF_ShapeWrapper s{}; s.type = CF_SHAPE_TYPE_CAPSULE; s.u.capsule = c; return s; }
	static CF_ShapeWrapper FromPoly(const CF_Poly& p) { CF_ShapeWrapper s{}; s.type = CF_SHAPE_TYPE_POLY; s.u.poly = p; return s; }
};

enum class ColliderType {
	VOID, // 不参与碰撞
	LIQUID, // 液体样碰撞（可定制行为）
	SOLID // 实体碰撞（常规碰撞）
};

// 前置声明：BasePhysics 提供给上层对象一个统一的物理属性/形状接口
class BasePhysics;

// PhysicsSystem 提供面向使用者的物理子系统入口：
// - 注册/反注册 BasePhysics 实例（通过 ObjToken 关联对象生命周期）
// - Step() 在每帧执行 broadphase -> narrowphase -> 事件合并 -> Enter/Stay/Exit 回调阶段
// 使用建议：在主循环中调用 ObjManager::UpdateAll()，其内部会调用 PhysicsSystem::Step()
class PhysicsSystem {
public:
	struct CollisionEvent {
		ObjManager::ObjToken a;
		ObjManager::ObjToken b;
		CF_Manifold manifold{};
	};

	static PhysicsSystem& Instance() noexcept
	{
		static PhysicsSystem inst;
		return inst;
	}

	// 将 BasePhysics 实例加入物理系统以参与碰撞检测（通常在对象 Start() 时调用）
	void Register(const ObjManager::ObjToken& token, BasePhysics* phys) noexcept;

	// 从系统中移除指定 token 的物理条目（通常在对象销毁前调用）
	void Unregister(const ObjManager::ObjToken& token) noexcept;

	// 每帧推进物理系统（cell_size 可调整 broadphase 网格规模，默认 64.0f）
	void Step(float cell_size = 64.0f) noexcept;

private:
	PhysicsSystem() noexcept = default;
	~PhysicsSystem() noexcept = default;

	struct Entry {
		ObjManager::ObjToken token;
		BasePhysics* physics = nullptr;
		int32_t grid_x = 0;
		int32_t grid_y = 0;
	};

	// 将 (index,generation) 编码为 uint64_t，以便与 ObjManager 的 token 匹配
	static uint64_t make_key(const ObjManager::ObjToken& t) noexcept
	{
		return (static_cast<uint64_t>(t.index) << 32) | static_cast<uint64_t>(t.generation);
	}

	static uint64_t grid_key(int32_t x, int32_t y) noexcept
	{
		return (static_cast<uint64_t>(static_cast<uint32_t>(x)) << 32) | static_cast<uint64_t>(static_cast<uint32_t>(y));
	}

	std::vector<Entry> entries_;
	std::unordered_map<uint64_t, size_t> token_map_; // token_key -> entries_ 索引

	std::unordered_map<uint64_t, std::vector<size_t>> grid_; // broadphase 网格映射

	std::vector<CollisionEvent> events_;

	// 保存上一帧的碰撞对，用于生成 Enter / Exit 事件
	std::unordered_map<uint64_t, std::pair<ObjManager::ObjToken, ObjManager::ObjToken>> prev_collision_pairs_;

	// 每帧使用的 world-shape 缓存与临时容器
	std::vector<CF_ShapeWrapper> world_shapes_;
	std::vector<uint64_t> grid_keys_used_; // 记录已分配的 grid 键以便复用容器

	// 合并与临时存储结构
	std::unordered_map<uint64_t, CollisionEvent> merged_map_;
	std::vector<uint64_t> merged_order_;
	std::unordered_map<uint64_t, std::pair<ObjManager::ObjToken, ObjManager::ObjToken>> current_pairs_;
};

// BasePhysics 为可碰撞对象提供通用的物理属性与形状管理接口：
// - 管理 position/velocity/force 等基础状态，并提供便捷的 set/get 接口。
// - 保存局部 shape（local-space），并能根据旋转/枢轴转换为 world-space（get_shape）。
// - 支持 enable_world_shape 来指示 shape 是否已经是 world-space，从而跳过重复计算以提高性能。
// - 提供 world_shape_version() 与 mark_world_shape_dirty() 以便上层高效检测形状变化。
class BasePhysics {
private:
	CF_V2 _position;
	CF_V2 _velocity;
	CF_V2 _force;

	CF_ShapeWrapper shape; // 本地空间形状
	ColliderType collider_type = ColliderType::LIQUID; // 默认碰撞类型

	// 旋转与枢轴参数（用于计算 world-space 形状）
	float rotation_ = 0.0f;
	CF_V2 pivot_{ 0.0f, 0.0f };
	float scale_x_ = 1.0f;
	float scale_y_ = 1.0f;

	// 是否启用 world-space 形状（若启用，get_shape 直接返回已处理的 world shape）
	bool use_world_shape_ = false;

	// world-shape 缓存与版本控制（mutable 以支持 const get_shape）
	mutable CF_ShapeWrapper cached_world_shape_;
	mutable bool world_shape_dirty_ = true;
	mutable uint64_t world_shape_version_ = 0;

	// 负责把 local shape 转换为 world-space 的具体实现（在 cpp 中定义）
	void tweak_shape_with_rotation() const noexcept;

public:
	BasePhysics() noexcept
		: _position{ 0.0f, 0.0f }
		, _velocity{ 0.0f, 0.0f }
		, _force{ 0.0f, 0.0f }
		, shape{}
		, cached_world_shape_{}
	{
	}

	virtual ~BasePhysics() noexcept = default;

	// 位置/速度/力 的基本操作接口
	void set_position(const CF_V2& p) { _position = p; world_shape_dirty_ = true; }
	const CF_V2& get_position() const { return _position; }
	void apply_velocity(float dt)
	{
		_position.x += _velocity.x * dt;
		_position.y += _velocity.y * dt;
		world_shape_dirty_ = true;
	}

	void set_velocity(const CF_V2& v) { _velocity = v; }
	const CF_V2& get_velocity() const { return _velocity; }
	void apply_force(float dt)
	{
		_velocity.x += _force.x * dt;
		_velocity.y += _force.y * dt;
	}
	void add_velocity(const CF_V2& dv)
	{
		_velocity.x += dv.x;
		_velocity.y += dv.y;
	}

	void set_force(const CF_V2& f) { _force = f; }
	const CF_V2& get_force() const { return _force; }
	void add_force(const CF_V2& df)
	{
		_force.x += df.x;
		_force.y += df.y;
	}

	// 碰撞类型接入
	void set_collider_type(ColliderType t) { collider_type = t; }
	ColliderType get_collider_type() const { return collider_type; }

	// 设置/获取本地形状；get_shape 会返回 world-space 的已处理形状（可能触发计算）
	void set_shape(const CF_ShapeWrapper& s) { shape = s; world_shape_dirty_ = true; }
	const CF_ShapeWrapper& get_shape() const
	{
		if (world_shape_dirty_) tweak_shape_with_rotation();
		return cached_world_shape_;
	}

	// rotation / pivot 接口
	void set_rotation(float r) noexcept { rotation_ = r; world_shape_dirty_ = true; }
	float get_rotation() const noexcept { return rotation_; }

	void set_pivot(const CF_V2& p) noexcept { pivot_ = p; world_shape_dirty_ = true; }
	CF_V2 get_pivot() const noexcept { return pivot_; }

	void scale_x(float sx) noexcept { scale_x_ = sx; world_shape_dirty_ = true; }
	float get_scale_x() const noexcept { return scale_x_; }
	void scale_y(float sy) noexcept { scale_y_ = sy; world_shape_dirty_ = true; }
	float get_scale_y() const noexcept { return scale_y_; }

	void enable_world_shape(bool enable) noexcept { use_world_shape_ = enable; world_shape_dirty_ = true; }
	bool is_world_shape_enabled() const noexcept { return use_world_shape_; }

	void force_update_world_shape() const noexcept { tweak_shape_with_rotation(); }

	uint64_t world_shape_version() const noexcept { return world_shape_version_; }

	void mark_world_shape_dirty() noexcept { world_shape_dirty_ = true; }

	const CF_ShapeWrapper& get_local_shape() const noexcept { return shape; }

};