#include "base_physics.h"
#include "base_object.h"
#include "debug_config.h"
#include <algorithm>
#include <cmath>
#include <sstream> // 用于构建复杂字符串

#if COLLISION_DEBUG
#include <iomanip>
// 辅助：打印形状的 world-space 信息，用于调试碰撞细节。仅在 COLLISION_DEBUG 启用时编译。
static void dump_shape_world(const CF_ShapeWrapper& s) noexcept {
	switch (s.type) {
	case CF_SHAPE_TYPE_AABB:
		OUTPUT({"Physics"}, "    AABB min=(", s.u.aabb.min.x, ",", s.u.aabb.min.y, ")",
			" max=(", s.u.aabb.max.x, ",", s.u.aabb.max.y, ")");
		break;
	case CF_SHAPE_TYPE_CIRCLE:
		OUTPUT({"Physics"}, "    CIRCLE p=(", s.u.circle.p.x, ",", s.u.circle.p.y, ") r=", s.u.circle.r);
		break;
	case CF_SHAPE_TYPE_CAPSULE:
		OUTPUT({"Physics"}, "    CAPSULE a=(", s.u.capsule.a.x, ",", s.u.capsule.a.y, ")",
			" b=(", s.u.capsule.b.x, ",", s.u.capsule.b.y, ") r=", s.u.capsule.r);
		break;
	case CF_SHAPE_TYPE_POLY:
	{
		std::stringstream ss;
		ss << "    POLY count=" << s.u.poly.count << " verts:";
		for (int i = 0; i < s.u.poly.count; ++i) {
			ss << " (" << s.u.poly.verts[i].x << "," << s.u.poly.verts[i].y << ")";
		}
		OUTPUT({"Physics"}, ss.str());
	}
	break;
	default:
		OUTPUT({"Physics"}, "    Unknown shape");
		break;
	}
}
#endif

// 清理并规范化碰撞流中产生的 manifold，保证数值与语义安全（供后续处理使用）
// - 将 count 限定在合法范围 [0,2]
// - 修正 depths, contact_points, normal 等可能的 NaN/Inf/负值
static void normalize_and_clamp_manifold(CF_Manifold& m) noexcept
{
	// 将 count 限定在 [0,2]
	if (m.count < 0) m.count = 0;
	if (m.count > 2) m.count = 2;

	// 确保 penetration depth 非 NaN/Inf 且不为负
	for (int i = 0; i < 2; ++i) {
		if (std::isnan(m.depths[i]) || std::isinf(m.depths[i]) || m.depths[i] < 0.0f) {
			m.depths[i] = 0.0f;
		}
	}

	// 清理 contact points 中的非法值
	for (int i = 0; i < 2; ++i) {
		CF_V2& cp = m.contact_points[i];
		if (std::isnan(cp.x) || std::isnan(cp.y) || std::isinf(cp.x) || std::isinf(cp.y)) {
			cp = v2math::zero();
		}
	}

	// 归一化法线向量，若不可用则置零
	m.n = v2math::normalized(m.n);
}

// 将局部空间的 shape 平移至 world-space（不做旋转），用于 narrowphase 前的预处理
// - delta 为 world-space 中的位置偏移（通常为 object.position）
// - 该函数不会修改输入 shape，而是返回一个新的 CF_ShapeWrapper（值拷贝）
static CF_ShapeWrapper translate_shape_world(const CF_ShapeWrapper& s, const CF_V2& delta) noexcept
{
	CF_ShapeWrapper out = s;
	switch (s.type) {
	case CF_SHAPE_TYPE_AABB:
		out.u.aabb.min = s.u.aabb.min + delta;
		out.u.aabb.max = s.u.aabb.max + delta;
		break;
	case CF_SHAPE_TYPE_CIRCLE:
		out.u.circle.p = s.u.circle.p + delta;
		break;
	case CF_SHAPE_TYPE_CAPSULE:
		out.u.capsule.a = s.u.capsule.a + delta;
		out.u.capsule.b = s.u.capsule.b + delta;
		break;
	case CF_SHAPE_TYPE_POLY:
		for (int i = 0; i < out.u.poly.count; ++i) {
			out.u.poly.verts[i] = s.u.poly.verts[i] + delta;
		}
		break;
	default:
		break;
	}
	return out;
}

