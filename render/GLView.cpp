#include "pch.h"
#include "GLView.h"
#include "../MainFrm.h"
#include "../ShpViewerView.h"
#include <windows.h>
#include <mapbox/earcut.hpp>

namespace mapbox {
    namespace util {
        template <>
        struct nth<0, Vec3> {
            static float get(const Vec3& p) { return p.x; }
        };

        template <>
        struct nth<1, Vec3> {
            static float get(const Vec3& p) { return p.z; }
        };
} }

namespace {
    float ComputeRingArea(const std::vector<Vec3>& ring) {
        float area = 0.0f;
        size_t n = ring.size();
        Vec3 origin = ring[0];   // 정밀도 확보용 기준점 — 결과인 넓이 값 자체엔 영향 없음
        for (size_t i = 0; i < n; ++i) {
            size_t j = (i + 1) % n;
            float xi = ring[i].x - origin.x;
            float zi = ring[i].z - origin.z;
            float xj = ring[j].x - origin.x;
            float zj = ring[j].z - origin.z;
            area += xi * zj - xj * zi;
        }
        return area * 0.5f;
    }
}

IMPLEMENT_DYNAMIC(CGLView, CWnd)

BEGIN_MESSAGE_MAP(CGLView, CWnd)
	ON_WM_PAINT()
	ON_WM_SIZE()
	ON_WM_ERASEBKGND()
    ON_WM_LBUTTONDOWN()
    ON_WM_LBUTTONUP()
    ON_WM_RBUTTONDOWN()
    ON_WM_RBUTTONUP()
    ON_WM_MOUSEMOVE()
    ON_WM_MOUSEWHEEL()
    ON_WM_CONTEXTMENU()
END_MESSAGE_MAP()

CGLView::CGLView() : m_camera(Vec3(0.0f, 0.0f, 0.0f), 10.0f, 0.0f, 0.5f) {}
CGLView::~CGLView() { Cleanup(); }

namespace {
    constexpr float kCameraFovRadians = 0.7853982f;
    constexpr float kCameraNearPlane = 1.0f;
    constexpr float kCameraFarPlane = 50000.0f;
    constexpr float kZoomFactor = 0.95f;
    constexpr float kMaxDrawDistance = 4000.0f;
    constexpr float kMaxDrawDistanceSquared = kMaxDrawDistance * kMaxDrawDistance;
    constexpr float kNodeMinSizeToDistanceRatio = 0.014f;     // 노드용 
    constexpr float kNodeMinSizeToDistanceRatioSquared = kNodeMinSizeToDistanceRatio * kNodeMinSizeToDistanceRatio;
    constexpr float kObjectMinSizeToDistanceRatio = 0.02f;    // 객체용
    constexpr float kObjectMinSizeToDistanceRatioSquared = kObjectMinSizeToDistanceRatio * kObjectMinSizeToDistanceRatio;
    constexpr float kLevelColors[14][4] = {
       {1.00f, 0.24f, 0.24f, 1.0f},  // depth 0
       {1.00f, 0.59f, 0.00f, 1.0f},  // depth 1
       {1.00f, 0.90f, 0.00f, 1.0f},  // depth 2
       {0.59f, 1.00f, 0.00f, 1.0f},  // depth 3
       {0.00f, 0.86f, 0.31f, 1.0f},  // depth 4
       {0.00f, 0.86f, 0.78f, 1.0f},  // depth 5
       {0.00f, 0.63f, 1.00f, 1.0f},  // depth 6
       {0.24f, 0.31f, 1.00f, 1.0f},  // depth 7
       {0.67f, 0.24f, 1.00f, 1.0f},  // depth 8
       {1.00f, 0.24f, 0.78f, 1.0f},  // depth 9
       {0.55f, 0.10f, 0.10f, 1.0f},  // depth 10 (dark red)
       {0.55f, 0.30f, 0.05f, 1.0f},  // depth 11 (dark orange/brown)
       {0.05f, 0.40f, 0.15f, 1.0f},  // depth 12 (dark green) - 가장 많은 객체가 몰리는 depth
       {0.05f, 0.15f, 0.55f, 1.0f},  // depth 13 (dark navy)
    };
    constexpr int32_t kLevelColorCount = 14;
    constexpr float kPlaceholderBuildingHeight = 10.0f;
    constexpr float kWallBottomShade = 0.6f;

