#pragma once
#include <vector>
#include <memory>
#include <GLES3/gl3.h>
#include <glm/glm.hpp>
#include "Shader.h"
#include "QuadNode.h"   // NodeInfo, ItemInfo 타입 포함

namespace Render {

    // 축정렬 사각형(AABB) 윤곽선을 깊이 레벨별 색상으로 렌더링한다.
    // 쿼드트리 노드 MBR과 개별 객체 MBR 시각화 양쪽에 사용된다.
    //
    // 정점 레이아웃: [x, y, r, g, b, a]  (6개 float, 2D 위치)
    class BoxRenderer {
    private:
        GLuint  m_vao = 0;          // Vertex Array Object
        GLuint  m_vbo = 0;          // Vertex Buffer Object
        GLint   m_locMVP = -1;      // 셰이더 MVP 유니폼 위치
        GLsizei m_vertexCount = 0;  // 업로드된 정점 수
        std::shared_ptr<Shader> m_shader; // 컴파일된 셰이더

    public:
        ~BoxRenderer();

        // GL 컨텍스트가 활성화된 상태에서 호출 (VAO/VBO/셰이더 초기화)
        void Initialize();

        // 쿼드트리 노드 MBR들을 GPU에 업로드 (깊이별 색상 적용)
        void UploadNodes(const std::vector<Spatial::NodeInfo>& nodes);

        // 객체 MBR들을 GPU에 업로드 (깊이별 색상 적용)
        void UploadItems(const std::vector<Spatial::ItemInfo>& items);

        // GPU 버퍼를 비운다 (토글 OFF 시 호출)
        void Clear();

        // 업로드된 박스 윤곽선들을 렌더링
        void Draw(const glm::mat4& vpMatrix);

        // 업로드된 데이터가 없으면 true
        bool IsEmpty() const { return m_vertexCount == 0; }

    private:
        // CPU에서 만든 정점 배열을 GPU 버퍼에 올리는 내부 헬퍼
        // data 형식: [x, y, r, g, b, a] x N
        void Upload(const std::vector<float>& data);
    };
}
