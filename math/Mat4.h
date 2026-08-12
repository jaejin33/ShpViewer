#pragma once
#include <cmath>
#include "Vec3.h"

struct Mat4 {
	float m[16] = {};

	Mat4 operator*(const Mat4& rhs) const {
		Mat4 result{};
		for (int col = 0; col < 4; ++col) 
		{
			for (int row = 0; row < 4; ++row) 
			{
				float sum = 0.0f;
				for (int k = 0; k < 4; ++k)
					sum += m[k * 4 + row] * rhs.m[col * 4 + k];
				result.m[col * 4 + row] = sum;
			}
		}
		return result;
	}

	Vec3 operator*(const Vec3& v) const
	{
		return Vec3(
			m[0] * v.x + m[4] * v.y + m[8] * v.z + m[12],
			m[1] * v.x + m[5] * v.y + m[9] * v.z + m[13],
			m[2] * v.x + m[6] * v.y + m[10] * v.z + m[14]
		);
	}
};

inline Mat4 Mat4Identity()
{
	Mat4 result{};
	result.m[0] = 1.0f;
	result.m[5] = 1.0f;
	result.m[10] = 1.0f;
	result.m[15] = 1.0f;
	return result;
}

inline Mat4 Mat4Translate(const Vec3& t)
{
	Mat4 result = Mat4Identity();
	result.m[12] = t.x;
	result.m[13] = t.y;
	result.m[14] = t.z;
	return result;
}

inline Mat4 Mat4RotateY(float radians) {
	Mat4 result = Mat4Identity();
	float c = std::cos(radians);
	float s = std::sin(radians);
	result.m[0] = c;
	result.m[2] = -s;
	result.m[8] = s;
	result.m[10] = c;
	return result;
}

inline Mat4 Mat4LookAt(const Vec3& eye, const Vec3& target, const Vec3& up)
{
	Vec3 zaxis = Vec3Normalize(eye - target);
	Vec3 xaxis = Vec3Normalize(Vec3Cross(up, zaxis));
	Vec3 yaxis = Vec3Cross(zaxis, xaxis);

	Mat4 result{};
	result.m[0] = xaxis.x; result.m[4] = xaxis.y; result.m[8] = xaxis.z; result.m[12] = -Vec3Dot(xaxis, eye);
	result.m[1] = yaxis.x; result.m[5] = yaxis.y; result.m[9] = yaxis.z; result.m[13] = -Vec3Dot(yaxis, eye);
	result.m[2] = zaxis.x; result.m[6] = zaxis.y; result.m[10] = zaxis.z; result.m[14] = -Vec3Dot(zaxis, eye);
	result.m[15] = 1.0f;
	return result;
}

inline Mat4 Mat4Perspective(float fovYRadians, float aspect, float nearZ, float farZ) {
	Mat4 result{};
	float f = 1.0f / std::tan(fovYRadians * 0.5f);

	result.m[0] = f / aspect;
	result.m[5] = f;
	result.m[10] = (farZ + nearZ) / (nearZ - farZ);
	result.m[11] = -1.0f;
	result.m[14] = (2.0f * farZ * nearZ) / (nearZ - farZ);
	return result;
}