    struct ExtrudeVertex {
        Vec3 position;
        float shade = 1.0f;
    };

    GLuint CompileShader(GLenum type, const char* source) {
        GLuint shader = glCreateShader(type);
        glShaderSource(shader, 1, &source, nullptr);
        glCompileShader(shader);

        GLint compiled = GL_FALSE;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
        if (!compiled) {
            char log[512];
            glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
            OutputDebugStringA(log);
            glDeleteShader(shader);
            return 0;
        }
        return shader;
    }
}

BOOL CGLView::InitEGL()
{
    HDC hdc = ::GetDC(GetSafeHwnd());

    m_eglDisplay = eglGetDisplay(hdc);
    ::ReleaseDC(GetSafeHwnd(), hdc);
    if (m_eglDisplay == EGL_NO_DISPLAY) return FALSE;

    if (!eglInitialize(m_eglDisplay, nullptr, nullptr)) return FALSE;

    EGLint configAttribs[] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
        EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 8, EGL_DEPTH_SIZE, 24,
        EGL_NONE
    };
    EGLConfig config;
    EGLint numConfigs;
    if (!eglChooseConfig(m_eglDisplay, configAttribs, &config, 1, &numConfigs) || numConfigs == 0)
        return FALSE;

    m_eglSurface = eglCreateWindowSurface(m_eglDisplay, config, GetSafeHwnd(), nullptr);
    if (m_eglSurface == EGL_NO_SURFACE) return FALSE;

    EGLint contextAttribs[] = { EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE };
    m_eglContext = eglCreateContext(m_eglDisplay, config, EGL_NO_CONTEXT, contextAttribs);
    if (m_eglContext == EGL_NO_CONTEXT) return FALSE;

    if (!eglMakeCurrent(m_eglDisplay, m_eglSurface, m_eglSurface, m_eglContext))
        return FALSE;

    if (!InitShader()) return FALSE;
    glGenBuffers(1, &m_vertexBuffer);
    glEnable(GL_DEPTH_TEST);
    glGenBuffers(1, &m_fillVertexBuffer);
    glGenBuffers(1, &m_fillIndexBuffer);
    glGenBuffers(1, &m_nodeBoxVertexBuffer);
    glGenBuffers(1, &m_frustumVertexBuffer);
    glGenBuffers(1, &m_objectBoxVertexBuffer);
    glGenBuffers(1, &m_extrudeVertexBuffer);
    glGenBuffers(1, &m_extrudeIndexBuffer);
    glGenBuffers(1, &m_edgeVertexBuffer);
    return TRUE;
}

bool CGLView::InitShader() {
    const char* vertex_source = R"(#version 300 es
layout(location = 0) in vec3 a_position;
layout(location = 1) in float a_shade;
uniform mat4 u_mvp;
out float v_shade;
void main() {
    gl_Position = u_mvp * vec4(a_position, 1.0);
    v_shade = a_shade;
}
)";

    const char* fragment_source = R"(#version 300 es
precision mediump float;
uniform vec4 u_color;
in float v_shade;
out vec4 frag_color;
void main() {
    frag_color = vec4(u_color.rgb * v_shade, u_color.a);
}
)";

    GLuint vertex_shader = CompileShader(GL_VERTEX_SHADER, vertex_source);
    GLuint fragment_shader = CompileShader(GL_FRAGMENT_SHADER, fragment_source);
    if (vertex_shader == 0 || fragment_shader == 0) return false;

    m_shaderProgram = glCreateProgram();
    glAttachShader(m_shaderProgram, vertex_shader);
    glAttachShader(m_shaderProgram, fragment_shader);
    glLinkProgram(m_shaderProgram);

    GLint linked = GL_FALSE;
    glGetProgramiv(m_shaderProgram, GL_LINK_STATUS, &linked);
    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);

    return linked == GL_TRUE;
}

