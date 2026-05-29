#pragma once
#include <memory>
#include <GLES3/gl3.h>
#include <glm/glm.hpp>
#include "Shader.h"

namespace Render {

    // 카메라 프러스텀을 3D 컬러 선분으로 렌더링한다.
    //
    // 그려지는 지오메트리:
    //   눈->코너 광선 4개 : eye에서 각 지면 코너로 향하는 선 (노란색)
    //   지면 풋프린트   4개 : 코너들을 순서대로 연결하는 테두리 (청록색)
    //
    // 정점 레이아웃: [x, y, z, r, g, b, a]  (7개 float, 3D 위치)
    //   aPos(location=0)   : vec3 -- 3D 위치 (2D 렌더러와 달리 Z 포함)
    //   aColor(location=1) : vec4 -- RGBA 색상
    class FrustumRenderer {
    private:
        GLuint  m_vao = 0, m_vbo = 0; // Vertex Array Object / Vertex Buffer Object
        GLint   m_locMVP = -1;         // 셰이더 MVP 유니폼 위치
        GLsizei m_vertexCount = 0;     // 업로드된 정점 수 (0이면 비어있음)
        std::shared_ptr<Shader> m_shader; // 컴파일된 셰이더 프로그램

    public:
        ~FrustumRenderer();

        // GL 컨텍스트가 활성화된 상태에서 호출 (VAO/VBO/셰이더 생성)
        void Initialize();

        // GPU 데이터를 비운다 (토글 OFF 시)
        void Clear() { m_vertexCount = 0; }

        // 현재 업로드된 프러스텀을 화면에 렌더링한다.
        //   vp : 뷰-프로젝션 행렬
        void Draw(const glm::mat4& vp);

        // 프러스텀 지오메트리를 GPU에 업로드한다.
        //   eye     : 월드 공간 카메라 눈 위치
        //   corners : 화면 4 코너의 지면(Z=0) 투영점 [BL, BR, TR, TL]
        //             수평선 너머 코너는 최대 가시거리까지 연장된 점
        void Upload(const glm::vec3& eye, const glm::vec3 corners[4]);
    };

} // namespace Render
