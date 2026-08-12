#pragma once		//두 번이상 include 되는 것을 막는 헤더 가드
#include <cmath>	// std::sqrt

struct Vec3
{
	float x = 0.0f;
	float y = 0.0f;
	float z = 0.0f;

	constexpr Vec3() = default; // "constexpr 컴파일 타임에 실행될 수도 있다" 는 뜻
	constexpr Vec3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}

	//산술 연산자
	constexpr Vec3 operator+(const Vec3& rhs) const { return Vec3(x + rhs.x, y + rhs.y, z + rhs.z); }
	constexpr Vec3 operator-(const Vec3& rhs) const { return Vec3(x - rhs.x, y - rhs.y, z - rhs.z); }
	constexpr Vec3 operator-() const { return Vec3(-x, -y, -z); }
	constexpr Vec3 operator*(float s) const { return Vec3(x * s, y * s, z * s); }

	Vec3& operator+=(const Vec3& rhs) { x += rhs.x; y += rhs.y; z += rhs.z; return *this; }
	Vec3& operator-=(const Vec3& rhs) { x -= rhs.x; y -= rhs.y; z -= rhs.z; return *this; }
	Vec3& operator*=(float s) { x*=s; y*=s; z*=s; return *this; }
};

inline constexpr float Vec3Dot(const Vec3& a, const Vec3& b) {
	return a.x * b.x + a.y * b.y + a.z * b.z;
}

inline constexpr Vec3 Vec3Cross(const Vec3& a, const Vec3& b) {
	return Vec3(
		a.y * b.z - a.z * b.y,
		a.z * b.x - a.x * b.z,
		a.x * b.y - a.y * b.x
	);
}

inline float Vec3LengthSquared(const Vec3& v) {	// sqrt를 사용할 필요 없는 상황일 때, 사용
	return Vec3Dot(v, v);
}

inline float Vec3Length(const Vec3& v) {
	return std::sqrt(Vec3LengthSquared(v));
}

inline Vec3 Vec3Normalize(const Vec3& v) {
	float len = Vec3Length(v);
	if (len < 1e-8f) return Vec3(0.0f, 0.0f, 0.0f); // 0벡터 정규화 방지
	return v * (1.0f / len); // 나눗셈 한 번만 하기위해 곱하기로 변경
}