void CGLView::Render()
{
    // ── 1. 화면 지우기 ──
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // ── 2. 성능 측정 시작 ──
    LARGE_INTEGER freq, start, end;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&start);

    LARGE_INTEGER freq_for_fps, now;
    QueryPerformanceFrequency(&freq_for_fps);
    QueryPerformanceCounter(&now);

    if (m_lastFrameTimestamp.QuadPart != 0) {
        double frame_seconds = static_cast<double>(now.QuadPart - m_lastFrameTimestamp.QuadPart)
            / static_cast<double>(freq_for_fps.QuadPart);
        m_fpsAccumulatedSeconds += frame_seconds;
        ++m_fpsFrameCount;

        if (m_fpsAccumulatedSeconds >= 1.0) {
            m_fps = static_cast<float>(m_fpsFrameCount / m_fpsAccumulatedSeconds);
            m_fpsFrameCount = 0;
            m_fpsAccumulatedSeconds = 0.0;
        }
    }
    m_lastFrameTimestamp = now;

    int32_t visible_count = 0;

    if (m_shaderProgram != 0 && !m_drawRanges.empty()) {
        glUseProgram(m_shaderProgram);

        // ── 3. 카메라/투영 행렬 계산 (MVP) ──
        float aspect = (m_clientHeight != 0)
            ? static_cast<float>(m_clientWidth) / static_cast<float>(m_clientHeight)
            : 1.0f;
        Mat4 view = m_camera.GetViewMatrix();
        Mat4 proj = Mat4Perspective(kCameraFovRadians, aspect, kCameraNearPlane, kCameraFarPlane);
        Mat4 mvp = proj * view;

        GLint mvp_loc = glGetUniformLocation(m_shaderProgram, "u_mvp");
        glUniformMatrix4fv(mvp_loc, 1, GL_FALSE, mvp.m);
        GLint color_loc = glGetUniformLocation(m_shaderProgram, "u_color");
        glUniform4f(color_loc, 1.0f, 1.0f, 1.0f, 1.0f);  // 기본 색: 흰색

        glDisableVertexAttribArray(1);
        glVertexAttrib1f(1, 1.0f);

        // ── 4. 윤곽선(line loop)용 버퍼 bind ──
        glBindBuffer(GL_ARRAY_BUFFER, m_vertexBuffer);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vec3), (void*)0);

        // ── 5. 프러스텀 평면 추출 ──
        std::array<Plane, 6> planes = ExtractFrustumPlane(mvp);

        std::vector<int32_t> candidate_indices;
        std::vector<int32_t> candidate_depths;
        std::vector<int32_t> object_box_depths;
        std::vector<NodeDebugInfo> visible_nodes;
        std::vector<int32_t> filled_candidates;

        // ── 6. 쿼드트리 broad-phase 쿼리 (컬링 1단계) ──
        QueryVisibleObjects(
            m_pDataset->quad_tree.get(),
            planes,
            m_camera.GetEye(),
            kMaxDrawDistanceSquared,
            kNodeMinSizeToDistanceRatioSquared,
            &candidate_indices,
            (m_showAllObjectLevelColors || m_showObjectBounds) ? &candidate_depths : nullptr,
            (m_showQuadTreeLevels && !m_showAllNodes) ? &visible_nodes : nullptr);

        if (m_showAllNodes) {
            CollectAllQuadTreeNodes(m_pDataset->quad_tree.get(), planes, 0, &visible_nodes);
        }

        int32_t candidate_count = static_cast<int32_t>(candidate_indices.size());
        int32_t draw_call_count = 0;

        // ── 7. 객체별 narrow-phase 컬링 + LOD 판정 + 윤곽선 그리기 (컬링 2단계) ──
        for (size_t i = 0; i < candidate_indices.size(); ++i) {
            int32_t candidate_index = candidate_indices[i];
            const RecordRange& record_range = m_recordRanges[candidate_index];

            if (!IsBoxInsideFrustum(planes, record_range.bounds_min, record_range.bounds_max)) {
                continue;  // 프러스텀 재검사 탈락
            }

            Vec3 object_center = (record_range.bounds_min + record_range.bounds_max) * 0.5f;
            float distance_sq = Vec3LengthSquared(object_center - m_camera.GetEye());

            if (distance_sq > kMaxDrawDistanceSquared) {
                continue;  // draw distance 컷오프
            }

            float width = record_range.bounds_max.x - record_range.bounds_min.x;
            float depth = record_range.bounds_max.z - record_range.bounds_min.z;
            float object_size_sq = width * depth;

            if (object_size_sq < kObjectMinSizeToDistanceRatioSquared * distance_sq) {
                continue;  // 객체 size/distance LOD 탈락
            }

            ++visible_count;

            // (디버그) 쿼드트리 깊이별 색상
            if (m_showAllObjectLevelColors && i < candidate_depths.size()) {
                int32_t node_depth = candidate_depths[i];
                int32_t color_index = (node_depth < kLevelColorCount) ? node_depth : (kLevelColorCount - 1);
                glUniform4fv(color_loc, 1, kLevelColors[color_index]);
            }

            // 윤곽선 그리기
            int32_t end_index = record_range.first_range_index + record_range.range_count;
            for (int32_t k = record_range.first_range_index; k < end_index; ++k) {
                const DrawRange& range = m_drawRanges[k];
                glDrawArrays(GL_LINE_LOOP, range.first, range.count);
                ++draw_call_count;
            }

            filled_candidates.push_back(candidate_index);  // 채우기 대상으로 기록
            object_box_depths.push_back((i < candidate_depths.size()) ? candidate_depths[i] : 0);

            if (m_showAllObjectLevelColors) {
                glUniform4f(color_loc, 1.0f, 1.0f, 1.0f, 1.0f);  // 다음 객체를 위해 흰색으로 복구
            }
        }

        // ── 8. 채우기(fill) 그리기 — 
        if (m_showFill && !filled_candidates.empty()) {
            glDisableVertexAttribArray(1);
            glVertexAttrib1f(1, 1.0f);
            glBindBuffer(GL_ARRAY_BUFFER, m_fillVertexBuffer);
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vec3), (void*)0);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_fillIndexBuffer);

            for (size_t j = 0; j < filled_candidates.size(); ++j) {
                int32_t idx = filled_candidates[j];

                if (m_showAllObjectLevelColors) {
                    int32_t node_depth = object_box_depths[j];
                    int32_t color_index = (node_depth < kLevelColorCount) ? node_depth : (kLevelColorCount - 1);
                    glUniform4fv(color_loc, 1, kLevelColors[color_index]);
                }

                const FillRange& fill_range = m_fillRanges[idx];
                glDrawElements(GL_TRIANGLES, fill_range.index_count, GL_UNSIGNED_INT,
                    (void*)(fill_range.first_index * sizeof(uint32_t)));
            }

            if (m_showAllObjectLevelColors) {
                glUniform4f(color_loc, 1.0f, 1.0f, 1.0f, 1.0f);  // 다음 객체를 위해 흰색으로 복구
            }
        }

        if (m_show3D && !filled_candidates.empty()) {
            glEnable(GL_POLYGON_OFFSET_FILL);   
            glPolygonOffset(1.0f, 1.0f);
            glBindBuffer(GL_ARRAY_BUFFER, m_extrudeVertexBuffer);
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(ExtrudeVertex), (void*)offsetof(ExtrudeVertex, position));
            glEnableVertexAttribArray(1);
            glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, sizeof(ExtrudeVertex), (void*)offsetof(ExtrudeVertex, shade));
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_extrudeIndexBuffer);

            for (size_t j = 0; j < filled_candidates.size(); ++j) {
                int32_t idx = filled_candidates[j];

                if (m_showAllObjectLevelColors) {
                    int32_t node_depth = object_box_depths[j];
                    int32_t color_index = (node_depth < kLevelColorCount) ? node_depth : (kLevelColorCount - 1);
                    glUniform4fv(color_loc, 1, kLevelColors[color_index]);
                }

                const ExtrudeRange& extrude_range = m_extrudeRanges[idx];
                glDrawElements(GL_TRIANGLES, extrude_range.index_count, GL_UNSIGNED_INT, (void*)(extrude_range.first_index * sizeof(uint32_t)));
            }

            if (m_showAllObjectLevelColors) {
                glUniform4f(color_loc, 1.0f, 1.0f, 1.0f, 1.0f);  
            }
            glDisable(GL_POLYGON_OFFSET_FILL);
        }

        if (m_showEdges && !filled_candidates.empty()) {
            glDisableVertexAttribArray(1);
            glVertexAttrib1f(1, 1.0f);
            glBindBuffer(GL_ARRAY_BUFFER, m_edgeVertexBuffer);
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vec3), (void*)0);

            glUniform4f(color_loc, 0.0f, 0.0f, 0.0f, 1.0f);

            for (int32_t idx : filled_candidates) {
                const EdgeRange& edge_range = m_edgeRanges[idx];
                glDrawArrays(GL_LINES, edge_range.first_vertex, edge_range.vertex_count);
            }
        }


        if (m_showObjectBounds && !filled_candidates.empty()) {
            RenderObjectBounds(filled_candidates, object_box_depths);
        }

        // ── 9. 디버그: 쿼드트리 레벨 박스 시각화 ──
        if (m_showQuadTreeLevels && !visible_nodes.empty()) {
            RenderQuadTreeLevels(visible_nodes);
        }

        // ── 10. 성능 측정 종료 + 로그 출력 ──
        QueryPerformanceCounter(&end);
        float elapsed_ms = static_cast<float>(end.QuadPart - start.QuadPart) * 1000.0f
            / static_cast<float>(freq.QuadPart);

        CString debug_msg;
        debug_msg.Format(_T("candidate_count=%d, visible_count=%d, draw_call_count=%d, elapsed_ms=%.2f\n"),
            candidate_count, visible_count, draw_call_count, elapsed_ms);
        OutputDebugString(debug_msg);
    }

    // ── 11. 인스펙터 패널 갱신 ──
    if (CShpViewerView* view = dynamic_cast<CShpViewerView*>(GetParent())) {
        view->UpdateInspector(visible_count, static_cast<int32_t>(m_recordRanges.size()), m_fps);
    }

    // ── 12. 화면에 표시 ──
    eglSwapBuffers(m_eglDisplay, m_eglSurface);
}