// 将 shape 转换为 AABB，用于 broadphase 网格索引或快速剔除
// - 返回值为该形状在 world-space 下的轴对齐包围盒（用于格子索引）
static CF_Aabb shape_wrapper_to_aabb(const CF_ShapeWrapper& s) noexcept
{
	CF_Aabb aabb{};
	if (s.type == CF_SHAPE_TYPE_AABB) {
		aabb = s.u.aabb;
		return aabb;
	}
	else if (s.type == CF_SHAPE_TYPE_CIRCLE) {
		CF_V2 p = s.u.circle.p;
		float r = s.u.circle.r;
		aabb.min = p - cf_v2(r, r);
		aabb.max = p + cf_v2(r, r);
		return aabb;
	}
	else if (s.type == CF_SHAPE_TYPE_CAPSULE) {
		CF_V2 a = s.u.capsule.a;
		CF_V2 b = s.u.capsule.b;
		float r = s.u.capsule.r;
		float minx = std::min(a.x, b.x) - r;
		float miny = std::min(a.y, b.y) - r;
		float maxx = std::max(a.x, b.x) + r;
		float maxy = std::max(a.y, b.y) + r;
		aabb.min = cf_v2(minx, miny);
		aabb.max = cf_v2(maxx, maxy);
		return aabb;
	}
	else if (s.type == CF_SHAPE_TYPE_POLY) {
		if (s.u.poly.count <= 0) {
			aabb.min = v2math::zero();
			aabb.max = v2math::zero();
			return aabb;
		}
		float minx = s.u.poly.verts[0].x;
		float miny = s.u.poly.verts[0].y;
		float maxx = minx;
		float maxy = miny;
		for (int i = 1; i < s.u.poly.count; ++i) {
			minx = std::min(minx, s.u.poly.verts[i].x);
			miny = std::min(miny, s.u.poly.verts[i].y);
			maxx = std::max(maxx, s.u.poly.verts[i].x);
			maxy = std::max(maxy, s.u.poly.verts[i].y);
		}
		aabb.min = cf_v2(minx, miny);
		aabb.max = cf_v2(maxx, maxy);
		return aabb;
	}
	// fallback
	aabb.min = v2math::zero();
	aabb.max = v2math::zero();
	return aabb;
}

// 使用 Cute Framework 的碰撞函数计算 world-space shape 的碰撞信息，
// 并对结果进行基础校验和归一化，返回是否发生碰撞。
// - 调用者应保证传入的 A/B 为 world-space（translate_shape_world 或 BasePhysics 已处理）
// - out_manifold 为可选输出（若非 nullptr 则写入计算结果）
static bool shapes_collide_world(const CF_ShapeWrapper& A, const CF_ShapeWrapper& B, CF_Manifold* out_manifold) noexcept
{
	CF_Manifold m{};
	cf_collide(&A.u, nullptr, A.type, &B.u, nullptr, B.type, &m);

	// 如果没有接触点则认为未碰撞
	if (m.count <= 0) return false;

	// 规范化 manifold 数据，防止数值异常
	normalize_and_clamp_manifold(m);

	if (out_manifold) *out_manifold = m;
	return true;
}

// 注意：PhysicsSystem 通过 ObjToken 管理 BasePhysics 的注册与反注册，从而在 Step() 中统一进行碰撞检测与回调。
// 以下实现关注性能与稳定性：使用格子 broadphase 降低 narrowphase 次数，合并重复 contact 以限制每对最多两个 contact。
void PhysicsSystem::Register(const ObjManager::ObjToken& token, BasePhysics* phys) noexcept
{
	if (!phys) return;
	uint64_t key = make_key(token);

	auto it = dynamic_token_map_.find(key);
	if (it != dynamic_token_map_.end()) {
		dynamic_entries_[it->second].physics = phys;
		dynamic_entries_[it->second].token = token;
		return;
	}
	Entry e;
	e.token = token;
	e.physics = phys;
	dynamic_entries_.push_back(e);
	dynamic_token_map_[key] = dynamic_entries_.size() - 1;
}

// 反注册：将条目从 entries_ 中移除并维护映射一致性
// - 将尾部条目移动到被删除位置以避免 O(n) 删除成本，同时更新 token_map_
void PhysicsSystem::Unregister(const ObjManager::ObjToken& token) noexcept
{
	uint64_t key = make_key(token);

	auto static_it = static_token_map_.find(key);
	if (static_it != static_token_map_.end()) {
		size_t idx = static_it->second;
		size_t last = static_entries_.size() - 1;
		if (idx != last) {
			static_entries_[idx] = static_entries_[last];
			uint64_t moved_key = make_key(static_entries_[idx].token);
			static_token_map_[moved_key] = idx;
		}
		static_entries_.pop_back();
		static_token_map_.erase(static_it);
	}
	else {
		auto dynamic_it = dynamic_token_map_.find(key);
		if (dynamic_it == dynamic_token_map_.end()) return;
		size_t idx = dynamic_it->second;
		size_t last = dynamic_entries_.size() - 1;
		if (idx != last) {
			dynamic_entries_[idx] = dynamic_entries_[last];
			uint64_t moved_key = make_key(dynamic_entries_[idx].token);
			dynamic_token_map_[moved_key] = idx;
		}
		dynamic_entries_.pop_back();
		dynamic_token_map_.erase(dynamic_it);
	}


    // 修复：当对象被销毁时，从碰撞对记录中移除相关条目，防止内存泄漏和性能下降
    auto clean_pairs = [&](auto& pairs_map) {
        for (auto it = pairs_map.begin(); it != pairs_map.end(); ) {
            if (make_key(it->second.first) == key || make_key(it->second.second) == key) {
                it = pairs_map.erase(it);
            } else {
                ++it;
            }
        }
    };

    clean_pairs(prev_collision_pairs_);
    clean_pairs(current_pairs_);
}

