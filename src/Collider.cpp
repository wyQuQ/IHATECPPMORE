#include "base_physics.h"
#include "base_object.h"
#include "debug_config.h"

#if COLLISION_DEBUG
#include <iomanip>
#include <cmath>
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

// 防御性：规范化 manifold.n 并修正 depth 与 contact_points（确保数值稳定）
static void normalize_and_clamp_manifold(CF_Manifold& m) noexcept
{
	// 限制 count 到合法范围 [0,2]
	if (m.count < 0) m.count = 0;
	if (m.count > 2) m.count = 2;

	// 修正 depth（非负且非 NaN/Inf）
	for (int i = 0; i < 2; ++i) {
		if (std::isnan(m.depths[i]) || std::isinf(m.depths[i]) || m.depths[i] < 0.0f) {
			m.depths[i] = 0.0f;
		}
	}

	// 清理非法接触点坐标
	for (int i = 0; i < 2; ++i) {
		CF_V2& cp = m.contact_points[i];
		if (std::isnan(cp.x) || std::isnan(cp.y) || std::isinf(cp.x) || std::isinf(cp.y)) {
			cp = cf_v2(0.0f, 0.0f);
		}
	}

	// 规范化法线；若长度过小则置零向量
	float len = v2math::length(m.n);
	if (std::isnan(len) || std::isinf(len) || len <= 1e-6f) {
		m.n = cf_v2(0.0f, 0.0f);
	}
	else {
		m.n.x /= len;
		m.n.y /= len;
	}
}

// 将局部 shape 平移 delta，返回新副本（用于在 narrowphase 前得到 world-space shape）
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

// 将 world-space shape 转为一个覆盖它的 AABB（用于 broadphase 网格映射）
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
		aabb.min = cf_v2(p.x - r, p.y - r);
		aabb.max = cf_v2(p.x + r, p.y + r);
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
			aabb.min = cf_v2(0.0f, 0.0f);
			aabb.max = cf_v2(0.0f, 0.0f);
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
	aabb.min = cf_v2(0.0f, 0.0f);
	aabb.max = cf_v2(0.0f, 0.0f);
	return aabb;
}

static bool shapes_collide_world(const CF_ShapeWrapper& A, const CF_ShapeWrapper& B, CF_Manifold* out_manifold) noexcept
{
	CF_Manifold m{};
	cf_collide(&A.u, nullptr, A.type, &B.u, nullptr, B.type, &m);

	// 仅当 manifold 有有效接触点时认为发生碰撞
	if (m.count <= 0) return false;

	// 规范化并修正深度
	// 保证深度非负
	for (int i = 0; i < 2; ++i) {
		if (m.depths[i] < 0.0f) m.depths[i] = 0.0f;
	}
	// 规范化法线
	float len = v2math::length(m.n);
	if (len > 1e-6f) {
		m.n.x /= len;
		m.n.y /= len;
	}
	else {
		m.n.x = 0.0f; m.n.y = 0.0f;
	}

	if (out_manifold) *out_manifold = m;
	return true;
}

// 注册：将物体及其 BasePhysics 指针加入检测集合（Create 后调用）
void PhysicsSystem::Register(const ObjManager::ObjToken& token, BasePhysics* phys) noexcept
{
	if (!phys) return;
	uint64_t key = make_key(token);
	auto it = token_map_.find(key);
	if (it != token_map_.end()) {
		// 已存在：更新指针（通常不会发生）
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

// 注销：在对象销毁前调用（Destroy 前调用，以避免回调访问已销毁对象）
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

	// 构建空间哈希（格子映射）
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

		CF_ShapeWrapper ws = translate_shape_world(p->get_shape(), p->get_position());
		world_shapes_[i] = ws;
		CF_Aabb waabb = shape_wrapper_to_aabb(ws);

		int32_t gx0 = static_cast<int32_t>(std::floor(waabb.min.x / cell_size));
		int32_t gy0 = static_cast<int32_t>(std::floor(waabb.min.y / cell_size));
		int32_t gx1 = static_cast<int32_t>(std::floor(waabb.max.x / cell_size));
		int32_t gy1 = static_cast<int32_t>(std::floor(waabb.max.y / cell_size));

		// 遍历覆盖格子并插入索引
		for (int32_t gx = gx0; gx <= gx1; ++gx) {
			for (int32_t gy = gy0; gy <= gy1; ++gy) {
				uint64_t gkey = grid_key(gx, gy);
				auto it = grid_.find(gkey);
				if (it == grid_.end()) {
					// 新桶：创建并预留
					auto res = grid_.emplace(gkey, std::vector<size_t>());
					it = res.first;
					it->second.reserve(4);
					// 记录被使用的键
					grid_keys_used_.push_back(gkey);
				}
				else {
					// 确保仅记录一次
					if (it->second.empty()) grid_keys_used_.push_back(gkey);
				}
				it->second.push_back(i);
			}
		}

		// cache grid coord for neighbor sweep 使用 aabb 中心作为参考格子
		float cx = (waabb.min.x + waabb.max.x) * 0.5f;
		float cy = (waabb.min.y + waabb.max.y) * 0.5f;
		entries_[i].grid_x = static_cast<int32_t>(std::floor(cx / cell_size));
		entries_[i].grid_y = static_cast<int32_t>(std::floor(cy / cell_size));
	}

	// 对每个对象，只与自身格子及邻格的对象比较（避免重复对比）
	// 预留 events_ 避免重复分配
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

					// 过滤 VOID
					if (pa->get_collider_type() == ColliderType::VOID || pb->get_collider_type() == ColliderType::VOID)
						continue;

					// --- narrowphase: 使用缓存的 world_shapes_ 避免重复 translate 与拷贝 ---
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

	// 合并同一对对象的 manifold：保留最多 2 个不同 contact points
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

			// 合并 manifold: 将 ev.manifold 的 contact 点合并到已有 manifold（最多 2 个不同点）
			CF_Manifold& dst = it->second.manifold;
			const CF_Manifold& src = ev.manifold;
			const float eps_sq = 1e-4f; // 两点被视为相同的距离阈值平方

			// 遍历源 contact 点
			for (int si = 0; si < src.count && dst.count < 2; ++si) {
				const CF_V2& sp = src.contact_points[si];
				bool found = false;
				for (int di = 0; di < dst.count; ++di) {
					const CF_V2& dp = dst.contact_points[di];
					float dx = sp.x - dp.x;
					float dy = sp.y - dp.y;
					if (dx * dx + dy * dy <= eps_sq) {
						// 相同 contact -> 保留更大 penetration depth
						if (src.depths[si] > dst.depths[di]) dst.depths[di] = src.depths[si];
						found = true;
						break;
					}
				}
				if (!found && dst.count < 2) {
					// 将新的 contact 放到第一个空槽
					dst.contact_points[dst.count] = sp;
					dst.depths[dst.count] = src.depths[si];
					dst.count++;
				}
			}

			// 合并法线：以 penetration depth 加权（简单合并）
			if (v2math::length(dst.n) > 1e-6f || v2math::length(src.n) > 1e-6f) {
				dst.n.x = dst.n.x * 0.5f + src.n.x * 0.5f;
				dst.n.y = dst.n.y * 0.5f + src.n.y * 0.5f;
			}

			// 确保数值稳定
			normalize_and_clamp_manifold(dst);
		}

		// 将合并后的结果按原始首次出现顺序重建 events_
		std::vector<CollisionEvent> merged_events;
		merged_events.reserve(merged_map_.size());
		for (uint64_t key : merged_order_) {
			merged_events.push_back(merged_map_[key]);
		}
		events_.swap(merged_events);
	}

	// 统一派发：使用上一帧保存的数据区分 Enter / Stay / Exit
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

		BaseObject* oa = ObjManager::Instance().Get(ev.a);
		BaseObject* ob = ObjManager::Instance().Get(ev.b);

