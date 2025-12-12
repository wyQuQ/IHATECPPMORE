#include "base_physics.h"
#include "base_object.h"
#include "debug_config.h"

#if COLLISION_DEBUG
#include <iomanip>
#include <cmath>
// 辅助：打印形状的 world-space 信息，用于调试碰撞细节。仅在 COLLISION_DEBUG 启用时编译。
static void dump_shape_world(const CF_ShapeWrapper& s) noexcept {
	switch (s.type) {
	case CF_SHAPE_TYPE_AABB:
		std::cerr << "    AABB min=(" << s.u.aabb.min.x << "," << s.u.aabb.min.y << ")"
			<< " max=(" << s.u.aabb.max.x << "," << s.u.aabb.max.y << ")\n";
		break;
	case CF_SHAPE_TYPE_CIRCLE:
		std::cerr << "    CIRCLE p=(" << s.u.circle.p.x << "," << s.u.circle.p.y << ") r=" << s.u.circle.r << "\n";
		break;
	case CF_SHAPE_TYPE_CAPSULE:
		std::cerr << "    CAPSULE a=(" << s.u.capsule.a.x << "," << s.u.capsule.a.y << ")"
			<< " b=(" << s.u.capsule.b.x << "," << s.u.capsule.b.y << ") r=" << s.u.capsule.r << "\n";
		break;
	case CF_SHAPE_TYPE_POLY:
		std::cerr << "    POLY count=" << s.u.poly.count << " verts:";
		for (int i = 0; i < s.u.poly.count; ++i) {
			std::cerr << " (" << s.u.poly.verts[i].x << "," << s.u.poly.verts[i].y << ")";
		}
		std::cerr << "\n";
		break;
	default:
		std::cerr << "    Unknown shape\n";
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
	for (int i = 0; i < 2; ++i) {
		if (m.depths[i] < 0.0f) m.depths[i] = 0.0f;
	}
	m.n = v2math::normalized(m.n);

	if (out_manifold) *out_manifold = m;
	return true;
}

// 注意：PhysicsSystem 通过 ObjToken 管理 BasePhysics 的注册与反注册，从而在 Step() 中统一进行碰撞检测与回调。
// 以下实现关注性能与稳定性：使用格子 broadphase 降低 narrowphase 次数，合并重复 contact 以限制每对最多两个 contact。
void PhysicsSystem::Register(const ObjManager::ObjToken& token, BasePhysics* phys) noexcept
{
	if (!phys) return;
	uint64_t key = make_key(token);
	auto it = token_map_.find(key);
	if (it != token_map_.end()) {
		// 如果已存在条目，更新指针与 token
		entries_[it->second].physics = phys;
		entries_[it->second].token = token;
		return;
	}
	Entry e;
	e.token = token;
	e.physics = phys;
	entries_.push_back(e);
	token_map_[key] = entries_.size() - 1;
}

// 反注册：将条目从 entries_ 中移除并维护映射一致性
// - 将尾部条目移动到被删除位置以避免 O(n) 删除成本，同时更新 token_map_
void PhysicsSystem::Unregister(const ObjManager::ObjToken& token) noexcept
{
	uint64_t key = make_key(token);
	auto it = token_map_.find(key);
	if (it == token_map_.end()) return;
	size_t idx = it->second;
	size_t last = entries_.size() - 1;
	if (idx != last) {
		entries_[idx] = entries_[last];
		uint64_t moved_key = make_key(entries_[idx].token);
		token_map_[moved_key] = idx;
	}
	entries_.pop_back();
	token_map_.erase(it);
}

void PhysicsSystem::Step(float cell_size) noexcept
{
	events_.clear();
	if (entries_.empty()) return;

	// 清空上次 frame 使用过的网格 bucket，以便复用容器
	for (uint64_t k : grid_keys_used_) {
		auto git = grid_.find(k);
		if (git != grid_.end()) {
			git->second.clear();
		}
	}
	grid_keys_used_.clear();

	world_shapes_.resize(entries_.size());

	for (size_t i = 0; i < entries_.size(); ++i) {
		BasePhysics* p = entries_[i].physics;
		if (!p) continue;

		// 获取 shape 并根据配置决定是否需要将其视为已经是 world-space（use_world_shape_）
		CF_ShapeWrapper s = p->get_shape();
		CF_ShapeWrapper ws;
		if (p->is_world_shape_enabled()) {
			// 物理组件已经提供 world-space 的 shape
			ws = s;
		}
		else {
			// 将 local shape 平移到 world-space（旋转在 BasePhysics::tweak_shape_with_rotation 中处理）
			ws = translate_shape_world(s, p->get_position());
		}

		world_shapes_[i] = ws;
		CF_Aabb waabb = shape_wrapper_to_aabb(ws);

		int32_t gx0 = static_cast<int32_t>(std::floor(waabb.min.x / cell_size));
		int32_t gy0 = static_cast<int32_t>(std::floor(waabb.min.y / cell_size));
		int32_t gx1 = static_cast<int32_t>(std::floor(waabb.max.x / cell_size));
		int32_t gy1 = static_cast<int32_t>(std::floor(waabb.max.y / cell_size));

		// 将条目放入覆盖到的网格桶中，用于后续的邻域检测
		for (int32_t gx = gx0; gx <= gx1; ++gx) {
			for (int32_t gy = gy0; gy <= gy1; ++gy) {
				uint64_t gkey = grid_key(gx, gy);
				auto it = grid_.find(gkey);
				if (it == grid_.end()) {
					auto res = grid_.emplace(gkey, std::vector<size_t>());
					it = res.first;
					it->second.reserve(4);
					grid_keys_used_.push_back(gkey);
				}
				else {
					if (it->second.empty()) grid_keys_used_.push_back(gkey);
				}
				it->second.push_back(i);
			}
		}

		// 缓存该条目的网格坐标（用于邻域遍历）
		CF_V2 center = (waabb.min + waabb.max) * 0.5f;
		entries_[i].grid_x = static_cast<int32_t>(std::floor(center.x / cell_size));
		entries_[i].grid_y = static_cast<int32_t>(std::floor(center.y / cell_size));
	}

	// 进行 narrowphase：对每个条目检查邻域桶内可能的碰撞对并产生事件
	events_.reserve(entries_.size());

	for (size_t i = 0; i < entries_.size(); ++i) {
		Entry& a = entries_[i];
		BasePhysics* pa = a.physics;
		if (!pa) continue;

		for (int dx = -1; dx <= 1; ++dx) {
			for (int dy = -1; dy <= 1; ++dy) {
				uint64_t nkey = grid_key(a.grid_x + dx, a.grid_y + dy);
				auto nit = grid_.find(nkey);
				if (nit == grid_.end()) continue;
				const auto& bucket = nit->second;
				for (size_t j : bucket) {
					if (j <= i) continue;
					Entry& b = entries_[j];
					BasePhysics* pb = b.physics;
					if (!pb) continue;

					// 忽略 VOID 类型的碰撞器
					if (pa->get_collider_type() == ColliderType::VOID || pb->get_collider_type() == ColliderType::VOID)
						continue;

					// 使用 world_shapes_ 进行 narrowphase 碰撞检测
					CF_Manifold m{};
					const CF_ShapeWrapper& aw = world_shapes_[i];
					const CF_ShapeWrapper& bw = world_shapes_[j];
					bool collided = shapes_collide_world(aw, bw, &m);

					if (collided) {
						CollisionEvent ev;
						ev.a = a.token;
						ev.b = b.token;
						ev.manifold = m;
						events_.push_back(ev);
					}
				}
			}
		}
	}

	// 合并同一对的多个 contact，使每对最终最多包含两个接触点，便于上层处理
	// 优先保留法向量方向与物体速度方向更一致的碰撞信息
	if (!events_.empty()) {
		merged_map_.clear();
		merged_order_.clear();
		merged_map_.reserve(events_.size() * 2);
		merged_order_.reserve(events_.size());

		for (const auto& ev : events_) {
			uint64_t k1 = make_key(ev.a);
			uint64_t k2 = make_key(ev.b);
			uint64_t pair_key = (k1 <= k2)
				? (k1 ^ (k2 + 0x9e3779b97f4a7c15ULL + (k1 << 6) + (k1 >> 2)))
				: (k2 ^ (k1 + 0x9e3779b97f4a7c15ULL + (k2 << 6) + (k2 >> 2)));

			auto it = merged_map_.find(pair_key);
			if (it == merged_map_.end()) {
				merged_map_.emplace(pair_key, ev);
				merged_order_.push_back(pair_key);
				continue;
			}

			// 获取物体 a 的速度用于判断法向量优先级
			BasePhysics* pa = nullptr;
			for (const auto& entry : entries_) {
				if (make_key(entry.token) == k1) {
					pa = entry.physics;
					break;
				}
			}

			// 计算法向量与速度方向的对齐度（点积）
			// 更大的点积表示法向量更接近速度方向，优先保留这种碰撞
			float current_alignment = 0.0f;
			float new_alignment = 0.0f;

			if (pa) {
				CF_V2 vel = pa->get_velocity();
				float vel_len = v2math::length(vel);

				if (vel_len > 1e-3f) { // 仅当物体有明显速度时才考虑方向
					CF_V2 vel_norm = v2math::normalized(vel);

					current_alignment = v2math::dot(it->second.manifold.n, vel_norm);
					new_alignment = v2math::dot(ev.manifold.n, vel_norm);

					// 如果新碰撞的法向量更接近速度方向，则完全替换
					if (new_alignment > current_alignment + 0.1f) { // 0.1 为阈值，避免频繁抖动
						it->second = ev;
						continue;
					}
				}
			}

			// 合并 manifold：将接触点与穿透深度合并至最多两个 contact
			CF_Manifold& dst = it->second.manifold;
			const CF_Manifold& src = ev.manifold;
			const float eps_sq = 1e-4f;

			for (int si = 0; si < src.count && dst.count < 2; ++si) {
				const CF_V2& sp = src.contact_points[si];
				bool found = false;
				for (int di = 0; di < dst.count; ++di) {
					const CF_V2& dp = dst.contact_points[di];
					CF_V2 diff = sp - dp;
					if (v2math::dot(diff, diff) <= eps_sq) {
						if (src.depths[si] > dst.depths[di]) dst.depths[di] = src.depths[si];
						found = true;
						break;
					}
				}
				if (!found && dst.count < 2) {
					dst.contact_points[dst.count] = sp;
					dst.depths[dst.count] = src.depths[si];
					dst.count++;
				}
			}

			// 法线方向合并：根据速度对齐度加权
			if (v2math::length(dst.n) > 1e-6f || v2math::length(src.n) > 1e-6f) {
				// 如果有明显的对齐度差异，按对齐度加权；否则简单平均
				if (std::abs(new_alignment - current_alignment) > 0.05f) {
					float total = std::abs(current_alignment) + std::abs(new_alignment) + 1e-6f;
					float w_dst = std::abs(current_alignment) / total;
					float w_src = std::abs(new_alignment) / total;
					dst.n = dst.n * w_dst + src.n * w_src;
				}
				else {
					dst.n = (dst.n + src.n) * 0.5f;
				}
			}

			// 规范化合并结果
			normalize_and_clamp_manifold(dst);
		}

		// 将合并结果替换 events_
		std::vector<CollisionEvent> merged_events;
		merged_events.reserve(merged_map_.size());
		for (uint64_t key : merged_order_) {
			merged_events.push_back(merged_map_[key]);
		}
		events_.swap(merged_events);
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

#if COLLISION_DEBUG
		// 调试绘制碰撞信息：紫色线连接接触点，紫色箭头表示法向量
		cf_draw_push();
		cf_draw_push_color(CF_Color(1.0f, 0.5f, 1.0f, 1.0f)); // 粉紫色

		// 绘制接触点之间的连线（如果有两个接触点）
		if (ev.manifold.count == 2) {
			CF_V2 cp0 = ev.manifold.contact_points[0];
			cf_draw_circle2(cp0, 2.0f, 0.0f);
			CF_V2 cp1 = ev.manifold.contact_points[1];
			cf_draw_circle2(cp1, 2.0f, 0.0f);
			cf_draw_line(cp0, cp1, 0.0f);
		}

		// 从每个接触点绘制法向量（短线表示方向）
		const float normal_length = 10.0f; // 法向量的可视长度
		for (int i = 0; i < ev.manifold.count; ++i) {
			CF_V2 cp = ev.manifold.contact_points[i];
			CF_V2 normal_end = cp + ev.manifold.n * normal_length;
			cf_draw_line(cp, normal_end, 0.0f);

			// 在法向量终点绘制一个小圆点，使方向更明显
			cf_draw_circle2(normal_end, 2.0f, 0.0f);
		}

		cf_draw_pop_color();
		cf_draw_pop();
#endif

		// 直接以引用调用回调（顺序不保证：a 的回调可能先于 b，也可能相反）
		if (was_colliding) {
			oa.OnCollisionState(ev.b, ev.manifold, BaseObject::CollisionPhase::Stay);
		}
		else {
			oa.OnCollisionState(ev.b, ev.manifold, BaseObject::CollisionPhase::Enter);
		}

		if (was_colliding) {
			ob.OnCollisionState(ev.a, ev.manifold, BaseObject::CollisionPhase::Stay);
		}
		else {
			ob.OnCollisionState(ev.a, ev.manifold, BaseObject::CollisionPhase::Enter);
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
			std::cerr << "[Physics] Collision EXIT: a=" << ta.index << " b=" << tb.index << "\n";
#endif

			oa.OnCollisionState(tb, CF_Manifold{}, BaseObject::CollisionPhase::Exit);
			ob.OnCollisionState(ta, CF_Manifold{}, BaseObject::CollisionPhase::Exit);
		}
	}
	prev_collision_pairs_.swap(current_pairs_);
}