void CGLView::RenderQuadTreeLevels(const std::vector<NodeDebugInfo>& nodes) {
    std::vector<Vec3> box_vertices;
    box_vertices.reserve(nodes.size() * 4);

    for (const NodeDebugInfo& info : nodes) {
        box_vertices.push_back(Vec3(info.bounds.min_x, 0.0f, info.bounds.min_z));
        box_vertices.push_back(Vec3(info.bounds.max_x, 0.0f, info.bounds.min_z));
        box_vertices.push_back(Vec3(info.bounds.max_x, 0.0f, info.bounds.max_z));
        box_vertices.push_back(Vec3(info.bounds.min_x, 0.0f, info.bounds.max_z));
    }

    glBindBuffer(GL_ARRAY_BUFFER, m_nodeBoxVertexBuffer);
    glBufferData(GL_ARRAY_BUFFER, box_vertices.size() * sizeof(Vec3), box_vertices.data(), GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vec3), (void*)0);

    GLint color_loc = glGetUniformLocation(m_shaderProgram, "u_color");

    for (size_t i = 0; i < nodes.size(); ++i) {
        int32_t depth = nodes[i].depth;
        int32_t color_index = (depth < kLevelColorCount) ? depth : (kLevelColorCount - 1);
        glUniform4fv(color_loc, 1, kLevelColors[color_index]);
        glDrawArrays(GL_LINE_LOOP, static_cast<GLint>(i * 4), 4);
    }
}

