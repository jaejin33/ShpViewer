#pragma once
#include "../math/Vec3.h"
#include "../math/Mat4.h"

// 3인칭 궤도(orbit) 카메라.
// pivot_ : 회전/줌의 기준점. 우클릭으로 찍은 지점이 여기로 들어온다.
// eye_   : 카메라 위치.  yaw_/pitch_ : 카메라가 보는 방향.
// 불변식 : distance_ == |eye_ - pivot_|  — 팬/줌 속도의 축척 기준.
// 시선은 yaw/pitch만으로 만든다. pivot은 시선에 관여하지 않는다.
class Camera {
public:
    Camera(const Vec3& pivot, float distance, float yaw, float pitch);

    // 우클릭 드래그: pivot을 축으로 카메라를 통째로 돌린다.
    void Rotate(float delta_yaw, float delta_pitch);

    // 좌클릭 드래그: 카메라와 pivot을 화면 기준 좌우/상하로 함께 옮긴다.
    void Pan(float delta_right, float delta_up);

    // 휠: pivot을 향해 다가가거나 멀어진다.
    void Zoom(float scale_factor);

    // 데이터 로드 직후 등, 카메라를 pivot 정면에 새로 배치한다.
    void Recenter(const Vec3& pivot, float distance);

    // 회전/줌 기준점만 바꾼다. 위치도 시선도 그대로라 화면은 안 움직인다.
    void SetPivot(const Vec3& pivot);

    Vec3 GetEye() const { return eye_; }
    Vec3 GetPivot() const { return pivot_; }
    const Mat4& GetViewMatrix() const { return view_; }

#ifdef ENABLE_CULLING_STATS
    float GetDistance() const { return distance_; }
    float GetYaw() const { return yaw_; }
    float GetPitch() const { return pitch_; }
#endif

private:
    // yaw/pitch가 가리키는 카메라의 세 축. back은 카메라가 등지고 있는 방향.
    static void MakeBasis(float yaw, float pitch, Vec3& right, Vec3& up, Vec3& back);

    // offset_/yaw_/pitch_ -> eye_와 view_를 다시 만든다.
    void Commit();

    // pivot_ 정면 distance_ 지점에 카메라를 놓는다(생성/Recenter 전용).
    void PlaceOnPivot();

    Vec3 pivot_;     // 회전/줌 기준점. 월드 좌표라 값이 크다(38만 대).
    Vec3 offset_;    // eye_ - pivot_. 값이 작아 오차가 안 쌓인다 — 진짜 상태는 이쪽.
    float distance_; // 항상 |offset_|. 팬/줌 속도의 축척 기준.
    float yaw_;
    float pitch_;
    Vec3 eye_;       // pivot_ + offset_ (캐시)
    Mat4 view_;      // 캐시

    static constexpr float kMaxPitchRadians = 1.5533f;  // 약 89도
    static constexpr float kMinDistance = 1.0f;
    static constexpr float kPanSensitivity = 0.001f;
};