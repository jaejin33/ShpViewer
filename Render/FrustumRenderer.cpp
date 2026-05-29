#include "pch.h"
#include "FrustumRenderer.h"
#include <glm/gtc/type_ptr.hpp>

namespace Render {

    // 정점 셰이더: 3D 위치(vec3)와 정점별 색상(vec4) 사용
    // 2D 렌더러(PolygonRenderer, BoxRenderer)와 달리 aPos가 vec3임에 주의
    static const char* s_vert =
        "#version 300 es\n"
        "layout(location = 0) in vec3 aPos;\n"   // 3D 월드 좌표
        "layout(location = 1) in vec4 aColor;\n" // RGBA 색상
        "uniform mat4 u_MVP;\n"                  // 뷰-프로젝션 행렬 (M=단위행렬)
        "out vec4 v_Color;\n"
        "void main() {\n"
        "   gl_Position = u_MVP * vec4(aPos, 1.0);\n"
        "   v_Color = aColor;\n"
        "}\n";

    // 프래그먼트 셰이더: 정점 색상을 그대로 출력
    static const char* s_frag =
        "#version 300 es\n"
        "precision mediump float;\n"
        "in vec4 v_Color;\n"
        "out vec4 FragColor;\n"
        "void main() { FragColor = v_Color; }\n";

    // 소멸자: GPU 리소스 해제
    FrustumRenderer::~FrustumRenderer()
    {
        if (m_vao) glDeleteVertexArrays(1, &m_vao);
        if (m_vbo) glDeleteBuffers(1, &m_vbo);
    }

    // VAO/VBO 생성 및 셰이더 컴파일
    void FrustumRenderer::Initialize()
    {
        glGenVertexArrays(1, &m_vao);
        glGenBuffers(1, &m_vbo);
        m_shader = std::make_shared<Shader>(s_vert, s_frag);
        m_locMVP = glGetUniformLocation(m_shader->GetID(), "u_MVP");
    }

    // 정점 하나를 버퍼에 추가하는 헬퍼 함수
    // 형식: [x, y, z, r, g, b, a] (7 float)
    static void PushVert3(std::vector<float>& v,
                          const glm::vec3& pos,
                          const glm::vec4& col)
    {
        v.push_back(pos.x); v.push_back(pos.y); v.push_back(pos.z); // 3D 위치
        v.push_back(col.r); v.push_back(col.g); v.push_back(col.b); v.push_back(col.a); // RGBA
    }

    // 프러스텀 지오메트리를 GPU에 업로드한다.
    //
    // 선분 구성 (총 8개 선분, 16개 정점):
    //   1. eye -> corners[0..3] : 눈에서 각 코너로 향하는 4개의 광선 (노란색)
    //   2. corners[0->1->2->3->0] : 지면 풋프린트 4변 (청록색)
    void FrustumRenderer::Upload(const glm::vec3& eye, const glm::vec3 corners[4])
    {
        // 8선분 x 2정점 x 7float = 112 float 예약
        std::vector<float> data;
        data.reserve(16 * 7);

        const glm::vec4 yellow(1.0f, 1.0f, 0.0f, 0.8f); // 눈->코너 광선 색상 (노란색)
        const glm::vec4 cyan  (0.0f, 1.0f, 1.0f, 0.9f); // 지면 풋프린트 색상 (청록색)

        // 광선 4개: eye -> 각 코너
        for (int i = 0; i < 4; ++i) {
            PushVert3(data, eye,        yellow); // 시작점: 눈
            PushVert3(data, corners[i], yellow); // 끝점: 지면 코너
        }

        // 풋프린트 4변: BL-BR, BR-TR, TR-TL, TL-BL
        for (int i = 0; i < 4; ++i) {
            PushVert3(data, corners[i],           cyan); // 현재 코너
            PushVert3(data, corners[(i + 1) % 4], cyan); // 다음 코너 (마지막은 첫 코너로)
        }

        m_vertexCount = (GLsizei)(data.size() / 7); // = 16

        // VAO에 정점 데이터 바인딩
        glBindVertexArray(m_vao);
        glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
        glBufferData(GL_ARRAY_BUFFER,
                     data.size() * sizeof(float),
                     data.data(),
                     GL_DYNAMIC_DRAW); // 매 프레임 갱신되므로 DYNAMIC

        // aPos: location=0, 3 float, stride=7*sizeof(float), offset=0
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE,
                              7 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);

        // aColor: location=1, 4 float, stride=7*sizeof(float), offset=3*sizeof(float)
        glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE,
                              7 * sizeof(float), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(1);

        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);
    }

    // 업로드된 프러스텀 선분을 GL_LINES로 렌더링
    void FrustumRenderer::Draw(const glm::mat4& vp)
    {
        if (m_vertexCount == 0 || !m_shader) return;

        glUseProgram(m_shader->GetID());
        glUniformMatrix4fv(m_locMVP, 1, GL_FALSE, glm::value_ptr(vp)); // MVP 전달
        glBindVertexArray(m_vao);
        glDrawArrays(GL_LINES, 0, m_vertexCount); // 16개 정점 = 8개 선분
        glBindVertexArray(0);
        glUseProgram(0);
    }

} // namespace Render
