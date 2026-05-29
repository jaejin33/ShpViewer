#include "pch.h"
#include "Camera.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <algorithm>
#include <cfloat>
#include <cmath>

namespace Render {

    Camera::Camera(float aspect, float initialDist)
        : m_aspect(aspect), m_distance(initialDist)
    {
        RecalculateMatrix();
    }

    // ---------------------------------------------------------------------------
    // 월드 공간 내 눈(eye) 위치 계산
    //   eye = target + distance * (cos(pitch)*sin(yaw), -cos(pitch)*cos(yaw), sin(pitch))
    // ---------------------------------------------------------------------------
    glm::vec3 Camera::GetEyePosition() const
    {
        float cp = std::cos(m_pitch), sp = std::sin(m_pitch);
        float cy = std::cos(m_yaw),   sy = std::sin(m_yaw);
        return m_target + glm::vec3(
             m_distance * cp * sy,   // X : 동쪽 방향 성분
            -m_distance * cp * cy,   // Y : 북쪽 방향 성분
             m_distance * sp         // Z : 높이 성분
        );
    }

    // ---------------------------------------------------------------------------
    // 현재 카메라 상태로부터 뷰-프로젝션 행렬을 재계산
    //   near : distance * 0.001 (최소 1)  — 지리 스케일에서 깊이 정밀도 유지
    //   far  : distance * 100             — 시야 끝단 충분히 커버
    // ---------------------------------------------------------------------------
    void Camera::RecalculateMatrix()
    {
        glm::vec3 eye = GetEyePosition();
        glm::vec3 up  = glm::vec3(0.0f, 0.0f, 1.0f); // 월드 Z축을 위 방향으로 사용

        float nearZ = std::max(1.0f, m_distance * 0.001f);
        float farZ  = m_distance * 100.0f;

        glm::mat4 view = glm::lookAt(eye, m_target, up);
        glm::mat4 proj = glm::perspective(m_fov, m_aspect, nearZ, farZ);
        m_viewProjMatrix = proj * view;
    }

    // ---------------------------------------------------------------------------
    // 위치 / 이동 함수
    // ---------------------------------------------------------------------------
    void Camera::SetPosition(const glm::vec2& pos)
    {
        // 타겟을 XY 절대 좌표로 이동 (Z는 항상 0 = 지면)
        m_target = glm::vec3(pos.x, pos.y, 0.0f);
        RecalculateMatrix();
    }

    void Camera::Move(const glm::vec2& worldDelta)
    {
        // 타겟을 월드 XY 방향으로 상대 이동 (마우스 드래그에서 호출)
        m_target.x += worldDelta.x;
        m_target.y += worldDelta.y;
        RecalculateMatrix();
    }

    void Camera::Pan(float forwardDelta, float rightDelta)
    {
        // 카메라가 바라보는 수평 방향(yaw 기준) 및 수직 방향으로 이동
        //   forward 방향: (-sin(yaw), cos(yaw))  — pitch 무시, 지면 투영
        //   right   방향: ( cos(yaw), sin(yaw))  — forward의 90도 CW 회전
        float sy = std::sin(m_yaw), cy = std::cos(m_yaw);
        glm::vec2 fwd(-sy,  cy);
        glm::vec2 rgt( cy,  sy);

        m_target.x += fwd.x * forwardDelta + rgt.x * rightDelta;
        m_target.y += fwd.y * forwardDelta + rgt.y * rightDelta;
        RecalculateMatrix();
    }

    // ---------------------------------------------------------------------------
    // 줌 함수
    // ---------------------------------------------------------------------------
    void Camera::SetZoom(float dist)
    {
        m_distance = std::max(dist, 1.0f); // 최소 거리 1 보장 (glm::lookAt 안전)
        RecalculateMatrix();
    }

    void Camera::AddZoom(float delta)
    {
        SetZoom(m_distance + delta);
    }

    // ---------------------------------------------------------------------------
    // 방향 함수 (yaw / pitch)
    // ---------------------------------------------------------------------------
    void Camera::SetYaw(float yaw)
    {
        m_yaw = yaw;
        RecalculateMatrix();
    }

