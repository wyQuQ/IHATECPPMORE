#pragma once
#include<cute_math.h>
#include<cmath>

inline constexpr float pi = 3.14159265358979f;

// 2D 向量基础数学工具（面向使用者的简单接口）
// 说明：这些函数封装了 Cute Framework 的 CF_V2 及 math 操作，目标是提供轻量且无异常的工具函数。
// 注意数值稳定性：normalized/length 等在接近 0 的输入下做了保护处理以避免除以零。
namespace v2math {
	inline CF_V2 zero() { return CF_V2{ 0.0f, 0.0f }; }
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
	inline bool equal(const CF_V2& a, const CF_V2& b) noexcept {
		const float eps = 1e-6f;
		return (std::fabs(a.x - b.x) < eps) && (std::fabs(a.y - b.y) < eps);
	}
	inline CF_V2 rotate(const CF_V2& v, float angle_rad) noexcept {
		float cos_a = cf_cos(angle_rad);
		float sin_a = cf_sin(angle_rad);
		return CF_V2{
			v.x * cos_a - v.y * sin_a,
			v.x * sin_a + v.y * cos_a
		};
	}
	inline CF_V2 get_dir(float angle_rad) noexcept {
		return CF_V2{ cf_cos(angle_rad), cf_sin(angle_rad) };
	}
	inline float get_angle(const CF_V2& dir) noexcept {
		return cf_atan2(dir.y, dir.x); // 返回值范围 [-pi, pi]
	}
	inline CF_V2 angled(const CF_V2& v, float angle_rad) noexcept {
		float len = length(v);
		CF_V2 dir = get_dir(angle_rad);
		return dir * len;
	}
}