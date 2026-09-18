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
    //float ComputeRingArea(const std::vector<Vec3>& ring) {
    //    float area = 0.0f;
    //    size_t n = ring.size();
    //    Vec3 origin = ring[0];   // 정밀도 확보용 기준점 — 결과인 넓이 값 자체엔 영향 없음
    //    for (size_t i = 0; i < n; ++i) {
    //        size_t j = (i + 1) % n;
    //        area += ring[i].x * ring[j].z - ring[j].x * ring[i].z;
    //    }
    //    return area * 0.5f;
    //    //for (size_t i = 0; i < n; ++i) {
    //    //    size_t j = (i + 1) % n;
    //    //    float xi = ring[i].x - origin.x;
    //    //    float zi = ring[i].z - origin.z;
    //    //    float xj = ring[j].x - origin.x;
    //    //    float zj = ring[j].z - origin.z;
    //    //    area += xi * zj - xj * zi;
    //    //}
    //    //return area * 0.5f;
    //}
    double ComputeRingArea(const std::vector<Vec3>& ring) {
        double area = 0.0;
        size_t n = ring.size();
        for (size_t i = 0; i < n; ++i) {
            size_t j = (i + 1) % n;
            double xi = static_cast<double>(ring[i].x);
            double zi = static_cast<double>(ring[i].z);
            double xj = static_cast<double>(ring[j].x);
            double zj = static_cast<double>(ring[j].z);
            area += xi * zj - xj * zi;
        }
        return area * 0.5;
    }

    std::vector<std::vector<Vec3>> BuildRecordRings(const ShpPolygonRecord& record, int32_t record_index_for_log)
    {
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
        std::vector<int32_t> hole_part_indices;

        if (part_count > 1) {
            std::vector<int32_t> outer_candidate_indices;
            for (int32_t p = 0; p < part_count; ++p) {
                if (ComputeRingArea(all_parts[p]) > 0.0) {
                    outer_candidate_indices.push_back(p);
                }
            }

            if (outer_candidate_indices.empty()) {
                outer_part_index = 0;
            }
            else {
                outer_part_index = outer_candidate_indices[0];
                if (outer_candidate_indices.size() > 1 && record_index_for_log >= 0) {
                    CString msg;
                    msg.Format(_T("[Record %d] 데이터 오류: 외곽(양수) 파트가 %zu개 있음 — 서로 다른 건물이 반대 방향으로 감겨있을 가능성. part[%d]만 외곽으로 쓰고 나머지는 전부 홀로 처리함\n"),
                        record_index_for_log, outer_candidate_indices.size(), outer_part_index);
                    OutputDebugString(msg);
                }
            }

            // 외곽으로 뽑히지 않은 나머지 파트는 전부 홀로 처리
            for (int32_t p = 0; p < part_count; ++p) {
                if (p == outer_part_index) continue;
                hole_part_indices.push_back(p);
            }
        }

        std::vector<std::vector<Vec3>> rings;
        rings.push_back(all_parts[outer_part_index]);
        for (int32_t hole_index : hole_part_indices) {
            rings.push_back(all_parts[hole_index]);
        }
        return rings;
    }

    // 레이와 지면(y=0) 평면의 교차. 만나면 true와 거리 t를 돌려준다.
    bool IntersectRayGroundPlane(const Vec3& origin, const Vec3& direction, float* out_t) {
        if (std::fabs(direction.y) < 1e-8f) return false;

        const float t = -origin.y / direction.y;
        if (t < 0.0f) return false;

        *out_t = t;
        return true;
    }

    // 레이와 AABB의 교차 - slab 방식
    // 만나면 true와 처음 닿는 거리 t를 돌려준다. 시작점이 상자 안이면 t = 0,
    bool IntersectRayAabb(const Vec3& origin, const Vec3& direction, const Vec3& bounds_min, const Vec3& bounds_max, float* out_t) 
    {
        float t_min = 0.0f;         // 0 = 레이 뒤쪽은 안봄
        float t_max = FLT_MAX;

        const float o[3] = { origin.x, origin.y, origin.z };
        const float d[3] = { direction.x, direction.y, direction.z };
        const float lo[3] = { bounds_min.x, bounds_min.y, bounds_min.z };
        const float hi[3] = { bounds_max.x, bounds_max.y, bounds_max.z };

        for (int axis = 0; axis < 3; ++axis) {
            if (std::fabs(d[axis]) < 1e-8f) {
                // 이 축과 나란히 같다 : 시작점이 슬랩 밖이면 영영 못 들어감
                if (o[axis] < lo[axis] || o[axis] > hi[axis]) return false;
                continue;
            }

            const float inv_d = 1.0f / d[axis];
            float t_enter = (lo[axis] - o[axis]) * inv_d;
            float t_exit = (hi[axis] - o[axis]) * inv_d;
            if (t_enter > t_exit) {     // 음수 방향이면 순서가 뒤집혀서 나옴
                const float tmp = t_enter;
                t_enter = t_exit;
                t_exit = tmp;
            }

            if (t_enter > t_min) t_min = t_enter;
            if (t_exit < t_max) t_max = t_exit;

            if (t_min > t_max) return false;
        }

        *out_t = t_min;
        return true;
    }

    bool IntersectRayTriangle(const Vec3& origin, const Vec3& direction, const Vec3& v0, const Vec3& v1, const Vec3& v2, float* out_t)
    {
        const Vec3 e1 = v1 - v0;
        const Vec3 e2 = v2 - v0;
        const Vec3 normal = Vec3Cross(e1, e2);

        // 1) 평면과의 교점까지의 거리
        const float denominator = Vec3Dot(normal, direction);
        if (std::fabs(denominator) < 1e-8f) {
            return false;   // 레이가 평면과 나란함
        }

        const float t = Vec3Dot(normal, v0 - origin) / denominator;
        if (t < 0.0f) {
            return false;   // 교점이 레이 뒤쪽
        }

        // 2) 그 교점이 삼각형 안인지 - 세 변 모두에 대해 같은 쪽이어야 한다
        const Vec3 point = origin + direction * t;

        if (Vec3Dot(normal, Vec3Cross(v1 - v0, point - v0)) < 0.0f) return false;
        if (Vec3Dot(normal, Vec3Cross(v2 - v1, point - v1)) < 0.0f) return false;
        if (Vec3Dot(normal, Vec3Cross(v0 - v2, point - v2)) < 0.0f) return false;

        *out_t = t;
        return true;
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

CGLView::CGLView() : m_camera(Vec3(0.0f, 0.0f, 0.0f), 10.0f, 0.0f, 1.55f) {}
CGLView::~CGLView() { Cleanup(); }

namespace {
    constexpr float kCameraFovRadians = 0.7853982f;
    constexpr float kCameraNearPlane = 5.0f;
    constexpr float kCameraFarPlane = 100000.0f;
    constexpr float kZoomFactor = 0.95f;
    constexpr float kMaxDrawDistance = 3200.0f;
    constexpr float kMaxDrawDistanceSquared = kMaxDrawDistance * kMaxDrawDistance;
    constexpr float kNodeMinSizeToDistanceRatio = 0.04f;     // 노드용 
    constexpr float kNodeMinSizeToDistanceRatioSquared = kNodeMinSizeToDistanceRatio * kNodeMinSizeToDistanceRatio;
    constexpr float kObjectMinSizeToDistanceRatio = 0.04f;    // 객체용
    constexpr float kObjectMinSizeToDistanceRatioSquared = kObjectMinSizeToDistanceRatio * kObjectMinSizeToDistanceRatio;
    constexpr float kLevelColors[11][4] = {
        {0.60f, 0.26f, 0.38f, 1.0f},  // depth 0
        {0.23f, 0.67f, 0.09f, 1.0f},  // depth 1
        {0.01f, 0.25f, 1.00f, 1.0f},  // depth 2
        {0.16f, 0.61f, 0.80f, 1.0f},  // depth 3
        {0.64f, 0.36f, 1.00f, 1.0f},  // depth 4
        {0.19f, 0.16f, 0.98f, 1.0f},  // depth 5
        {0.00f, 0.51f, 1.00f, 1.0f},  // depth 6  
        {0.18f, 0.82f, 0.75f, 1.0f},  // depth 7
        {0.06f, 0.61f, 0.00f, 1.0f},  // depth 8 
        {0.90f, 0.84f, 0.24f, 1.0f},  // depth 9
        {0.95f, 0.51f, 0.23f, 1.0f},  // depth 10
    };
    constexpr int32_t kLevelColorCount = 11;
    constexpr float kPlaceholderBuildingHeight = 10.0f;
    constexpr float kWallBottomShade = 0.6f;
    constexpr float kFpsUpdateIntervalSeconds = 0.5;
    constexpr float kPickRayDebugLength = 100000.0f; // 디버그용 레이 길이
    constexpr float kPickMarkerScreenScale = 0.005f;
    constexpr float kSelectedColor[4] = { 1.0f, 0.2f, 0.2f, 1.0f };

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

    UpdateProjection();

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
    glGenBuffers(1, &m_fillWireIndexBuffer);
    glGenBuffers(1, &m_pickRayVertexBuffer);
    glGenBuffers(1, &m_pickMarkerVertexBuffer);
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

        if (m_fpsAccumulatedSeconds >= kFpsUpdateIntervalSeconds) {
            m_fps = static_cast<float>(m_fpsFrameCount / m_fpsAccumulatedSeconds);
            m_fpsFrameCount = 0;
            m_fpsAccumulatedSeconds = 0.0;
        }
    }
    m_lastFrameTimestamp = now;

    int32_t visible_count = 0;

    if (m_shaderProgram != 0 && !m_drawRanges.empty()) {
        glUseProgram(m_shaderProgram);

        // ── 3. 카메라/투영 행렬  ──
        Mat4 mvp = m_projMatrix * m_camera.GetViewMatrix();

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

#ifdef ENABLE_CULLING_STATS
        g_cullingStats = CullingStats{};   // 이번 프레임 측정을 위해 초기화
#endif

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

        // ── 7. 객체별 컬링 ──
        for (size_t i = 0; i < candidate_indices.size(); ++i) {
            int32_t candidate_index = candidate_indices[i];
            const RecordRange& record_range = m_recordRanges[candidate_index];

#ifdef ENABLE_CULLING_STATS
            g_cullingStats.narrow_phase_tested++;
#endif

            if (!IsBoxInsideFrustum(planes, record_range.bounds_min, record_range.bounds_max)) {
#ifdef ENABLE_CULLING_STATS
                g_cullingStats.narrow_phase_culled_frustum++;
#endif
                continue;  // 프러스텀 재검사
            }

            Vec3 camera_eye = m_camera.GetEye();
            float center_x = (record_range.bounds_min.x + record_range.bounds_max.x) * 0.5f;
            float center_y = (record_range.bounds_min.y + record_range.bounds_max.y) * 0.5f;
            float center_z = (record_range.bounds_min.z + record_range.bounds_max.z) * 0.5f;
            Vec3 center_point(center_x, center_y, center_z);
            float distance_sq = Vec3LengthSquared(center_point - camera_eye);

            //if (distance_sq > kMaxDrawDistanceSquared) {
            //    continue;  // draw distance 컷
            //}

            float width = record_range.bounds_max.x - record_range.bounds_min.x;
            float depth = record_range.bounds_max.z - record_range.bounds_min.z;
            float height = record_range.bounds_max.y;
            float object_size_sq = width * width + depth * depth + height * height;

            if (object_size_sq < kObjectMinSizeToDistanceRatioSquared * distance_sq) {
#ifdef ENABLE_CULLING_STATS
                g_cullingStats.narrow_phase_culled_size_distance++;
#endif
                continue;  // 객체 size/distance
            }

            ++visible_count;
#ifdef ENABLE_CULLING_STATS
            g_cullingStats.objects_drawn++;
#endif

            // (디버그) 쿼드트리 깊이별 색상
            if (m_showAllObjectLevelColors && i < candidate_depths.size()) {
                int32_t node_depth = candidate_depths[i];
                int32_t color_index = (node_depth < kLevelColorCount) ? node_depth : (kLevelColorCount - 1);
                glUniform4fv(color_loc, 1, kLevelColors[color_index]);
            }

            // 윤곽선 그리기
            if (m_showObjectOutline) {
                int32_t end_index = record_range.first_range_index + record_range.range_count;
                for (int32_t k = record_range.first_range_index; k < end_index; ++k) {
                    const DrawRange& range = m_drawRanges[k];
                    glDrawArrays(GL_LINE_LOOP, range.first, range.count);
                    ++draw_call_count;
                }
            }

            filled_candidates.push_back(candidate_index);  // 채우기 대상으로 기록
            object_box_depths.push_back((i < candidate_depths.size()) ? candidate_depths[i] : 0);

            if (m_showAllObjectLevelColors) {
                glUniform4f(color_loc, 1.0f, 1.0f, 1.0f, 1.0f);  // 다음 객체를 위해 흰색으로 복구
            }
        }
        m_lastVisibleIndices = filled_candidates;
#ifdef ENABLE_CULLING_STATS
        {
            char buf[512];
            sprintf_s(buf,
                "[CullingStats] nodes_visited=%d culled_frustum=%d culled_size_dist=%d | "
                "narrow_tested=%d narrow_culled_frustum=%d narrow_culled_size_dist=%d objects_drawn=%d\n",
                g_cullingStats.nodes_visited, g_cullingStats.nodes_culled_frustum, g_cullingStats.nodes_culled_size_distance,
                g_cullingStats.narrow_phase_tested, g_cullingStats.narrow_phase_culled_frustum,
                g_cullingStats.narrow_phase_culled_size_distance, g_cullingStats.objects_drawn);
            OutputDebugStringA(buf);
        }
#endif

        // ── 8. 채우기(fill) 그리기 — 
        if (m_showFill && !filled_candidates.empty()) {
            glDisableVertexAttribArray(1);
            glVertexAttrib1f(1, 1.0f);
            glBindBuffer(GL_ARRAY_BUFFER, m_fillVertexBuffer);
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vec3), (void*)0);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_fillIndexBuffer);

            for (size_t j = 0; j < filled_candidates.size(); ++j) {
                int32_t idx = filled_candidates[j];
                const bool is_selected = (idx == m_pickedRecordIndex);

                if (is_selected) {
                    glUniform4fv(color_loc, 1, kSelectedColor);
                }
                else if (m_showAllObjectLevelColors) {
                    int32_t node_depth = object_box_depths[j];
                    int32_t color_index = (node_depth < kLevelColorCount) ? node_depth : (kLevelColorCount - 1);
                    glUniform4fv(color_loc, 1, kLevelColors[color_index]);
                }

                const FillRange& fill_range = m_fillRanges[idx];
                glDrawElements(GL_TRIANGLES, fill_range.index_count, GL_UNSIGNED_INT,
                    (void*)(fill_range.first_index * sizeof(uint32_t)));

                if (is_selected) {
                    glUniform4f(color_loc, 1.0f, 1.0f, 1.0f, 1.0f);  // 다음 객체를 위해 복구
                }
            }

            if (m_showAllObjectLevelColors) {
                glUniform4f(color_loc, 1.0f, 1.0f, 1.0f, 1.0f);  // 다음 객체를 위해 흰색으로 복구
            }
        }

        if (m_showTriangulationLines && !filled_candidates.empty()) {
            glDisableVertexAttribArray(1);
            glVertexAttrib1f(1, 1.0f);
            glBindBuffer(GL_ARRAY_BUFFER, m_fillVertexBuffer);
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vec3), (void*)0);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_fillWireIndexBuffer);

            for (size_t j = 0; j < filled_candidates.size(); ++j) {
                int32_t idx = filled_candidates[j];
                const FillRange& wire_range = m_fillWireRanges[idx];
                glDrawElements(GL_LINES, wire_range.index_count, GL_UNSIGNED_INT,
                    (void*)(wire_range.first_index * sizeof(uint32_t)));
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
                const bool is_selected = (idx == m_pickedRecordIndex);

                if (is_selected) {
                    glUniform4fv(color_loc, 1, kSelectedColor);
                }
                else if (m_showAllObjectLevelColors) {
                    int32_t node_depth = object_box_depths[j];
                    int32_t color_index = (node_depth < kLevelColorCount) ? node_depth : (kLevelColorCount - 1);
                    glUniform4fv(color_loc, 1, kLevelColors[color_index]);
                }

                const ExtrudeRange& extrude_range = m_extrudeRanges[idx];
                glDrawElements(GL_TRIANGLES, extrude_range.index_count, GL_UNSIGNED_INT, (void*)(extrude_range.first_index * sizeof(uint32_t)));

                if (is_selected) {
                    glUniform4f(color_loc, 1.0f, 1.0f, 1.0f, 1.0f);  // 다음 객체를 위해 복구
                }
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

            glUniform4f(color_loc, 0.45f, 0.45f, 0.45f, 1.0f);

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

        // picking
        if (m_hasPickRay) {
            glDisableVertexAttribArray(1);
            glVertexAttrib1f(1, 1.0f);
            if (m_showPickRay) {
                RenderPickRay();
            }
            if (m_hasPickHit && m_isPickMarkerVisible) {
                RenderPickMarker();
            }
        }

        // ── 10. 성능 측정 종료 + 로그 출력 ──
        QueryPerformanceCounter(&end);
        float elapsed_ms = static_cast<float>(end.QuadPart - start.QuadPart) * 1000.0f
            / static_cast<float>(freq.QuadPart);

        CString debug_msg;
#ifdef ENABLE_CULLING_STATS
        Vec3 cam_target = m_camera.GetTarget();
        debug_msg.Format(_T("candidate_count=%d, visible_count=%d, draw_call_count=%d, elapsed_ms=%.2f | camera target=(%.4f, %.4f, %.4f) distance=%.4f yaw=%.6f pitch=%.6f\n"),
            candidate_count, visible_count, draw_call_count, elapsed_ms,
            cam_target.x, cam_target.y, cam_target.z,
            m_camera.GetDistance(), m_camera.GetYaw(), m_camera.GetPitch());
#else
        //debug_msg.Format(_T("candidate_count=%d, visible_count=%d, draw_call_count=%d, elapsed_ms=%.2f\n"),
        //    candidate_count, visible_count, draw_call_count, elapsed_ms);
#endif
        //OutputDebugString(debug_msg);
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

void CGLView::SetShowObjectOutline(bool show) {
    m_showObjectOutline = show;
}

void CGLView::SetShowTriangulationLines(bool show) {
    m_showTriangulationLines = show;
}

void CGLView::SetShowPickRay(bool show) {
    m_showPickRay = show;
    Invalidate();
}

void CGLView::SetDataset(const ShpDataset* dataset) {
    m_pDataset = dataset;
    if (m_pDataset) {
        Vec3 center = (m_pDataset->header.world_bbox_min + m_pDataset->header.world_bbox_max) * 0.5f;
        Vec3 extent = m_pDataset->header.world_bbox_max - m_pDataset->header.world_bbox_min;
        float max_extent = (extent.x > extent.z) ? extent.x : extent.z;
        m_camera.Recenter(center, max_extent * 0.05f);
#ifdef ENABLE_CULLING_STATS
        // 진단용: 배율 비교 실험 동안은 항상 이 시점으로 강제 고정
        //m_camera = Camera(center, max_extent * 0.05f, 0.0f, 1.55f);
        // 원하는 구도가 따로 있으면 위 줄 대신 이렇게 직접 값을 박아도 됩니다:
        // m_camera = Camera(Vec3(원하는_x, 0.0f, 원하는_z), 원하는_distance, 원하는_yaw_라디안, 원하는_pitch_라디안);
        m_camera = Camera(Vec3(388561.4375, 0.0000, -290364.6250), 1054.2139, -0.07, 0.310001);
#endif
    }
    BuildDebugGeometry();
}

void CGLView::BuildDebugGeometry() {
    m_drawRanges.clear();
    m_recordRanges.clear();
    m_fillRanges.clear();
    m_fillWireRanges.clear();
    if (!m_pDataset) return;

    std::vector<Vec3> vertices;
    std::vector<Vec3> fill_vertices;
    std::vector<uint32_t> fill_indices;
    std::vector<ExtrudeVertex> extrude_vertices;
    std::vector<uint32_t> extrude_indices;
    std::vector<Vec3> edge_vertices;
    std::vector<uint32_t> fill_wire_indices;

    int32_t record_count = static_cast<int32_t>(m_pDataset->records.size());

    for (int32_t i = 0; i < record_count; ++i) {
        const ShpPolygonRecord& record = m_pDataset->records[i];
        const float building_height = GetRecordHeight(*m_pDataset, static_cast<size_t>(i), kPlaceholderBuildingHeight);
        int32_t part_count = static_cast<int32_t>(record.part_start_indices.size());

        const std::vector<std::vector<Vec3>> rings = BuildRecordRings(record, i);

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

        FillRange wire_range;
        wire_range.first_index = static_cast<GLint>(fill_wire_indices.size());
        for (size_t t = 0; t + 2 < tri_indices.size(); t += 3) {
            uint32_t a = vertex_offset + tri_indices[t];
            uint32_t b = vertex_offset + tri_indices[t + 1];
            uint32_t c = vertex_offset + tri_indices[t + 2];
            fill_wire_indices.push_back(a); fill_wire_indices.push_back(b);
            fill_wire_indices.push_back(b); fill_wire_indices.push_back(c);
            fill_wire_indices.push_back(c); fill_wire_indices.push_back(a);
        }
        wire_range.index_count = static_cast<GLsizei>(fill_wire_indices.size() - wire_range.first_index);
        m_fillWireRanges.push_back(wire_range);

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
        record_range.bounds_max.y = building_height;

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

        //if (part_count > 1) {
        //    CString debug_msg;
        //    debug_msg.Format(_T("record %d: part_count=%d\n"), i, part_count);
        //    OutputDebugString(debug_msg);
        //    for (int32_t k = 0; k < static_cast<int32_t>(record.points.size()) && k < 5; ++k) {
        //        CString point_msg;
        //        point_msg.Format(_T("  point[%d] = (%.2f, %.2f, %.2f)\n"),
        //            k, record.points[k].x, record.points[k].y, record.points[k].z);
        //        OutputDebugString(point_msg);
        //    }
        //}

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

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_fillWireIndexBuffer);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, fill_wire_indices.size() * sizeof(uint32_t), fill_wire_indices.data(), GL_STATIC_DRAW);
}

