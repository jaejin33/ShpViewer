#include "pch.h"
#include "Renderer.h"

namespace Render {

    // GL 컨텍스트가 현재 스레드에 바인딩된 상태에서 호출해야 한다.
    // 블렌딩 활성화 및 각 하위 렌더러의 VAO/VBO/셰이더를 초기화한다.
    void Renderer::Initialize() {
        // 알파 블렌딩 활성화 (반투명 객체 지원)
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        m_polygonRenderer.Initialize();  // 폴리곤 렌더러 초기화
        m_nodeMbrRenderer.Initialize();  // 노드 MBR 렌더러 초기화
        m_itemMbrRenderer.Initialize();  // 객체 MBR 렌더러 초기화
        m_frustumRenderer.Initialize();  // 프러스텀 렌더러 초기화
    }

    // 가시 폴리곤 리스트를 GPU에 업로드한다.
    // useLevelColors=true : 쿼드트리 깊이별 색상, false : 단일 회청색
    void Renderer::LoadGeometries(const std::vector<Spatial::ItemInfo>& items, bool useLevelColors) {
        m_polygonRenderer.UploadData(items, useLevelColors);
    }

    // 쿼드트리 노드 MBR과 객체 MBR을 GPU에 업로드한다.
    // 빈 벡터를 전달하면 해당 레이어의 기존 데이터가 지워진다.
    void Renderer::LoadMBR(const std::vector<Spatial::NodeInfo>& nodes,
                            const std::vector<Spatial::ItemInfo>& items)
    {
        m_nodeMbrRenderer.UploadNodes(nodes);
        m_itemMbrRenderer.UploadItems(items);
    }

    // MBR 지오메트리를 모두 비운다 (토글 OFF 시 호출)
    void Renderer::ClearMBR() {
        m_nodeMbrRenderer.Clear();
        m_itemMbrRenderer.Clear();
    }

    // 카메라 프러스텀 지오메트리를 GPU에 업로드한다.
    //   eye     : 월드 공간 카메라 눈 위치
    //   corners : 화면 4 코너의 지면(Z=0) 투영점 [BL, BR, TR, TL]
    void Renderer::LoadFrustum(const glm::vec3& eye, const glm::vec3 corners[4]) {
        m_frustumRenderer.Upload(eye, corners);
    }

    // 프러스텀 지오메트리를 비운다 (토글 OFF 시 호출)
    void Renderer::ClearFrustum() {
        m_frustumRenderer.Clear();
    }

    // 한 프레임을 렌더링한다.
    //   1. 배경 클리어 (어두운 회색)
    //   2. 폴리곤 윤곽선 렌더링 (항상)
    //   3. 객체/노드 MBR 렌더링 (토글 ON일 때만)
    //   4. 프러스텀 렌더링 (토글 ON일 때만)
    void Renderer::RenderFrame() {
        glClearColor(0.15f, 0.15f, 0.15f, 1.0f); // 배경색: 어두운 회색
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        if (!m_camera) return;

        glm::mat4 vp = m_camera->GetViewProjectionMatrix();

        m_polygonRenderer.Draw(vp); // 폴리곤 (항상 렌더링)

        if (m_showItemMBR) m_itemMbrRenderer.Draw(vp); // 객체 MBR
        if (m_showNodeMBR) m_nodeMbrRenderer.Draw(vp); // 쿼드트리 노드 MBR
        if (m_showFrustum) m_frustumRenderer.Draw(vp); // 카메라 프러스텀
    }
}
