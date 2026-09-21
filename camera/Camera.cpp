#include "pch.h"
#include "Camera.h"

void Camera::MakeBasis(float yaw, float pitch, Vec3& right, Vec3& up, Vec3& back) {
    float cos_pitch = std::cos(pitch);
    float sin_pitch = std::sin(pitch);
    float sin_yaw = std::sin(yaw);
    float cos_yaw = std::cos(yaw);

    // back = 카메라가 등지고 있는 방향 (pivot에서 eye로 가는 쪽)
    back = Vec3(cos_pitch * sin_yaw, sin_pitch, cos_pitch * cos_yaw);
    right = Vec3Normalize(Vec3Cross(Vec3(0.0f, 1.0f, 0.0f), back));
    up = Vec3Cross(back, right);
}

void Camera::Commit() {
    eye_ = pivot_ + offset_;

    Vec3 right, up, back;
    MakeBasis(yaw_, pitch_, right, up, back);

    // 축으로 직접 만든다. 큰 좌표끼리 빼지 않으므로 시선이 떨리지 않는다.
    view_ = Mat4ViewFromBasis(right, up, back, eye_);
}

void Camera::PlaceOnPivot() {
    Vec3 right, up, back;
    MakeBasis(yaw_, pitch_, right, up, back);
    offset_ = back * distance_;
    Commit();
}

Camera::Camera(const Vec3& pivot, float distance, float yaw, float pitch)
    : pivot_(pivot), distance_(distance), yaw_(yaw), pitch_(pitch) {
    if (distance_ < kMinDistance) distance_ = kMinDistance;
    PlaceOnPivot();
}

void Camera::SetPivot(const Vec3& pivot) {
    // 큰 좌표 뺄셈은 여기 한 번뿐 — 우클릭/휠 시점에만 일어나므로 오차가 쌓이지 않는다.
    pivot_ = pivot;
    offset_ = eye_ - pivot_;
    distance_ = Vec3Length(offset_);

    if (distance_ < kMinDistance) {   // 카메라와 피벗이 겹칠 뻔한 예외
        distance_ = kMinDistance;
        PlaceOnPivot();
        return;
    }
    Commit();   // eye_도 시선도 그대로 — 화면은 안 움직인다.
}

void Camera::Rotate(float delta_yaw, float delta_pitch) {
    // 1. 돌리기 전 카메라의 세 축
    Vec3 right0, up0, back0;
    MakeBasis(yaw_, pitch_, right0, up0, back0);

    // 2. 피벗에서 카메라로 가는 벡터를 "카메라 기준" 세 숫자로 적어둔다.
    const float a = Vec3Dot(offset_, right0);
    const float b = Vec3Dot(offset_, up0);
    const float c = Vec3Dot(offset_, back0);

    // 3. 방향만 돌린다.
    yaw_ += delta_yaw;
    pitch_ += delta_pitch;
    if (pitch_ > kMaxPitchRadians) pitch_ = kMaxPitchRadians;
    if (pitch_ < -kMaxPitchRadians) pitch_ = -kMaxPitchRadians;

    // 4. 돌린 뒤의 세 축
    Vec3 right1, up1, back1;
    MakeBasis(yaw_, pitch_, right1, up1, back1);

    // 5. a/b/c가 새 축 기준으로도 똑같이 나오도록 카메라를 되놓는다.
    offset_ = right1 * a + up1 * b + back1 * c;

    // 6. 강체 회전이라 길이는 변할 수 없다 — 쌓이기 전에 매번 오차를 지운다.
    offset_ = Vec3Normalize(offset_) * distance_;

    Commit();
}

void Camera::Pan(float delta_right, float delta_up) {
    const float world_units_per_pixel = distance_ * kPanSensitivity;

    Vec3 right, up, back;
    MakeBasis(yaw_, pitch_, right, up, back);
    const Vec3 ground_forward = Vec3Normalize(Vec3(-back.x, 0.0f, -back.z));

    const Vec3 move = right * (delta_right * world_units_per_pixel)
        + ground_forward * (delta_up * world_units_per_pixel);

    // offset_은 그대로 두고 기준점만 옮기면 카메라가 같은 만큼 따라온다.
    pivot_ += move;
    Commit();
}

void Camera::Zoom(float scale_factor) {
    distance_ *= scale_factor;
    if (distance_ < kMinDistance) distance_ = kMinDistance;

    // 방향은 그대로, 길이만 바꾼다 -> 피벗은 화면의 같은 픽셀에 남는다.
    offset_ = Vec3Normalize(offset_) * distance_;
    Commit();
}

void Camera::Recenter(const Vec3& pivot, float distance) {
    pivot_ = pivot;
    distance_ = distance;
    if (distance_ < kMinDistance) distance_ = kMinDistance;
    PlaceOnPivot();
}