void CGLView::ComputePickRay(CPoint point, Vec3* out_origin, Vec3* out_direction) const {
    // 1) 픽셀 -> NDC (y는 위아래가 반대라 부호를 뒤집는다)
    const float ndc_x = (2.0f * point.x) / static_cast<float>(m_clientWidth) - 1.0f;
    const float ndc_y = 1.0f - (2.0f * point.y) / static_cast<float>(m_clientHeight);

    // 2) 렌더링에 쓴 것과 "완전히 동일한" 카메라/투영으로 역행렬을 만든다.
    const Mat4 inv_view = Mat4InverseView(m_camera.GetViewMatrix());
    const Mat4 inv_proj = Mat4InversePerspective(kCameraFovRadians, m_aspect, kCameraNearPlane, kCameraFarPlane);

    // 역행렬은 곱하는 순서가 뒤집힘
    const Mat4 inv_view_proj = inv_view * inv_proj;

    // 3) 같은 픽셀의 근평면 점과 원평면 점을 각각 월드로 되돌린다
    const Vec3 near_world = Vec4PerspectiveDivide(inv_view_proj * Vec4(ndc_x, ndc_y, -1.0f, 1.0f));
    const Vec3 far_world = Vec4PerspectiveDivide(inv_view_proj * Vec4(ndc_x, ndc_y, 1.0f, 1.0f));

    // 4) 두 점을 이으면 레이가 됨
    *out_origin = near_world;
    *out_direction = Vec3Normalize(far_world - near_world);
}