    void Camera::AddYaw(float delta)
    {
        m_yaw += delta;
        RecalculateMatrix();
    }

    void Camera::SetPitch(float pitch)
    {
        // 상한(89도): glm::lookAt은 forward == up일 때 (pitch=90도) 특이점이 생긴다.
        // 하한(0.5도): 눈과 타겟의 Z가 거의 같아질 때 near 클리핑이 불안정해지는 것을 방지.
        const float MIN_P = 0.0087f;  // ~0.5도
        const float MAX_P = 1.5533f;  // 89도
        m_pitch = std::max(MIN_P, std::min(MAX_P, pitch));
        RecalculateMatrix();
    }

    void Camera::AddPitch(float delta)
    {
        SetPitch(m_pitch + delta);
    }

    void Camera::SetAspectRatio(float width, float height)
    {
        if (height > 0.0f) {
            m_aspect = width / height;
            RecalculateMatrix();
        }
    }

    // ---------------------------------------------------------------------------
    // NDC 좌표 → Z=0 지면 역투영 (마우스 드래그 월드 좌표 계산에 사용)
    //   1. VP 역행렬로 NDC near/far 점을 월드 공간으로 변환
    //   2. 광선 방향 계산
    //   3. 광선과 Z=0 평면 교점 (t = -eye.z / dir.z)
    //   광선이 지면과 평행(dir.z ≈ 0)이거나 뒤를 향하면(t < 0) 타겟 반환
    // ---------------------------------------------------------------------------
    glm::vec3 Camera::UnprojectToGround(float ndcX, float ndcY) const
    {
        glm::mat4 invVP = glm::inverse(m_viewProjMatrix);
        glm::vec3 eye   = GetEyePosition();

        // NDC -1(near)과 +1(far) 두 점을 월드 공간으로 변환
        glm::vec4 wNear = invVP * glm::vec4(ndcX, ndcY, -1.0f, 1.0f);
        glm::vec4 wFar  = invVP * glm::vec4(ndcX, ndcY,  1.0f, 1.0f);
        if (std::abs(wNear.w) < 1e-8f || std::abs(wFar.w) < 1e-8f) return m_target;
        wNear /= wNear.w;
        wFar  /= wFar.w;

        glm::vec3 dir = glm::normalize(glm::vec3(wFar) - glm::vec3(wNear));
        if (std::abs(dir.z) < 1e-6f) return m_target;  // 광선이 지면과 평행

        float t = -eye.z / dir.z;
        if (t < 0.0f) return m_target;                  // 교점이 카메라 뒤에 있음
        return eye + t * dir;
    }

    // ---------------------------------------------------------------------------
    // 가시 영역 AABB 계산 (공간 쿼리 컬링에 사용)
    //
    // 카메라가 수평에 가깝게 기울어지면 화면 상단 코너 광선이 Z=0과 교차하지 않는다.
    // 이런 경우 UnprojectToGround는 m_target을 반환하므로 가시 영역이 과소추정된다.
    // 보정을 위해 분석적 최대 가시거리를 구해 기본 범위로 사용한다:
    //   farBound = distance / sin(pitch)   (pitch가 충분히 클 때)
    //   farBound = distance * 20            (거의 수평 시)
    // ---------------------------------------------------------------------------
    GeoData::BoundingBox Camera::GetViewBounds() const
    {
        float sinP     = std::sin(m_pitch);
        float farBound = (sinP > 0.05f) ? (m_distance / sinP)
                                        : (m_distance * 20.0f);

        // 타겟 주변 farBound 범위를 초기 경계로 설정.
        // 유효한 코너 교점은 가까운 쪽 경계를 좁혀줄 수 있다.
        float minX = m_target.x - farBound, maxX = m_target.x + farBound;
        float minY = m_target.y - farBound, maxY = m_target.y + farBound;

        // NDC 4 코너: BL, BR, TR, TL
        const float cx[4] = { -1.0f,  1.0f,  1.0f, -1.0f };
        const float cy[4] = { -1.0f, -1.0f,  1.0f,  1.0f };

        for (int i = 0; i < 4; ++i) {
            glm::vec3 hit = UnprojectToGround(cx[i], cy[i]);
            // 수평선 너머 광선은 m_target을 반환하므로 제외
            // (farBound로 이미 커버되어 있음)
            if (glm::distance(hit, m_target) < 1.0f) continue;
            if (hit.x < minX) minX = hit.x;
            if (hit.y < minY) minY = hit.y;
            if (hit.x > maxX) maxX = hit.x;
            if (hit.y > maxY) maxY = hit.y;
        }

        // 화면 경계에서 객체가 팝핑되지 않도록 5% 여유 추가
        float mx = (maxX - minX) * 0.05f;
        float my = (maxY - minY) * 0.05f;
        return GeoData::BoundingBox(minX - mx, minY - my, maxX + mx, maxY + my);
    }