void CGLView::RenderObjectBounds(const std::vector<int32_t>& visible_indices, const std::vector<int32_t>& depths) {
    std::vector<Vec3> box_vertices;
    box_vertices.reserve(visible_indices.size() * 4);  // 미리 공간 할당

    for (int32_t idx : visible_indices) {
        const RecordRange& record_range = m_recordRanges[idx];
        box_vertices.push_back(Vec3(record_range.bounds_min.x, 0.0f, record_range.bounds_min.z));
        box_vertices.push_back(Vec3(record_range.bounds_max.x, 0.0f, record_range.bounds_min.z));
        box_vertices.push_back(Vec3(record_range.bounds_max.x, 0.0f, record_range.bounds_max.z));
        box_vertices.push_back(Vec3(record_range.bounds_min.x, 0.0f, record_range.bounds_max.z));
    }

    glBindBuffer(GL_ARRAY_BUFFER, m_objectBoxVertexBuffer);
    glBufferData(GL_ARRAY_BUFFER, box_vertices.size() * sizeof(Vec3), box_vertices.data(), GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vec3), (void*)0);

    GLint color_loc = glGetUniformLocation(m_shaderProgram, "u_color");

    for (size_t i = 0; i < visible_indices.size(); ++i) {
        int32_t node_depth = (i < depths.size()) ? depths[i] : 0;
        int32_t color_index = (node_depth < kLevelColorCount) ? node_depth : (kLevelColorCount -1);
        glUniform4fv(color_loc, 1, kLevelColors[color_index]);
        glDrawArrays(GL_LINE_LOOP, static_cast<GLint>(i * 4), 4);
    }

    glUniform4f(color_loc, 1.0f, 1.0f, 1.0f, 1.0f);
}