// 건물 하나를 실제 삼각형(지붕 + 벽)
bool CGLView::IntersectRayRecord(const Vec3& origin, const Vec3& direction, int32_t record_index, float* out_t) const {
    if (!m_pDataset) return false;
    if (record_index < 0 || record_index >= static_cast<int32_t>(m_pDataset->records.size())) return false;

    const ShpPolygonRecord& record = m_pDataset->records[record_index];
    const float building_height = GetRecordHeight(*m_pDataset, static_cast<size_t>(record_index), kPlaceholderBuildingHeight);

    // 로그는 로딩 때 이미 찍었으므로 -1을 넘겨 억제한다
    const std::vector<std::vector<Vec3>> rings = BuildRecordRings(record, -1);

    // earcut이 인덱스로 가리키는 것과 같은 순서로 정점을 한 줄로 편다
    std::vector<Vec3> flat_points;
    for (const std::vector<Vec3>& ring : rings) {
        for (const Vec3& p : ring) {
            flat_points.push_back(p);
        }
    }

    const std::vector<uint32_t> tri_indices = mapbox::earcut<uint32_t>(rings);

    float best_t = FLT_MAX;
    float hit_t = 0.0f;

    // 1) 지붕 - 삼각분할 결과를 건물 높이로 올린 것
    for (size_t k = 0; k + 2 < tri_indices.size(); k += 3) {
        const Vec3& a = flat_points[tri_indices[k]];
        const Vec3& b = flat_points[tri_indices[k + 1]];
        const Vec3& c = flat_points[tri_indices[k + 2]];

        if (IntersectRayTriangle(origin, direction,
            Vec3(a.x, building_height, a.z),
            Vec3(b.x, building_height, b.z),
            Vec3(c.x, building_height, c.z), &hit_t) && hit_t < best_t) {
            best_t = hit_t;
        }
    }

    // 2) 벽 — 링의 변마다 바닥~지붕 사각형을 삼각형 2개로 (렌더링과 같은 순서)
    for (const std::vector<Vec3>& ring : rings) {
        const int32_t point_count = static_cast<int32_t>(ring.size());
        for (int32_t k = 0; k < point_count; ++k) {
            const Vec3& p0 = ring[k];
            const Vec3& p1 = ring[(k + 1) % point_count];

            const Vec3 bottom0(p0.x, 0.0f, p0.z);
            const Vec3 bottom1(p1.x, 0.0f, p1.z);
            const Vec3 top1(p1.x, building_height, p1.z);
            const Vec3 top0(p0.x, building_height, p0.z);

            if (IntersectRayTriangle(origin, direction, bottom0, bottom1, top1, &hit_t)
                && hit_t < best_t) best_t = hit_t;
            if (IntersectRayTriangle(origin, direction, bottom0, top1, top0, &hit_t)
                && hit_t < best_t) best_t = hit_t;
        }
    }

    if (best_t == FLT_MAX) return false;
    *out_t = best_t;
    return true;
}