    // ---------------------------------------------------------------------------
    // 프러스텀 코너 계산 — 화면 4 코너를 Z=0 지면으로 투영
    //
    // 수평선 너머를 향하는 광선(dir.z >= 0)은 Z=0과 교차하지 않는다.
    // 이 경우 광선을 XY 방향으로 farDist까지 연장한 뒤 Z=0으로 클램핑한다.
    // 모든 결과는 farDist 이내로 클램핑되어 무한대 좌표를 방지한다.
    //
    // 출력 순서: BL(-1,-1), BR(+1,-1), TR(+1,+1), TL(-1,+1)
    // ---------------------------------------------------------------------------
    void Camera::GetFrustumCorners(glm::vec3 out[4]) const
    {
        glm::mat4 invVP = glm::inverse(m_viewProjMatrix);
        glm::vec3 eye   = GetEyePosition();

        // 최대 가시 거리 계산 (pitch가 낮을수록 지평선이 멀어짐)
        float sinP    = std::sin(m_pitch);
        float farDist = (sinP > 0.05f) ? (m_distance / sinP) : (m_distance * 20.0f);

        const float ndcX[4] = { -1.0f,  1.0f,  1.0f, -1.0f };
        const float ndcY[4] = { -1.0f, -1.0f,  1.0f,  1.0f };

        for (int i = 0; i < 4; ++i) {
            glm::vec4 wNear = invVP * glm::vec4(ndcX[i], ndcY[i], -1.0f, 1.0f);
            glm::vec4 wFar  = invVP * glm::vec4(ndcX[i], ndcY[i],  1.0f, 1.0f);

            if (std::abs(wNear.w) < 1e-8f || std::abs(wFar.w) < 1e-8f) {
                // 역행렬 특이점 — 카메라 앞 방향으로 farDist 거리의 점 사용
                out[i] = glm::vec3(eye.x, eye.y + farDist, 0.0f);
                continue;
            }
            wNear /= wNear.w;
            wFar  /= wFar.w;
            glm::vec3 dir = glm::normalize(glm::vec3(wFar) - glm::vec3(wNear));

            glm::vec3 hit;
            if (dir.z < -1e-6f) {
                // 광선이 아래를 향함 → Z=0 평면과 교차
                float t = -eye.z / dir.z;
                hit = eye + t * dir;
            } else {
                // 광선이 위를 향하거나 수평 → XY 방향으로 farDist까지 연장
                float xyLen = std::sqrt(dir.x * dir.x + dir.y * dir.y);
                float t     = (xyLen > 1e-6f) ? (farDist / xyLen) : farDist;
                hit = eye + t * dir;
                hit.z = 0.0f; // Z=0 지면으로 강제 클램핑
            }

            // 눈으로부터 XY 거리를 farDist 이내로 클램핑
            float dx     = hit.x - eye.x;
            float dy     = hit.y - eye.y;
            float xyDist = std::sqrt(dx * dx + dy * dy);
            if (xyDist > farDist) {
                float scale = farDist / xyDist;
                hit.x = eye.x + dx * scale;
                hit.y = eye.y + dy * scale;
                hit.z = 0.0f;
            }

            out[i] = hit;
        }
    }

} // namespace Render
