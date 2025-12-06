#pragma once
#include<cute_math.h>

// 2D 向量基础数学工具（面向使用者的简单接口）
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
}