void CGLView::SetShowQuadTreeLevels(bool show) {
    m_showQuadTreeLevels = show;
    Invalidate();
}
void CGLView::SetShowAllNodes(bool show) {
    m_showAllNodes = show;
    Invalidate();
}
void CGLView::SetShowAllObjectLevelColors(bool show) {
    m_showAllObjectLevelColors = show;
    Invalidate();
}

void CGLView::SetShowFill(bool show) {
    m_showFill = show;
    Invalidate();
}

void CGLView::SetShowObjectBounds(bool show) {
    m_showObjectBounds = show;
    Invalidate();
}

void CGLView::SetShow3D(bool show) {
    m_show3D = show;
    Invalidate();
}

void CGLView::SetShowEdges(bool show) {
    m_showEdges = show;
    Invalidate();
}

void CGLView::SetDataset(const ShpDataset* dataset) {
    m_pDataset = dataset;
    if (m_pDataset) {
        Vec3 center = (m_pDataset->header.world_bbox_min + m_pDataset->header.world_bbox_max) * 0.5f;
        Vec3 extent = m_pDataset->header.world_bbox_max - m_pDataset->header.world_bbox_min;
        float max_extent = (extent.x > extent.z) ? extent.x : extent.z;
        m_camera.Recenter(center, max_extent);
    }
    BuildDebugGeometry();
}

