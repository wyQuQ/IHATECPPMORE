#pragma once
#include <cute.h>
#include <vector>
#include <unordered_map>
#include <cstdint>
#include <cmath>
#include <algorithm>

#include "obj_manager.h"

// 简洁的“通用形状”封装，用于在 BasePhysics 中接收和处理各种碰撞体。
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

	// 工厂方法（按值创建）
	static CF_ShapeWrapper FromAabb(const CF_Aabb& a) { CF_ShapeWrapper s{}; s.type = CF_ShapeType::CF_SHAPE_TYPE_AABB; s.u.aabb = a; return s; }
	static CF_ShapeWrapper FromCircle(const CF_Circle& c) { CF_ShapeWrapper s{}; s.type = CF_ShapeType::CF_SHAPE_TYPE_CIRCLE; s.u.circle = c; return s; }
	static CF_ShapeWrapper FromCapsule(const CF_Capsule& c) { CF_ShapeWrapper s{}; s.type = CF_ShapeType::CF_SHAPE_TYPE_CAPSULE; s.u.capsule = c; return s; }
	static CF_ShapeWrapper FromPoly(const CF_Poly& p) { CF_ShapeWrapper s{}; s.type = CF_ShapeType::CF_SHAPE_TYPE_POLY; s.u.poly = p; return s; }
};

enum class ColliderType {
	VOID, // 表示虚碰撞体
	LIQUID, // 表示液体碰撞体，可穿透但有特殊交互
	SOLID // 表示实心碰撞体，标准碰撞处理
};

// 2D 向量辅助函数
namespace v2math {
	inline float length(const CF_V2& vect) noexcept { return cf_sqrt(vect.x * vect.x + vect.y * vect.y); }
	inline CF_V2 normalized(const CF_V2& vect) noexcept
	{
		const float len = length(vect);
		if (len == 0.0f) {
			return CF_V2{ 0.0f, 0.0f };
		}
		return CF_V2{ vect.x / len, vect.y / len };
	}
	inline float cross(const CF_V2& v1, const CF_V2& v2) noexcept { return v1.x * v2.y - v1.y * v2.x; }
	inline float dot(const CF_V2& v1, const CF_V2& v2) noexcept { return v1.x * v2.x + v1.y * v2.y; }
}

// 前向声明（避免包含 BaseObject）
class BasePhysics;

// ---- 轻量碰撞系统（单例） ----
// 目标：尽量减少每帧开销，使用简单的空间哈希（grid）做 broadphase，
// 在 narrowphase 使用已有的 BasePhysics::collides_with，检测结束后统一派发事件。
//
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

	// 注册：将物体及其 BasePhysics 指针加入检测集合（Create 后调用）
	void Register(const ObjManager::ObjToken& token, BasePhysics* phys) noexcept;

	// 注销：在对象销毁前调用（Destroy 前调用，以避免回调访问已销毁对象）
	void Unregister(const ObjManager::ObjToken& token) noexcept;

	// 每帧执行：broadphase -> narrowphase -> 收集事件 -> 统一派发
	// cell_size 可调节以获得更好性能（默认 64.0f）
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

	// 将 (index,generation) 编码为 uint64_t key，与 InstanceController 保持一致风格
	static uint64_t make_key(const ObjManager::ObjToken& t) noexcept
	{
		return (static_cast<uint64_t>(t.index) << 32) | static_cast<uint64_t>(t.generation);
	}

	static uint64_t grid_key(int32_t x, int32_t y) noexcept
	{
		return (static_cast<uint64_t>(static_cast<uint32_t>(x)) << 32) | static_cast<uint64_t>(static_cast<uint32_t>(y));
	}

	std::vector<Entry> entries_;
	std::unordered_map<uint64_t, size_t> token_map_; // token_key -> index in entries_

	std::unordered_map<uint64_t, std::vector<size_t>> grid_; // grid_key -> indices

	std::vector<CollisionEvent> events_;

	// 新增：保存上一帧的碰撞对（key -> 带顺序的 token pair），用于检测 Enter / Exit
	// 使用稳定但顺序无关的 pair_key 生成方式（Step 内负责生成）
	std::unordered_map<uint64_t, std::pair<ObjManager::ObjToken, ObjManager::ObjToken>> prev_collision_pairs_;

	// 可复用的 per-frame 容器，避免每帧重新分配
	std::vector<CF_ShapeWrapper> world_shapes_;
	std::vector<uint64_t> grid_keys_used_; // 记录本帧被写入的 grid keys，用于在下一帧清空桶但保留 capacity

	// 复用的合并容器/临时表
	std::unordered_map<uint64_t, CollisionEvent> merged_map_;
	std::vector<uint64_t> merged_order_;
	std::unordered_map<uint64_t, std::pair<ObjManager::ObjToken, ObjManager::ObjToken>> current_pairs_;
};

// 基础物理类：持有位置/速度/力，并提供通用碰撞查询接口。
class BasePhysics {
private:
	CF_V2 _position;
	CF_V2 _velocity;
	CF_V2 _force;

	CF_ShapeWrapper shape; // 当前物体的碰撞形状（如果有）
	ColliderType collider_type = ColliderType::LIQUID; // 碰撞体类型，默认为液体

public:
	// 构造：将物理量初始化为零，确保对象处于确定状态
	BasePhysics() noexcept
		: _position{ 0.0f, 0.0f }
		, _velocity{ 0.0f, 0.0f }
		, _force{ 0.0f, 0.0f }
		, shape{}
	{
	}

	// 虚析构以支持通过基类指针的多态删除
	virtual ~BasePhysics() noexcept = default;

	// 位置/速度/力 的简单访问
	void set_position(const CF_V2& p) { _position = p; }
	const CF_V2& get_position() const { return _position; }
	void apply_velocity(float dt)
	{
		_position.x += _velocity.x * dt;
		_position.y += _velocity.y * dt;
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

	// 碰撞体类型访问
	void set_collider_type(ColliderType t) { collider_type = t; }
	ColliderType get_collider_type() const { return collider_type; }

	// 绑定/读取当前形状
	void set_shape(const CF_ShapeWrapper& s) { shape = s; }
	const CF_ShapeWrapper& get_shape() const { return shape; }
};