void CGLView::UpdatePickAt(CPoint point)
{
    ComputePickRay(point, &m_pickRayOrigin, &m_pickRayDirection);
    m_hasPickRay = true;

    // 1) 화면에 보이는 건물들 중 가장 가까운 것
    int32_t best_index = -1;
    float best_t = FLT_MAX;
    int32_t aabb_pass_count = 0;

    for (int32_t index : m_lastVisibleIndices) {
        const RecordRange& record_range = m_recordRanges[index];
        
        float aabb_t = 0.0f;
        if (!IntersectRayAabb(m_pickRayOrigin, m_pickRayDirection, record_range.bounds_min, record_range.bounds_max, &aabb_t)) {
            continue;
        }
        ++aabb_pass_count;

        // AABB에 처음 닿는 거리는 그 안의 어떤 삼각형보다도 가깝거나 같다.
        // 이미 찾은 교차점보다 상자 자체가 멀면 안을 열어볼 필요가 없다.
        if (aabb_t >= best_t) continue;

        float triangle_t = 0.0f;
        if (IntersectRayRecord(m_pickRayOrigin, m_pickRayDirection, index, &triangle_t) && triangle_t < best_t) {
            best_t = triangle_t;
            best_index = index;
        }
    }

    if (best_index >= 0) {
        m_pickedRecordIndex = best_index;
        m_pickHitPoint = m_pickRayOrigin + m_pickRayDirection * best_t;
        m_pickHitDistance = best_t;
        m_hasPickHit = true;
        m_isPickMarkerVisible = true;

        CString msg;
        msg.Format(_T("[PickHit] building #%d (%.2f, %.2f, %.2f) t=%.2f  가시 %zu개 → AABB통과 %d개\n"),
            best_index, m_pickHitPoint.x, m_pickHitPoint.y, m_pickHitPoint.z,
            best_t, m_lastVisibleIndices.size(), aabb_pass_count);
        OutputDebugString(msg);
        return;
    }

    // 2) 건물을 못 맞췄으면 지면으로
    m_pickedRecordIndex = -1;
    float hit_t = 0.0f;
    m_hasPickHit = IntersectRayGroundPlane(m_pickRayOrigin, m_pickRayDirection, &hit_t);
    if (m_hasPickHit) {
        m_pickHitPoint = m_pickRayOrigin + m_pickRayDirection * hit_t;
        m_pickHitDistance = hit_t;
        m_isPickMarkerVisible = true;

        CString msg;
        msg.Format(_T("[PickHit] ground (%.2f, %.2f, %.2f) t=%.2f\n"),
            m_pickHitPoint.x, m_pickHitPoint.y, m_pickHitPoint.z, hit_t);
        OutputDebugString(msg);
    }
}