void PhysicsSystem::Step(float cell_size) noexcept
{
	events_.clear();
	if (dynamic_entries_.empty() && static_entries_.empty()) return;

	// 清空上次 frame 使用过的网格 bucket，以便复用容器
	for (uint64_t k : grid_keys_used_) {
		auto git = grid_.find(k);
		if (git != grid_.end()) {
			git->second.clear();
		}
	}
	grid_keys_used_.clear();

	// 调整 world_shapes_ 大小以容纳所有对象
	world_shapes_.resize(dynamic_entries_.size() + static_entries_.size());

	// 辅助函数：将对象更新并放入网格
	auto update_entry_in_grid = [&](Entry& entry, size_t world_shape_idx) {
		BasePhysics* p = entry.physics;
		if (!p) return;

		// 仅当对象是动态的或首次注册时才更新网格
		CF_ShapeWrapper ws;
		if (entry.dirty) {
			CF_ShapeWrapper s = p->get_shape();
			if (p->is_world_shape_enabled()) {
				ws = s;
			}
			else {
				ws = translate_shape_world(s, p->get_position());
			}
			world_shapes_[world_shape_idx] = ws;
		}
		else {
			ws = world_shapes_[world_shape_idx];
		}

		CF_Aabb waabb = shape_wrapper_to_aabb(ws);

		int32_t gx0 = static_cast<int32_t>(std::floor(waabb.min.x / cell_size));
		int32_t gy0 = static_cast<int32_t>(std::floor(waabb.min.y / cell_size));
		int32_t gx1 = static_cast<int32_t>(std::floor(waabb.max.x / cell_size));
		int32_t gy1 = static_cast<int32_t>(std::floor(waabb.max.y / cell_size));

		for (int32_t gx = gx0; gx <= gx1; ++gx) {
			for (int32_t gy = gy0; gy <= gy1; ++gy) {
				uint64_t gkey = grid_key(gx, gy);
				grid_[gkey].push_back(world_shape_idx);
				grid_keys_used_.emplace_back(gkey);
			}
		}

		CF_V2 center = (waabb.min + waabb.max) * 0.5f;
		entry.grid_x = static_cast<int32_t>(std::floor(center.x / cell_size));
		entry.grid_y = static_cast<int32_t>(std::floor(center.y / cell_size));

		p->clear_position_dirty();
	};

	// 更新动态对象
	for (size_t i = 0; i < dynamic_entries_.size(); ++i) {
		update_entry_in_grid(dynamic_entries_[i], i);
	}

	// 更新静态对象（仅在首次或需要时）
	size_t static_offset = dynamic_entries_.size();
	for (size_t i = 0; i < static_entries_.size(); ++i) {
		update_entry_in_grid(static_entries_[i], static_offset + i);
	}


	// 进行 narrowphase
	events_.reserve(dynamic_entries_.size() * 2); // 预估容量

	// 辅助函数：执行碰撞检测
	auto check_collisions_for_bucket = [&](size_t i, const std::vector<size_t>& bucket) {
		Entry& a_entry = (i < static_offset) ? dynamic_entries_[i] : static_entries_[i - static_offset];
		BasePhysics* pa = a_entry.physics;
		if (!pa || pa->get_collider_type() == ColliderType::VOID) return;

		for (size_t j_idx : bucket) {
			if (j_idx <= i) continue;

			Entry& b_entry = (j_idx < static_offset) ? dynamic_entries_[j_idx] : static_entries_[j_idx - static_offset];
			BasePhysics* pb = b_entry.physics;
			if (!pb || pb->get_collider_type() == ColliderType::VOID) continue;

			CF_Manifold m{};
			const CF_ShapeWrapper& aw = world_shapes_[i];
			const CF_ShapeWrapper& bw = world_shapes_[j_idx];
			if (shapes_collide_world(aw, bw, &m)) {
				CollisionEvent ev;
				ev.a = a_entry.token;
				ev.b = b_entry.token;
				ev.manifold = m;
				CF_V2 delta = pa->get_position() - pb->get_position();
				ev.distance_sq = v2math::dot(delta, delta);
				events_.push_back(ev);
			}
		}
	};

	for (size_t i = 0; i < dynamic_entries_.size(); ++i) {
		Entry& a = dynamic_entries_[i];
		for (int dx = -1; dx <= 1; ++dx) {
			for (int dy = -1; dy <= 1; ++dy) {
				uint64_t nkey = grid_key(a.grid_x + dx, a.grid_y + dy);
				auto nit = grid_.find(nkey);
				if (nit != grid_.end()) {
					check_collisions_for_bucket(i, nit->second);
				}
			}
		}
	}

	if (events_.size() > 1) {
		std::sort(events_.begin(), events_.end(), [](const CollisionEvent& lhs, const CollisionEvent& rhs) {
			return lhs.distance_sq < rhs.distance_sq;
		});
	}

	// 使用事件产生 Enter/Stay/Exit 回调序列
	current_pairs_.clear();
	current_pairs_.reserve(events_.size() * 2);

	auto make_pair_key_and_ordered_tokens = [this](const ObjManager::ObjToken& t1, const ObjManager::ObjToken& t2,
		uint64_t& out_key, ObjManager::ObjToken& out_first, ObjManager::ObjToken& out_second) noexcept
		{
			uint64_t k1 = make_key(t1);
			uint64_t k2 = make_key(t2);
			if (k1 <= k2) {
				out_first = t1;
				out_second = t2;
				out_key = k1 ^ (k2 + 0x9e3779b97f4a7c15ULL + (k1 << 6) + (k1 >> 2));
			}
			else {
				out_first = t2;
				out_second = t1;
				out_key = k2 ^ (k1 + 0x9e3779b97f4a7c15ULL + (k2 << 6) + (k2 >> 2));
			}
		};

	for (const auto& ev : events_) {
		if (!ObjManager::Instance().IsValid(ev.a) || !ObjManager::Instance().IsValid(ev.b)) continue;

		uint64_t pair_key;
		ObjManager::ObjToken first_tok;
		ObjManager::ObjToken second_tok;
		make_pair_key_and_ordered_tokens(ev.a, ev.b, pair_key, first_tok, second_tok);

		current_pairs_.emplace(pair_key, std::make_pair(first_tok, second_tok));

		auto prev_it = prev_collision_pairs_.find(pair_key);
		const bool was_colliding = (prev_it != prev_collision_pairs_.end());

		// 使用 token-based 的 operator[] 获取对象引用（在前面已通过 IsValid 校验，operator[] 不应抛出）
		BaseObject& oa = ObjManager::Instance()[ev.a];
		BaseObject& ob = ObjManager::Instance()[ev.b];

		auto orient_manifold = [](const CF_Manifold& src, const BaseObject& self, const BaseObject& other) {
			CF_Manifold out = src;
			CF_V2 dir = other.GetPosition() - self.GetPosition();
			if (v2math::length(dir) > 1e-6f) {
				CF_V2 dir_norm = v2math::normalized(dir);
				if (v2math::dot(out.n, dir_norm) < 0.0f) {
					out.n = -out.n;
				}
			}
			return out;
			};

		CF_Manifold manifold_for_a = orient_manifold(ev.manifold, oa, ob);
		CF_Manifold manifold_for_b = orient_manifold(ev.manifold, ob, oa);
			if (was_colliding) {
				oa.OnCollisionState(ev.b, manifold_for_a, BaseObject::CollisionPhase::Stay);
			}
			else {
				oa.OnCollisionState(ev.b, manifold_for_a, BaseObject::CollisionPhase::Enter);
			}
			if (was_colliding) {
				ob.OnCollisionState(ev.a, manifold_for_b, BaseObject::CollisionPhase::Stay);
			}
			else {
				ob.OnCollisionState(ev.a, manifold_for_b, BaseObject::CollisionPhase::Enter);
			}
	}

	// 对上帧存在但本帧消失的对触发 Exit 回调
	for (const auto& prev_pair : prev_collision_pairs_) {
		uint64_t key = prev_pair.first;
		if (current_pairs_.find(key) == current_pairs_.end()) {
			const auto& tok_pair = prev_pair.second;
			const ObjManager::ObjToken& ta = tok_pair.first;
			const ObjManager::ObjToken& tb = tok_pair.second;

			if (!ObjManager::Instance().IsValid(ta) || !ObjManager::Instance().IsValid(tb)) continue;

			// 使用 operator[] 获取引用（已校验）
			BaseObject& oa = ObjManager::Instance()[ta];
			BaseObject& ob = ObjManager::Instance()[tb];

#if COLLISION_DEBUG
			// Exit 只打印简短摘要
			OUTPUT({"Physics"}, "Collision EXIT: a=", ta.index, " b=", tb.index);
#endif

			oa.OnCollisionState(tb, CF_Manifold{}, BaseObject::CollisionPhase::Exit);
			ob.OnCollisionState(ta, CF_Manifold{}, BaseObject::CollisionPhase::Exit);
		}
	}
	prev_collision_pairs_.swap(current_pairs_);
}