#include "pch.h"
#include "Camera.h"

Camera::Camera(const Vec3& target, float distance, float yaw, float pitch) 
: target_(target), distance_(distance), yaw_(yaw), pitch_(pitch){
	RecomputeEye();
}

void Camera::Rotate(float delta_yaw, float delta_pitch) {
    yaw_ += delta_yaw;
    pitch_ += delta_pitch;

    if (pitch_ > kMaxPitchRadians) pitch_ = kMaxPitchRadians;
    if (pitch_ < -kMaxPitchRadians) pitch_ = -kMaxPitchRadians;

    RecomputeEye();
}

void Camera::Pan(float delta_right, float delta_up) {
    float world_units_per_pixel = distance_ * kPanSensitivity;

    Vec3 forward = Vec3Normalize(target_ - eye_);
    Vec3 right = Vec3Normalize(Vec3Cross(forward, Vec3(0.0f, 1.0f, 0.0f)));
    Vec3 ground_forward = Vec3Normalize(Vec3(forward.x, 0.0f, forward.z));

    target_ += right * (delta_right * world_units_per_pixel) + ground_forward * (delta_up * world_units_per_pixel);

    RecomputeEye();
}

void Camera::RecomputeEye() {
    float cos_pitch = std::cos(pitch_);
    float sin_pitch = std::sin(pitch_);
    float sin_yaw = std::sin(yaw_);
    float cos_yaw = std::cos(yaw_);

    Vec3 offset(distance_ * cos_pitch * sin_yaw,
        distance_ * sin_pitch,
        distance_ * cos_pitch * cos_yaw);

    eye_ = target_ + offset;
}

void Camera::Recenter(const Vec3& target, float distance) {
    target_ = target;
    distance_ = distance;
    RecomputeEye();
}

void Camera::Zoom(float scale_factor) {
    distance_ *= scale_factor;
    if (distance_ < kMinDistance) distance_ = kMinDistance;
    RecomputeEye();
}