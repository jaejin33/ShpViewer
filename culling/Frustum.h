#pragma once

#include <array>
#include <cstdint>

#include "../math/Mat4.h"

// 프러스텀을 이루는 평면 하나
struct Plane {
	float a = 0.0f;
	float b = 0.0f;
	float c = 0.0f;
	float d = 0.0f;
};

enum class FrustumPlaneIndex : int32_t {
	kLeft = 0,
	kRight = 1,
	kBottom = 2,
	kTop = 3, 
	kNear = 4,
	kFar = 5,
};

// 반환되는 6개 평면의 순서는 FrustumPlaneIndex와 일치
std::array<Plane, 6> ExtractFrustumPlane(const Mat4& view_projection);

// bounds_min~bounds_max로 이루어진 3D bbox가 6개 평면으로 이루어진 프러스텀 밖에 있는지 검사
// false면 컬링 대상, true면 완전히 밖은 아님
bool IsBoxInsideFrustum(const std::array<Plane, 6>& planes, const Vec3& bounds_min, const Vec3& bounds_max);