void CGLView::BuildDebugGeometry() {
    m_drawRanges.clear();
    m_recordRanges.clear();
    m_fillRanges.clear();
    if (!m_pDataset) return;

    std::vector<Vec3> vertices;
    std::vector<Vec3> fill_vertices;
    std::vector<uint32_t> fill_indices;
    std::vector<ExtrudeVertex> extrude_vertices;
    std::vector<uint32_t> extrude_indices;
    std::vector<Vec3> edge_vertices;

    int32_t record_count = static_cast<int32_t>(m_pDataset->records.size());

    for (int32_t i = 0; i < record_count; ++i) {
        const ShpPolygonRecord& record = m_pDataset->records[i];
        const float building_height = GetRecordHeight(*m_pDataset, static_cast<size_t>(i), kPlaceholderBuildingHeight);
        int32_t part_count = static_cast<int32_t>(record.part_start_indices.size());

        std::vector<std::vector<Vec3>> all_parts;
        for (int32_t p = 0; p < part_count; ++p) {
            int32_t start = record.part_start_indices[p];
            int32_t end = (p + 1 < part_count)
                ? record.part_start_indices[p + 1]
                : static_cast<int32_t>(record.points.size());
            all_parts.emplace_back(record.points.begin() + start, record.points.begin() + end);
        }

        int32_t outer_part_index = 0;
        float max_area = 0.0f;
        for (int32_t p = 0; p < part_count; ++p) {
            float area = std::fabs(ComputeRingArea(all_parts[p]));
            if (area > max_area) {
                max_area = area;
                outer_part_index = p;
            }
        }

        std::vector<std::vector<Vec3>> rings;
        rings.push_back(all_parts[outer_part_index]);
        for (int32_t p = 0; p < part_count; ++p) {
            if (p != outer_part_index) {
                rings.push_back(all_parts[p]);
            }
        }

        std::vector<uint32_t> tri_indices = mapbox::earcut<uint32_t>(rings);

        FillRange fill_range;
        fill_range.first_index = static_cast<GLint>(fill_indices.size());
        fill_range.index_count = static_cast<GLsizei>(tri_indices.size());

        int32_t vertex_offset = static_cast<int32_t>(fill_vertices.size());
        for (uint32_t idx : tri_indices) {
            fill_indices.push_back(vertex_offset + idx);
        }
        for (const std::vector<Vec3>& ring : rings) {
            for (const Vec3& p : ring) {
                fill_vertices.push_back(p);
            }
        }

        m_fillRanges.push_back(fill_range);

        ExtrudeRange extrude_range;
        extrude_range.first_index = static_cast<GLint>(extrude_indices.size());

        int32_t roof_vertex_offset = static_cast<int32_t>(extrude_vertices.size());
        for (uint32_t idx : tri_indices) {
            extrude_indices.push_back(roof_vertex_offset + idx);
        }
        for (const std::vector<Vec3>& ring : rings) {
            for (const Vec3& p : ring) {
                extrude_vertices.push_back({ Vec3(p.x, building_height, p.z), 1.0f });
            }
        }

        EdgeRange edge_range;
        edge_range.first_vertex = static_cast<GLint>(edge_vertices.size());

        for (const std::vector<Vec3>& ring : rings) {
            int32_t point_count = static_cast<int32_t>(ring.size());
            for (int32_t k = 0; k < point_count; ++k) {
                const Vec3& p0 = ring[k];
                const Vec3& p1 = ring[(k + 1) % point_count];

                int32_t wall_vertex_offset = static_cast<int32_t>(extrude_vertices.size());

                extrude_vertices.push_back({ Vec3(p0.x, 0.0f, p0.z), kWallBottomShade });
                extrude_vertices.push_back({ Vec3(p1.x, 0.0f, p1.z), kWallBottomShade });
                extrude_vertices.push_back({ Vec3(p1.x, building_height, p1.z), 1.0f });
                extrude_vertices.push_back({ Vec3(p0.x, building_height, p0.z), 1.0f });

                extrude_indices.push_back(wall_vertex_offset + 0);
                extrude_indices.push_back(wall_vertex_offset + 1);
                extrude_indices.push_back(wall_vertex_offset + 2);

                extrude_indices.push_back(wall_vertex_offset + 0);
                extrude_indices.push_back(wall_vertex_offset + 2);
                extrude_indices.push_back(wall_vertex_offset + 3);

                edge_vertices.push_back(Vec3(p0.x, building_height, p0.z));
                edge_vertices.push_back(Vec3(p1.x, building_height, p1.z));

                edge_vertices.push_back(Vec3(p0.x, 0.0f, p0.z));
                edge_vertices.push_back(Vec3(p0.x, building_height, p0.z));
            }
        }
        edge_range.vertex_count = static_cast<GLsizei>(edge_vertices.size() - edge_range.first_vertex);
        m_edgeRanges.push_back(edge_range);
        extrude_range.index_count = static_cast<GLsizei>(extrude_indices.size() - extrude_range.first_index);
        m_extrudeRanges.push_back(extrude_range);

        RecordRange record_range;
        record_range.first_range_index = static_cast<int32_t>(m_drawRanges.size());
        record_range.bounds_min = record.bounds_min;
        record_range.bounds_max = record.bounds_max;

        for (int32_t p = 0; p < part_count; ++p) {
            int32_t start = record.part_start_indices[p];
            int32_t end = (p + 1 < part_count)
                ? record.part_start_indices[p + 1]
                : static_cast<int32_t>(record.points.size());

            DrawRange range;
            range.first = static_cast<GLint>(vertices.size());
            range.count = static_cast<GLsizei>(end - start);
            m_drawRanges.push_back(range);

            for (int32_t k = start; k < end; ++k) {
                vertices.push_back(record.points[k]);
            }
        }

        if (part_count > 1) {
            CString debug_msg;
            debug_msg.Format(_T("record %d: part_count=%d\n"), i, part_count);
            OutputDebugString(debug_msg);
            for (int32_t k = 0; k < static_cast<int32_t>(record.points.size()) && k < 5; ++k) {
                CString point_msg;
                point_msg.Format(_T("  point[%d] = (%.2f, %.2f, %.2f)\n"),
                    k, record.points[k].x, record.points[k].y, record.points[k].z);
                OutputDebugString(point_msg);
            }
        }

        record_range.range_count = static_cast<int32_t>(m_drawRanges.size()) - record_range.first_range_index;
        m_recordRanges.push_back(record_range);
    }

    glBindBuffer(GL_ARRAY_BUFFER, m_vertexBuffer);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vec3), vertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ARRAY_BUFFER, m_fillVertexBuffer);
    glBufferData(GL_ARRAY_BUFFER, fill_vertices.size() * sizeof(Vec3), fill_vertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_fillIndexBuffer);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, fill_indices.size() * sizeof(uint32_t), fill_indices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ARRAY_BUFFER, m_extrudeVertexBuffer);
    glBufferData(GL_ARRAY_BUFFER, extrude_vertices.size() * sizeof(ExtrudeVertex), extrude_vertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_extrudeIndexBuffer);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, extrude_indices.size() * sizeof(uint32_t), extrude_indices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ARRAY_BUFFER, m_edgeVertexBuffer);
    glBufferData(GL_ARRAY_BUFFER, edge_vertices.size() * sizeof(Vec3), edge_vertices.data(), GL_STATIC_DRAW);
}

