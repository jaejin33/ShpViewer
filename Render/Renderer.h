#pragma once
#include <vector>
#include <memory>
#include "Camera.h"
#include "PolygonRenderer.h"
#include "BoxRenderer.h"
#include "FrustumRenderer.h"
#include "QuadNode.h"

namespace Render {

    // 최상위 렌더러 — 여러 하위 렌더러를 통합 관리하고 한 번의 RenderFrame() 호출로 화면을 구성한다.
    //
    // 렌더링 레이어:
    //   1. PolygonRenderer  : 가시 폴리곤 (선분 형태)
    //   2. BoxRenderer×2    : 객체 MBR / 쿼드트리 노드 MBR (선택적)
    //   3. FrustumRenderer  : 카메라 프러스텀 시각화 (선택적)
    class Renderer {
    private:
        PolygonRenderer  m_polygonRenderer;   // 폴리곤 윤곽선 렌더러
        BoxRenderer      m_nodeMbrRenderer;   // 쿼드트리 노드 MBR 렌더러
        BoxRenderer      m_itemMbrRenderer;   // 개별 객체 MBR 렌더러
        FrustumRenderer  m_frustumRenderer;   // 카메라 프러스텀 렌더러

        std::shared_ptr<Camera> m_camera;     // 공유 카메라 (VP 행렬 제공)

        bool m_showItemMBR  = false; // 객체 MBR 표시 여부
        bool m_showNodeMBR  = false; // 쿼드트리 노드 MBR 표시 여부
        bool m_showFrustum  = false; // 카메라 프러스텀 표시 여부

    public:
        Renderer() = default;
        ~Renderer() = default;

        // GL 컨텍스트가 활성화된 상태에서 호출해야 함 (VAO/VBO/셰이더 초기화)
        void Initialize();

        // 공유 카메라 등록 — RenderFrame()이 VP 행렬을 카메라에서 가져간다
        void SetCamera(std::shared_ptr<Camera> camera) { m_camera = camera; }

        // 폴리곤 업로드; useLevelColors=false 이면 균일 회청색으로 렌더링
        void LoadGeometries(const std::vector<Spatial::ItemInfo>& items, bool useLevelColors = true);

        // MBR 데이터 업로드 (빈 벡터 전달 시 해당 레이어만 비움)
        void LoadMBR(const std::vector<Spatial::NodeInfo>& nodes,
                     const std::vector<Spatial::ItemInfo>& items);
        void ClearMBR(); // 모든 MBR 지오메트리 비우기

        // 프러스텀 지오메트리 업로드 / 지우기
        void LoadFrustum(const glm::vec3& eye, const glm::vec3 corners[4]);
        void ClearFrustum();

        // 각 레이어 표시 토글
        void SetShowItemMBR(bool show) { m_showItemMBR = show; }
        void SetShowNodeMBR(bool show) { m_showNodeMBR = show; }
        void SetShowFrustum(bool show) { m_showFrustum = show; }

        // 현재 표시 상태 조회
        bool GetShowItemMBR() const { return m_showItemMBR; }
        bool GetShowNodeMBR() const { return m_showNodeMBR; }
        bool GetShowFrustum() const { return m_showFrustum; }

        // 한 프레임을 렌더링 (clear → 폴리곤 → MBR → 프러스텀 순)
        void RenderFrame();
    };
}
