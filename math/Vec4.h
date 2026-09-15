#pragma once
#include <cmath>
#include "Vec3.h"

struct Vec4
{
	float x = 0.0f;
	float y = 0.0f;
	float z = 0.0f;
	float w = 0.0f;

	constexpr Vec4() = default;
	constexpr Vec4(float x_, float y_, float z_, float w_) : x(x_), y(y_), z(z_), w(w_) {}
	// 점(w=1) / 방향(w=0)을 Vec3로 부터 만들 때 쓰는 생성자
	constexpr Vec4(const Vec3& v, float w_) : x(v.x), y(v.y), z(v.z), w(w_) {}
};
// 원근 나눗셈 : 동차 좌표를 실제 3D 좌표로 되돌린다.
// w가 0에 가까우면(= 방향 벡터이거나 카메라 평면상의 점) 나눌 수 없으므로 0벡터 반환
inline Vec3 Vec4PerspectiveDivide(const Vec4& v) {
	if (std::fabs(v.w) < 1e-8f) return Vec3(0.0f, 0.0f, 0.0f);
	const float inv_w = 1.0f / v.w;
	return Vec3(v.x * inv_w, v.y * inv_w, v.z * inv_w);
}