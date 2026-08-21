#pragma once
#include "../math/Vec3.h"
#include "../math/Mat4.h"

// 3인칭 궤도(orbit) 카메라.
// target을 중심으로 distance만큼 떨어진 채 yaw(수평 회전)/pitch(수직 회전)로 공전한다.
// eye는 target/distance/yaw/pitch로부터 계산되는 캐시 값 — 이 네 값이 바뀔 때만 재계산한다.
class Camera {
public:
    Camera(const Vec3& target, float distance, float yaw, float pitch);

    // 우클릭 드래그: target을 중심으로 회전.
    // deltaYaw/deltaPitch는 라디안 단위 (마우스 픽셀 -> 라디안 변환은 호출하는 쪽 책임).
    void Rotate(float delta_yaw, float delta_pitch);

    // 좌클릭 드래그: 화면 기준 좌우(right)/상하(up)로 target을 이동(pan).
    void Pan(float delta_right, float delta_up);

    void Recenter(const Vec3& target, float distance);

    Vec3 GetEye() const { return eye_; }
    Vec3 GetTarget() const { return target_; }
    Mat4 GetViewMatrix() const { return Mat4LookAt(eye_, target_, Vec3(0.0f, 1.0f, 0.0f)); }

    void Zoom(float scale_factor);

private:
    // target_/distance_/yaw_/pitch_로부터 eye_를 다시 계산해서 캐시한다.
    // Rotate()/Pan()이 값을 바꾼 뒤에는 반드시 이 함수를 호출해야 한다.
    void RecomputeEye();

    Vec3 target_;
    float distance_;
    float yaw_;    // 라디안, target 기준 수평 회전각
    float pitch_;  // 라디안, target 기준 수직 회전각
    Vec3 eye_;     // 캐시된 카메라 월드 좌표

    // pitch가 ±90도에 가까워지면 Mat4LookAt 내부에서 카메라 right 벡터가
    // 거의 0벡터가 되어 뷰 행렬이 깨진다 — 그 전에 멈추기 위한 안전 한계.
    static constexpr float kMaxPitchRadians = 1.5533f;  // 약 89도
    static constexpr float kMinDistance = 1.0f; // 0 이하로 가까워지는 것을 방지
    static constexpr float kPanSensitivity = 0.001f;    // distance_ 대비, 픽셀당 이동 비율
};