void CGLView::RenderPickRay() {
    const Vec3 end = m_pickRayOrigin + m_pickRayDirection * kPickRayDebugLength;
    const Vec3 line[2] = { m_pickRayOrigin, end };

    glBindBuffer(GL_ARRAY_BUFFER, m_pickRayVertexBuffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(line), line, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vec3), (void*)0);

    GLint color_loc = glGetUniformLocation(m_shaderProgram, "u_color");
    glUniform4f(color_loc, 1.0f, 0.2f, 0.2f, 1.0f);
    glDrawArrays(GL_LINES, 0, 2);
    glUniform4f(color_loc, 1.0f, 1.0f, 1.0f, 1.0f);
}

void CGLView::RenderPickMarker() {
    //화면에서 항상 비슷한 크기로 보이도록 현재 카메라와의 거리에 비례시킨다.
    const float distance = Vec3Length(m_pickHitPoint - m_camera.GetEye());
    const float half = distance * kPickMarkerScreenScale;
    const Vec3& p = m_pickHitPoint;

    // 꼭짓점 8개
    const Vec3 corner[8] = {
        Vec3(p.x - half, p.y - half, p.z - half),   // 0
        Vec3(p.x + half, p.y - half, p.z - half),   // 1
        Vec3(p.x + half, p.y + half, p.z - half),   // 2
        Vec3(p.x - half, p.y + half, p.z - half),   // 3
        Vec3(p.x - half, p.y - half, p.z + half),   // 4
        Vec3(p.x + half, p.y - half, p.z + half),   // 5
        Vec3(p.x + half, p.y + half, p.z + half),   // 6
        Vec3(p.x - half, p.y + half, p.z + half),   // 7
    };

    // 면 6개
    static const int kCubeIndices[36] = {
        0, 1, 2,  0, 2, 3,   // 뒷면   (z-)
        4, 5, 6,  4, 6, 7,   // 앞면   (z+)
        0, 4, 7,  0, 7, 3,   // 왼쪽   (x-)
        1, 2, 6,  1, 6, 5,   // 오른쪽 (x+)
        0, 1, 5,  0, 5, 4,   // 아랫면 (y-)
        3, 7, 6,  3, 6, 2,   // 윗면   (y+)
    };

    // 번호를 실제 좌표로 펼침
    Vec3 vertices[36];
    for (int i = 0; i < 36; i++) {
        vertices[i] = corner[kCubeIndices[i]];
    }

    glBindBuffer(GL_ARRAY_BUFFER, m_pickMarkerVertexBuffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vec3), (void*)0);

    GLint color_loc = glGetUniformLocation(m_shaderProgram, "u_color");
    glUniform4f(color_loc, 1.0f, 1.0f, 1.0f, 1.0f);
    glDrawArrays(GL_TRIANGLES, 0, 36);
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
    UpdateProjection();

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
    m_lButtonDownPos = point;

    UpdatePickAt(point);
    Invalidate();
    CWnd::OnLButtonDown(nFlags, point);
}

void CGLView::OnLButtonUp(UINT nFlags, CPoint point)
{
    m_isPanning = false;
    m_isPickMarkerVisible = false;
    ReleaseCapture();
    Invalidate();
    CWnd::OnLButtonUp(nFlags, point);
}

void CGLView::OnRButtonDown(UINT nFlags, CPoint point)
{
    SetCapture();
    m_lastMousePos = point;
    m_isRotating = true;

    UpdatePickAt(point);
    Invalidate();
    CWnd::OnRButtonDown(nFlags, point);
}

void CGLView::OnRButtonUp(UINT nFlags, CPoint point)
{
    m_isRotating = false;
    m_isPickMarkerVisible = false;
    ReleaseCapture();

    Invalidate();
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

void CGLView::UpdateProjection() {
    m_aspect = (m_clientHeight != 0)
        ? static_cast<float>(m_clientWidth) / static_cast<float>(m_clientHeight) : 1.0f;
    m_projMatrix = Mat4Perspective(kCameraFovRadians, m_aspect, kCameraNearPlane, kCameraFarPlane);
}