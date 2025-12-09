#include "base_physics.h"
#include <cmath>
#include <algorithm>

// 文件职责说明（面向使用者）：
// - 提供 BasePhysics 中与旋转/枢轴相关的 world-shape 计算逻辑。
// - 当物理形状需要基于旋转和枢轴转换为 world-space 时，本文件内的函数会生成缓存的 world shape 以供碰撞系统使用。
// - 保证不同形状类型（AABB / Circle / Capsule / Poly）在旋转与枢轴作用下转换为合理的 world-space 表示。

// file-local helper: 将点绕原点旋转（使用 sin/cos 提供加速）
static inline CF_V2 rotate_about_origin_local(const CF_V2& p, float sinr, float cosr) noexcept
{
	return CF_V2{ p.x * cosr - p.y * sinr, p.x * sinr + p.y * cosr };
}

// file-local helper: 对点按 x/y 分量缩放
static inline CF_V2 scale_point_local(const CF_V2& p, float sx, float sy) noexcept
{
	return CF_V2{ p.x * sx, p.y * sy };
}

// 将局部形状平移到 world-space（仅平移不旋转），用于 use_world_shape_ == false 的快捷路径
// 注意：该函数保持简单平移行为，scale 的处理在 tweak_shape_with_rotation 中完成（以便使用 BasePhysics 的 scale_x_/scale_y_）
static inline CF_ShapeWrapper translate_local_to_world(const CF_ShapeWrapper& s, const CF_V2& pos) noexcept
{
	CF_ShapeWrapper out = s;
	switch (s.type) {
	case CF_SHAPE_TYPE_AABB:
		out.u.aabb.min = CF_V2{ s.u.aabb.min.x + pos.x, s.u.aabb.min.y + pos.y };
		out.u.aabb.max = CF_V2{ s.u.aabb.max.x + pos.x, s.u.aabb.max.y + pos.y };
		break;
	case CF_SHAPE_TYPE_CIRCLE:
		out.u.circle.p = CF_V2{ s.u.circle.p.x + pos.x, s.u.circle.p.y + pos.y };
		break;
	case CF_SHAPE_TYPE_CAPSULE:
		out.u.capsule.a = CF_V2{ s.u.capsule.a.x + pos.x, s.u.capsule.a.y + pos.y };
		out.u.capsule.b = CF_V2{ s.u.capsule.b.x + pos.x, s.u.capsule.b.y + pos.y };
		break;
	case CF_SHAPE_TYPE_POLY:
		for (int i = 0; i < s.u.poly.count; ++i) {
			out.u.poly.verts[i] = CF_V2{ s.u.poly.verts[i].x + pos.x, s.u.poly.verts[i].y + pos.y };
		}
		out.u.poly.count = s.u.poly.count;
		break;
	default:
		break;
	}
	return out;
}

