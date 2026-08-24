#include "pch.h"
#include "Frustum.h"

namespace {
	// M의 row번째 행 4개 성분
	struct Row {
		float x, y, z, w;
	};

	Row GetRow(const Mat4& m, int32_t row) {
		return Row{ m.m[row], m.m[4 + row], m.m[8 + row], m.m[12 + row]};
	}

	Plane MakePlane(const Row& a, const Row& b, float sign) {
		return Plane{
			a.x + sign * b.x,
			a.y + sign * b.y,
			a.z + sign * b.z,
			a.w + sign * b.w
		};
	}
}

std::array<Plane, 6> ExtractFrustumPlane(const Mat4& view_projection) {
	Row row0 = GetRow(view_projection, 0);
	Row row1 = GetRow(view_projection, 1);
	Row row2 = GetRow(view_projection, 2);
	Row row3 = GetRow(view_projection, 3);

	std::array<Plane, 6> planes;
	planes[static_cast<int32_t>(FrustumPlaneIndex::kLeft)] = MakePlane(row3, row0, +1.0f);
	planes[static_cast<int32_t>(FrustumPlaneIndex::kRight)] = MakePlane(row3, row0, -1.0f);
	planes[static_cast<int32_t>(FrustumPlaneIndex::kBottom)] = MakePlane(row3, row1, +1.0f);
	planes[static_cast<int32_t>(FrustumPlaneIndex::kTop)] = MakePlane(row3, row1, -1.0f);
	planes[static_cast<int32_t>(FrustumPlaneIndex::kNear)] = MakePlane(row3, row2, +1.0f);
	planes[static_cast<int32_t>(FrustumPlaneIndex::kFar)] = MakePlane(row3, row2, -1.0f);

	return planes;
}

bool IsBoxInsideFrustum(const std::array<Plane, 6>& planes, const Vec3& bounds_min, const Vec3& bounds_max) {
	for (const Plane& plane : planes) {
		float p_x = (plane.a >= 0.0f) ? bounds_max.x : bounds_min.x;
		float p_y = (plane.b >= 0.0f) ? bounds_max.y : bounds_min.y;
		float p_z = (plane.c >= 0.0f) ? bounds_max.z : bounds_min.z;

		float distance = plane.a * p_x + plane.b * p_y + plane.c * p_z + plane.d;
		if (distance < 0.0f) {
			return false;
		}
	}
	return true;
}