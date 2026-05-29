#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "IGeometry.h"

namespace Render {

    // 3D 오빗(궤도) 카메라 — GIS 지리 데이터 전용
    //
    // Z=0 평면 위의 타겟(target) 지점을 중심으로 공전한다.
    //   yaw      : Z축 기준 수평 회전 (방위각), 라디안
    //   pitch    : 수평선 위쪽 앙각, 라디안; [~0.5도, 89도] 범위로 클램핑
    //   distance : 눈(eye) ↔ 타겟 거리 (줌 역할)
    //
    // 눈(eye) 위치 계산:
    //   eye.x = target.x + distance * cos(pitch) * sin(yaw)
    //   eye.y = target.y - distance * cos(pitch) * cos(yaw)
    //   eye.z =            distance * sin(pitch)
    //
    // 권장 키보드 배치:
    //   W/S     : 앞/뒤 이동 (yaw 방향)
    //   A/D     : 좌/우 이동 (스트레이프)
    //   Q/E     : 좌/우 수평 회전 (yaw)
    //   R/F     : 위/아래 기울기 (pitch)
    //   휠      : 줌 (distance)

    class Camera {
    private:
        glm::vec3 m_target   = { 0.0f, 0.0f, 0.0f }; // 카메라가 바라보는 지면 목표점
        float     m_distance = 100000.0f;              // 눈 ↔ 타겟 거리 (줌)
        float     m_yaw      = 0.0f;                   // 수평 회전각 (라디안)
        float     m_pitch    = 1.0472f;                // 앙각 (라디안, 기본값 60도)
        float     m_fov      = 0.7854f;                // 수직 화각 (라디안, 기본값 45도)
        float     m_aspect   = 1.0f;                   // 화면 종횡비 (width / height)

        glm::mat4 m_viewProjMatrix = glm::mat4(1.0f);  // 최종 뷰-프로젝션 행렬 (캐시)

    public:
        // aspect : 초기 종횡비, initialDist : 초기 줌 거리
        Camera(float aspect = 1.0f, float initialDist = 100000.0f);

        // 현재 상태(target, distance, yaw, pitch)로부터 뷰-프로젝션 행렬을 재계산
        void RecalculateMatrix();

        // --- 위치 / 이동 ---
        void SetPosition(const glm::vec2& pos);    // 지면 타겟 XY 절대 이동
        void Move(const glm::vec2& worldDelta);    // 타겟을 XY 방향으로 상대 이동 (마우스 드래그 호환)

        // 카메라 기준 지면 이동: forward = yaw 방향, right = 수직 방향
        void Pan(float forwardDelta, float rightDelta);

        // --- 줌 ---
        void SetZoom(float dist);   // 절대 거리 설정
        void AddZoom(float delta);  // 상대 거리 변경

        // --- 방향 ---
        void SetYaw(float yaw);     // yaw 절대 설정
        void AddYaw(float delta);   // yaw 상대 변경
        void SetPitch(float pitch); // pitch 절대 설정 (자동 클램핑)
        void AddPitch(float delta); // pitch 상대 변경

        // 2D 카메라 호환 별칭 (이전 코드와의 호환성 유지)
        void SetRotation(float r)     { SetYaw(r); }
        void AddRotation(float delta) { AddYaw(delta); }

        // 화면 크기가 변경될 때 종횡비 갱신
        void SetAspectRatio(float width, float height);

        // --- 조회 ---
        const glm::mat4& GetViewProjectionMatrix() const { return m_viewProjMatrix; }
        float     GetZoom()     const { return m_distance; }
        float     GetRotation() const { return m_yaw; }
        float     GetPitch()    const { return m_pitch; }
        float     GetFov()      const { return m_fov; }
        glm::vec2 GetPosition() const { return { m_target.x, m_target.y }; }
        glm::vec3 GetEyePosition() const; // 월드 공간 내 눈 위치 반환

        // 타겟 거리 기준으로 화면 1픽셀이 대응하는 월드 크기.
        // pitch와 무관하게 안정적인 LOD 임계값 계산에 사용.
        float GetWorldPerPixel(float screenHeight) const {
            if (screenHeight <= 0.0f) return 1.0f;
            return (m_distance * 2.0f * std::tan(m_fov * 0.5f)) / screenHeight;
        }

        // 화면에 보이는 지면 영역의 AABB (공간 컬링 / LOD 쿼리에 사용)
        GeoData::BoundingBox GetViewBounds() const;

        // NDC 좌표를 Z=0 지면으로 역투영(unproject).
        // ndcX [-1,1], ndcY [-1,1] (Y 위 방향).
        // 광선이 지면과 거의 평행하면 타겟 위치를 반환.
        glm::vec3 UnprojectToGround(float ndcX, float ndcY) const;

        // 화면 4 코너를 지면(Z=0)으로 투영한 결과를 out[4]에 저장.
        // 순서: BL(-1,-1), BR(+1,-1), TR(+1,+1), TL(-1,+1)
        // 수평선 너머를 향하는 광선(Z=0 미교차)은 최대 가시거리까지 연장 후 Z=0으로 클램핑.
        void GetFrustumCorners(glm::vec3 out[4]) const;
    };

} // namespace Render