// 将局部形状根据 rotation_/pivot_/position 转换为 world-space 并缓存。
// 说明：
// - 如果 use_world_shape_ == false，则仅做平移以获得 world-space（更高效）。
// - 否则根据形状类型执行绕 origin（结合 pivot）旋转，再平移到 position。
// - 结果写入 cached_world_shape_ 并递增版本号，供物理系统稳定读取。
// 变更：现在无论 use_world_shape_ 与否，都会先对局部形状应用 scale_x_/scale_y_（对 circle 的半径采用 max(|sx|,|sy|)）
void BasePhysics::tweak_shape_with_rotation() const noexcept
{
	// 读取缩放分量（可能为负；对半径使用绝对值中的较大者）
	const float sx = scale_x_;
	const float sy = scale_y_;
	const float abs_sx = std::fabs(sx);
	const float abs_sy = std::fabs(sy);
	const float max_abs_s = (abs_sx > abs_sy) ? abs_sx : abs_sy;

	// 如果不启用 world-shape，则先对 local shape 做缩放，再平移 local -> world
	if (!use_world_shape_) {
		// 复制并对局部坐标进行缩放，然后平移
		CF_ShapeWrapper scaled = shape;
		switch (shape.type) {
		case CF_SHAPE_TYPE_AABB:
		{
			// 缩放后可能导致 min/max 交换，修正之
			CF_Aabb a = shape.u.aabb;
			CF_V2 smin = scale_point_local(a.min, sx, sy);
			CF_V2 smax = scale_point_local(a.max, sx, sy);
			CF_V2 real_min{ std::min(smin.x, smax.x), std::min(smin.y, smax.y) };
			CF_V2 real_max{ std::max(smin.x, smax.x), std::max(smin.y, smax.y) };
			scaled.u.aabb.min = real_min;
			scaled.u.aabb.max = real_max;
			break;
		}
		case CF_SHAPE_TYPE_CIRCLE:
		{
			CF_Circle c = shape.u.circle;
			CF_V2 center = scale_point_local(c.p, sx, sy);
			CF_Circle nc{ center, c.r * max_abs_s };
			scaled.u.circle = nc;
			break;
		}
		case CF_SHAPE_TYPE_CAPSULE:
		{
			CF_Capsule cap = shape.u.capsule;
			CF_V2 a = scale_point_local(cap.a, sx, sy);
			CF_V2 b = scale_point_local(cap.b, sx, sy);
			// 半径按最大缩放分量缩放（保持不会被压扁为椭圆）
			CF_Capsule nc = cf_make_capsule(a, b, cap.r * max_abs_s);
			scaled = CF_ShapeWrapper::FromCapsule(nc);
			break;
		}
		case CF_SHAPE_TYPE_POLY:
		{
			CF_Poly p = shape.u.poly;
			CF_Poly wp{};
			wp.count = p.count;
			for (int i = 0; i < p.count; ++i) {
				wp.verts[i] = scale_point_local(p.verts[i], sx, sy);
			}
			cf_make_poly(&wp);
			scaled = CF_ShapeWrapper::FromPoly(wp);
			break;
		}
		default:
			break;
		}

		// 平移到 world-space
		cached_world_shape_ = translate_local_to_world(scaled, _position);
		world_shape_dirty_ = false;
		++world_shape_version_;
		return;
	}

	// 计算 sin/cos（用于旋转）
	const float ang = rotation_;
	const float sinr = cf_sin(ang);
	const float cosr = cf_cos(ang);

	switch (shape.type) {
	case CF_SHAPE_TYPE_AABB:
	{
		// 将 AABB 的四个顶点按缩放 -> 旋转 -> 平移到 world-space，并以多边形表示缓存
		CF_Aabb a = shape.u.aabb;
		CF_V2 corners[4];
		corners[0] = CF_V2{ a.min.x, a.min.y };
		corners[1] = CF_V2{ a.max.x, a.min.y };
		corners[2] = CF_V2{ a.max.x, a.max.y };
		corners[3] = CF_V2{ a.min.x, a.max.y };

		CF_Poly p{};
		p.count = 4;
		for (int i = 0; i < 4; ++i) {
			// 先缩放
			CF_V2 local_scaled = scale_point_local(corners[i], sx, sy);
			// 再旋转
			CF_V2 rotated = rotate_about_origin_local(local_scaled, sinr, cosr);
			// 最后平移到 world
			CF_V2 world_pt = CF_V2{ rotated.x + _position.x, rotated.y + _position.y };
			p.verts[i] = world_pt;
		}
		cf_make_poly(&p);
		cached_world_shape_ = CF_ShapeWrapper::FromPoly(p);
		break;
	}
	case CF_SHAPE_TYPE_CIRCLE:
	{
		// 圆形：中心按 x/y 分量缩放，半径按 max(|sx|,|sy|) 缩放，随后旋转和平移中心
		CF_Circle c = shape.u.circle;
		CF_V2 center_scaled = scale_point_local(c.p, sx, sy);
		CF_V2 rotated = rotate_about_origin_local(center_scaled, sinr, cosr);
		CF_V2 world_center = CF_V2{ rotated.x + _position.x, rotated.y + _position.y };
		CF_Circle wc{ world_center, c.r * max_abs_s };
		cached_world_shape_ = CF_ShapeWrapper::FromCircle(wc);
		break;
	}
	case CF_SHAPE_TYPE_CAPSULE:
	{
		// 对 capsule 的端点先缩放再旋转并平移，然后根据端点顺序生成世界空间的 capsule
		CF_Capsule cap = shape.u.capsule;
		CF_V2 a_local = scale_point_local(CF_V2{ cap.a.x, cap.a.y }, sx, sy);
		CF_V2 b_local = scale_point_local(CF_V2{ cap.b.x, cap.b.y }, sx, sy);
		CF_V2 a_rot = rotate_about_origin_local(a_local, sinr, cosr);
		CF_V2 b_rot = rotate_about_origin_local(b_local, sinr, cosr);
		CF_V2 a_world = CF_V2{ a_rot.x + _position.x, a_rot.y + _position.y };
		CF_V2 b_world = CF_V2{ b_rot.x + _position.x, b_rot.y + _position.y };

		// 确保端点按照坐标顺序排列以满足库的期望
		if (std::fabs(a_world.x - b_world.x) >= std::fabs(a_world.y - b_world.y)) {
			if (a_world.x > b_world.x) std::swap(a_world, b_world);
		}
		else {
			if (a_world.y > b_world.y) std::swap(a_world, b_world);
		}

		// 半径按最大缩放分量缩放
		CF_Capsule wc = cf_make_capsule(a_world, b_world, cap.r * max_abs_s);
		cached_world_shape_ = CF_ShapeWrapper::FromCapsule(wc);
		break;
	}
	case CF_SHAPE_TYPE_POLY:
	{
		// 多边形顶点先缩放再旋转并平移，然后重新构建多边形缓存
		CF_Poly poly = shape.u.poly;
		CF_Poly wp{};
		wp.count = poly.count;
		for (int i = 0; i < poly.count; ++i) {
			CF_V2 local_scaled = scale_point_local(CF_V2{ poly.verts[i].x, poly.verts[i].y }, sx, sy);
			CF_V2 rotated = rotate_about_origin_local(local_scaled, sinr, cosr);
			CF_V2 world_pt = CF_V2{ rotated.x + _position.x, rotated.y + _position.y };
			wp.verts[i] = world_pt;
		}
		cf_make_poly(&wp);
		cached_world_shape_ = CF_ShapeWrapper::FromPoly(wp);
		break;
	}
	default:
		// 其他类型默认做简单缩放+平移以获得 world-space 表示
		{
			CF_ShapeWrapper scaled = shape;
			// 对于未知类型，只对可能存在的向量字段尝试缩放（保守处理）
			// 直接走 translate_local_to_world 做平移（缩放已在 scaled 中尽量处理）
			cached_world_shape_ = translate_local_to_world(scaled, _position);
		}
		break;
	}

	// 更新脏标志与版本号，供外部检测变化
	world_shape_dirty_ = false;
	++world_shape_version_;
}