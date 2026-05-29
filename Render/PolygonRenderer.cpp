#include "pch.h"
#include "PolygonRenderer.h"
#include "LevelColors.h"
#include "PolygonShape.h"

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
    PolygonRenderer::~PolygonRenderer() {
        if (m_vao) glDeleteVertexArrays(1, &m_vao);
        if (m_vbo) glDeleteBuffers(1, &m_vbo);
    }

    // VAO/VBO 생성 및 셰이더 컴파일
    void PolygonRenderer::Initialize() {
        glGenVertexArrays(1, &m_vao);
        glGenBuffers(1, &m_vbo);
        m_shader = std::make_shared<Shader>(s_vert, s_frag);
        m_locMVP = glGetUniformLocation(m_shader->GetID(), "u_MVP");
    }

    // 가시 객체 리스트를 CPU에서 선분 정점 배열로 변환 후 GPU에 업로드한다.
    //
    // 각 PolygonShape는 parts[] 배열로 여러 파트(링)를 가진다.
    // 각 파트 내에서 j → j+1 → ... → j_end-1 → j(시작) 로 닫힌 선분을 구성한다.
    // 선분 하나당 정점 2개, 정점 하나는 [x, y, r, g, b, a] 6 float.
    void PolygonRenderer::UploadData(const std::vector<Spatial::ItemInfo>& items, bool useLevelColors) {
        std::vector<float> gpuData;
        gpuData.reserve(items.size() * 32 * 6); // 대략적 예약 (객체당 32선분 추정)

        static const glm::vec4 flatColor(0.65f, 0.80f, 1.0f, 1.0f); // 플랫 모드 단일 회청색

        for (const auto& info : items) {
            // PolygonShape로 다운캐스트 (폴리곤이 아닌 객체는 건너뜀)
            auto poly = std::dynamic_pointer_cast<GeoData::PolygonShape>(info.item);
            if (!poly) continue;

            // 레벨 색상 vs 플랫 색상 선택
            glm::vec4 col = useLevelColors ? GetLevelColor(info.depth) : flatColor;

            // 각 파트(링)를 순회하며 선분 정점 생성
            for (size_t i = 0; i < poly->parts.size(); ++i) {
                int start = poly->parts[i];
                int end   = (i + 1 < poly->parts.size())
                              ? poly->parts[i + 1]
                              : (int)poly->points.size(); // 마지막 파트는 points 끝까지

                for (int j = start; j < end; ++j) {
                    int next = (j + 1 < end) ? j + 1 : start; // 마지막 점은 시작으로 연결(닫힘)

                    // 현재 점 (선분 시작)
                    gpuData.push_back((float)poly->points[j].x);
                    gpuData.push_back((float)poly->points[j].y);
                    gpuData.push_back(col.r); gpuData.push_back(col.g);
                    gpuData.push_back(col.b); gpuData.push_back(col.a);

                    // 다음 점 (선분 끝)
                    gpuData.push_back((float)poly->points[next].x);
                    gpuData.push_back((float)poly->points[next].y);
                    gpuData.push_back(col.r); gpuData.push_back(col.g);
                    gpuData.push_back(col.b); gpuData.push_back(col.a);
                }
            }
        }

        m_vertexCount = (GLsizei)(gpuData.size() / 6); // 정점 수 계산

        // GPU 버퍼 업로드 + 속성 포인터 설정
        glBindVertexArray(m_vao);
        glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
        glBufferData(GL_ARRAY_BUFFER, gpuData.size() * sizeof(float), gpuData.data(), GL_DYNAMIC_DRAW);

        // aPos: location=0, 2 float
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);

        // aColor: location=1, 4 float
        glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(2 * sizeof(float)));
        glEnableVertexAttribArray(1);

        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);
    }

    // 업로드된 폴리곤 선분들을 GL_LINES로 렌더링
    void PolygonRenderer::Draw(const glm::mat4& vpMatrix) {
        if (m_vertexCount == 0 || !m_shader) return;
        glUseProgram(m_shader->GetID());
        glUniformMatrix4fv(m_locMVP, 1, GL_FALSE, &vpMatrix[0][0]);
        glBindVertexArray(m_vao);
        glDrawArrays(GL_LINES, 0, m_vertexCount);
        glBindVertexArray(0);
        glUseProgram(0);
    }
}