void CGLView::Cleanup()
{
    if (m_eglDisplay != EGL_NO_DISPLAY)
    {
        eglMakeCurrent(m_eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (m_eglContext != EGL_NO_CONTEXT) eglDestroyContext(m_eglDisplay, m_eglContext);
        if (m_eglSurface != EGL_NO_SURFACE) eglDestroySurface(m_eglDisplay, m_eglSurface);
        eglTerminate(m_eglDisplay);
    }
    m_eglDisplay = EGL_NO_DISPLAY;
}

void CGLView::OnPaint()
{
	CPaintDC dc(this); // device context for painting
    Render();
}

void CGLView::OnSize(UINT nType, int cx, int cy)
{
	CWnd::OnSize(nType, cx, cy);

    m_clientWidth = cx;
    m_clientHeight = cy;

    if (m_eglDisplay != EGL_NO_DISPLAY) {
        glViewport(0, 0, cx, cy);
        Render();
    }
}

BOOL CGLView::OnEraseBkgnd(CDC* pDC)
{
	// TODO: 여기에 메시지 처리기 코드를 추가 및/또는 기본값을 호출합니다.

    return TRUE;
}

void CGLView::OnLButtonDown(UINT nFlags, CPoint point)
{
    SetCapture();
    m_lastMousePos = point;
    m_isPanning = true;

    CWnd::OnLButtonDown(nFlags, point);
}

void CGLView::OnLButtonUp(UINT nFlags, CPoint point)
{
    m_isPanning = false;

    ReleaseCapture();
    CWnd::OnLButtonUp(nFlags, point);
}

void CGLView::OnRButtonDown(UINT nFlags, CPoint point)
{
    SetCapture();
    m_lastMousePos = point;
    m_isRotating = true;

    CWnd::OnRButtonDown(nFlags, point);
}

void CGLView::OnRButtonUp(UINT nFlags, CPoint point)
{
    m_isRotating = false;

    ReleaseCapture();
    CWnd::OnRButtonUp(nFlags, point);
}

void CGLView::OnMouseMove(UINT nFlags, CPoint point)
{
    float delta_x = static_cast<float>(m_lastMousePos.x - point.x);
    float delta_y = static_cast<float>(m_lastMousePos.y - point.y);
    
    if (m_isPanning) {
    
        m_camera.Pan(delta_x, -delta_y);
        m_lastMousePos = point;
        Invalidate();
    }
    else if (m_isRotating) {
        m_camera.Rotate(delta_x * 0.005f, -delta_y * 0.005f);
        m_lastMousePos = point;
        Invalidate();
    }

    CWnd::OnMouseMove(nFlags, point);
}


BOOL CGLView::OnMouseWheel(UINT nFlags, short zDelta, CPoint pt)
{
    float notches = static_cast<float>(zDelta) / WHEEL_DELTA;
    float scale = std::pow(kZoomFactor, notches);
    m_camera.Zoom(scale);
    Invalidate();

    return CWnd::OnMouseWheel(nFlags, zDelta, pt);
}

void CGLView::OnContextMenu(CWnd* /*pWnd*/, CPoint /*point*/)
{
    // 기본 컨텍스트 메뉴가 뜨지 않도록 비워둠
}
