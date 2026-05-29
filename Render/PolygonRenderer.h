#pragma once
#include <vector>
#include <memory>
#include <GLES3/gl3.h>
#include <glm/glm.hpp>
#include "Shader.h"
#include "PolygonShape.h"
#include "QuadNode.h"   // ItemInfo 타입 포함

namespace Render {

    // 폴리곤 외곽선(선분)을 렌더링한다.
    // 각 폴리곤의 파트(part)별로 인접한 점을 선분으로 연결해 GPU에 업로드한다.
    //
    // 정점 레이아웃: [x, y, r, g, b, a]  (6개 float, 2D 위치)
    class PolygonRenderer {
    private:
        GLuint  m_vao = 0;          // Vertex Array Object
        GLuint  m_vbo = 0;          // Vertex Buffer Object
        GLint   m_locMVP = -1;      // 셰이더 MVP 유니폼 위치
        GLsizei m_vertexCount = 0;  // 업로드된 정점 수
        std::shared_ptr<Shader> m_shader; // 컴파일된 셰이더

    public:
        PolygonRenderer() = default;
        ~PolygonRenderer();

        // GL 컨텍스트가 활성화된 상태에서 호출 (VAO/VBO/셰이더 초기화)
        void Initialize();

        // 가시 객체 리스트를 GPU에 업로드한다.
        //   useLevelColors=true  : 쿼드트리 깊이별 색상 (레벨 컬러)
        //   useLevelColors=false : 단일 회청색 (플랫 컬러)
        void UploadData(const std::vector<Spatial::ItemInfo>& items, bool useLevelColors = true);

        // 업로드된 폴리곤을 GL_LINES로 렌더링
        void Draw(const glm::mat4& vpMatrix);
    };
}