#if COLLISION_DEBUG
		static int enterCount = 0;
		if (!was_colliding) {
			std::cerr << "[Physics] Collision Event #" << (++enterCount) << ": a=" << ev.a.index << " b=" << ev.b.index
				<< (was_colliding ? " (STAY)" : " (ENTER)") << "\n";

			std::cerr << "[Physics] Enter collision (detailed): a=" << ev.a.index << " b=" << ev.b.index << "\n";

			if (oa && ob) {
				const CF_ShapeWrapper& aworld = world_shapes_[ev.a.index];
				const CF_ShapeWrapper& bworld = world_shapes_[ev.b.index];

				std::cerr << "  A (world):\n";
				dump_shape_world(aworld);
				std::cerr << "  B (world):\n";
				dump_shape_world(bworld);
			}
			else {
				std::cerr << "  Warning: one of objects is null when printing detailed shapes\n";
			}

			std::cerr << "  manifold.count=" << ev.manifold.count
				<< " normal=(" << ev.manifold.n.x << "," << ev.manifold.n.y << ")\n";
			for (int pi = 0; pi < std::min(ev.manifold.count, 2); ++pi) {
				std::cerr << std::fixed << std::setprecision(6)
					<< "    contact[" << pi << "] = (" << ev.manifold.contact_points[pi].x << ", "
					<< ev.manifold.contact_points[pi].y << ") depth=" << ev.manifold.depths[pi] << "\n";
			}
			std::cout << "-----\n";
		}
#endif

		if (oa) {
			if (was_colliding) {
				oa->OnCollisionState(ev.b, ev.manifold, BaseObject::CollisionPhase::Stay);
			}
			else {
				oa->OnCollisionState(ev.b, ev.manifold, BaseObject::CollisionPhase::Enter);
			}
		}
		if (ob) {
			if (was_colliding) {
				ob->OnCollisionState(ev.a, ev.manifold, BaseObject::CollisionPhase::Stay);
			}
			else {
				ob->OnCollisionState(ev.a, ev.manifold, BaseObject::CollisionPhase::Enter);
			}
		}
	}

	for (const auto& prev_pair : prev_collision_pairs_) {
		uint64_t key = prev_pair.first;
		if (current_pairs_.find(key) == current_pairs_.end()) {
			const auto& tok_pair = prev_pair.second;
			const ObjManager::ObjToken& ta = tok_pair.first;
			const ObjManager::ObjToken& tb = tok_pair.second;

			if (!ObjManager::Instance().IsValid(ta) || !ObjManager::Instance().IsValid(tb)) continue;

			BaseObject* oa = ObjManager::Instance().Get(ta);
			BaseObject* ob = ObjManager::Instance().Get(tb);

			if (oa) {
				oa->OnCollisionState(tb, CF_Manifold{}, BaseObject::CollisionPhase::Exit);
			}
			if (ob) {
				ob->OnCollisionState(ta, CF_Manifold{}, BaseObject::CollisionPhase::Exit);
			}
		}
	}
	prev_collision_pairs_.swap(current_pairs_);
}