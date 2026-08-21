#include "pch.h"
#include "GLView.h"

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
END_MESSAGE_MAP()

CGLView::CGLView() : m_camera(Vec3(0.0f, 0.0f, 0.0f), 10.0f, 0.0f, 0.5f) {}
CGLView::~CGLView() { Cleanup(); }

namespace {
    constexpr float kCameraFovRadians = 0.7853982f;
    constexpr float kCameraNearPlane = 1.0f;
    constexpr float kCameraFarPlane = 100000.0f;
    constexpr float kZoomFactor = 0.9f;

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
    
    return TRUE;
}

bool CGLView::InitShader() {
    const char* vertex_source = R"(#version 300 es
layout(location = 0) in vec3 a_position;
uniform mat4 u_mvp;
void main() {
    gl_Position = u_mvp * vec4(a_position, 1.0);
}
)";

    const char* fragment_source = R"(#version 300 es
precision mediump float;
out vec4 frag_color;
void main() {
    frag_color = vec4(1.0, 1.0, 1.0, 1.0);
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
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    if (m_shaderProgram != 0 && !m_drawRanges.empty()) {
        glUseProgram(m_shaderProgram);

        float aspect = (m_clientHeight != 0)
            ? static_cast<float>(m_clientWidth) / static_cast<float>(m_clientHeight)
            : 1.0f;

        Mat4 view = m_camera.GetViewMatrix();
        Mat4 proj = Mat4Perspective(kCameraFovRadians, aspect, kCameraNearPlane, kCameraFarPlane);
        Mat4 mvp = proj * view;

        GLint mvp_loc = glGetUniformLocation(m_shaderProgram, "u_mvp");
        glUniformMatrix4fv(mvp_loc, 1, GL_FALSE, mvp.m);

        glBindBuffer(GL_ARRAY_BUFFER, m_vertexBuffer);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vec3), (void*)0);

        for (const DrawRange& range : m_drawRanges) {
            glDrawArrays(GL_LINE_LOOP, range.first, range.count);
        }
    }

    eglSwapBuffers(m_eglDisplay, m_eglSurface);
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
    if (!m_pDataset) return;

    std::vector<Vec3> vertices;
    const int32_t kMaxRecordsForSanityCheck = 100;
    int32_t record_count = (std::min)(kMaxRecordsForSanityCheck, static_cast<int32_t>(m_pDataset->records.size()));

    for (int32_t i = 0; i < record_count; ++i) {
        const ShpPolygonRecord& record = m_pDataset->records[i];
        int32_t part_count = static_cast<int32_t>(record.part_start_indices.size());

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
    }

    glBindBuffer(GL_ARRAY_BUFFER, m_vertexBuffer);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vec3), vertices.data(), GL_STATIC_DRAW);
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
        m_camera.Rotate(delta_x * 0.005f, delta_y * 0.005f);
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
