#include "pch.h"
#include "BoxRenderer.h"
#include "LevelColors.h"

namespace Render {

    // 정점 셰이더: 2D 위치를 VP 행렬로 변환 (Z=0 고정)
    static const char* s_vert =
        "#version 300 es\n"
        "layout(location = 0) in vec2 aPos;\n"   // 2D 지면 좌표
        "layout(location = 1) in vec4 aColor;\n" // RGBA 색상
        "uniform mat4 u_MVP;\n"
        "out vec4 v_Color;\n"
        "void main() {\n"
        "   gl_Position = u_MVP * vec4(aPos, 0.0, 1.0);\n" // Z=0 지면
        "   v_Color = aColor;\n"
        "}\n";

    // 프래그먼트 셰이더: 정점 색상 그대로 출력
    static const char* s_frag =
        "#version 300 es\n"
        "precision mediump float;\n"
        "in vec4 v_Color;\n"
        "out vec4 FragColor;\n"
        "void main() { FragColor = v_Color; }\n";

    // 소멸자: GPU 리소스 해제
    BoxRenderer::~BoxRenderer() {
        if (m_vao) glDeleteVertexArrays(1, &m_vao);
        if (m_vbo) glDeleteBuffers(1, &m_vbo);
    }

    // VAO/VBO 생성 및 셰이더 컴파일
    void BoxRenderer::Initialize() {
        glGenVertexArrays(1, &m_vao);
        glGenBuffers(1, &m_vbo);
        m_shader = std::make_shared<Shader>(s_vert, s_frag);
        m_locMVP = glGetUniformLocation(m_shader->GetID(), "u_MVP");
    }

    // CPU 정점 배열을 GPU 버퍼에 업로드 + 속성 포인터 설정
    void BoxRenderer::Upload(const std::vector<float>& data) {
        if (data.empty()) { m_vertexCount = 0; return; }
        m_vertexCount = (GLsizei)(data.size() / 6); // 정점 수 = 총 float 수 / 6

        glBindVertexArray(m_vao);
        glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
        glBufferData(GL_ARRAY_BUFFER, data.size() * sizeof(float), data.data(), GL_DYNAMIC_DRAW);

        // aPos: location=0, 2 float, stride=6, offset=0
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);

        // aColor: location=1, 4 float, stride=6, offset=2*sizeof(float)
        glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(2 * sizeof(float)));
        glEnableVertexAttribArray(1);

        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);
    }

    // 정점 하나를 버퍼에 추가: [x, y, r, g, b, a]
    static void PushVert(std::vector<float>& v, float x, float y, const glm::vec4& c) {
        v.push_back(x); v.push_back(y);
        v.push_back(c.r); v.push_back(c.g); v.push_back(c.b); v.push_back(c.a);
    }

    // AABB 한 개를 4개의 선분(8개 정점)으로 버퍼에 추가
    // 순서: 아래변, 오른변, 위변, 왼변
    static void PushBox(std::vector<float>& v, const GeoData::BoundingBox& b, const glm::vec4& c) {
        float x0 = (float)b.minX, y0 = (float)b.minY;
        float x1 = (float)b.maxX, y1 = (float)b.maxY;

        PushVert(v, x0, y0, c); PushVert(v, x1, y0, c); // 아래변
        PushVert(v, x1, y0, c); PushVert(v, x1, y1, c); // 오른변
        PushVert(v, x1, y1, c); PushVert(v, x0, y1, c); // 위변
        PushVert(v, x0, y1, c); PushVert(v, x0, y0, c); // 왼변
    }

    // 쿼드트리 노드 MBR들을 업로드 (깊이별 색상 + 알파 0.6)
    void BoxRenderer::UploadNodes(const std::vector<Spatial::NodeInfo>& nodes) {
        std::vector<float> data;
        data.reserve(nodes.size() * 8 * 6); // 노드당 8정점, 정점당 6float
        for (const auto& n : nodes) {
            glm::vec4 col = GetLevelColor(n.depth);
            col.a = 0.6f; // 약간 투명하게
            PushBox(data, n.bounds, col);
        }
        Upload(data);
    }

    // 객체 MBR들을 업로드 (깊이별 색상 + 알파 0.5)
    void BoxRenderer::UploadItems(const std::vector<Spatial::ItemInfo>& items) {
        std::vector<float> data;
        data.reserve(items.size() * 8 * 6); // 객체당 8정점, 정점당 6float
        for (const auto& info : items) {
            glm::vec4 col = GetLevelColor(info.depth);
            col.a = 0.5f; // 폴리곤 위에 겹쳐 보이도록 반투명
            PushBox(data, info.item->GetBounds(), col);
        }
        Upload(data);
    }

    // GPU 버퍼를 비움 (정점 수만 0으로 설정)
    void BoxRenderer::Clear() {
        m_vertexCount = 0;
    }

    // 업로드된 박스 윤곽선들을 GL_LINES로 렌더링
    void BoxRenderer::Draw(const glm::mat4& vpMatrix) {
        if (m_vertexCount == 0 || !m_shader) return;
        glUseProgram(m_shader->GetID());
        glUniformMatrix4fv(m_locMVP, 1, GL_FALSE, &vpMatrix[0][0]);
        glBindVertexArray(m_vao);
        glDrawArrays(GL_LINES, 0, m_vertexCount);
        glBindVertexArray(0);
        glUseProgram(0);
    }
}
