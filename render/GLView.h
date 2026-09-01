#pragma once
#include <afxwin.h>
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include "../camera/Camera.h"
#include "../parse/ShpDataset.h"
#include <array>

class CGLView :
    public CWnd
{
    DECLARE_DYNAMIC(CGLView)

    struct DrawRange {
        GLint first;
        GLsizei count;
    };

    struct RecordRange {
        int32_t first_range_index = 0;
        int32_t range_count = 0;
        Vec3 bounds_min;
        Vec3 bounds_max;
    };

    struct FillRange {
        GLint first_index = 0;
        GLint index_count = 0;
    };

    struct ExtrudeRange {
        GLint first_index = 0;
        GLint index_count = 0;
    };

    struct EdgeRange {
        GLint first_vertex = 0;
        GLint vertex_count = 0;
    };

public:
    CGLView();
    virtual ~CGLView();

    BOOL InitEGL();
    void Render();
    void Cleanup();
    void SetDataset(const ShpDataset* dataset);
    void SetShowQuadTreeLevels(bool show);
    void SetShowAllNodes(bool show);
    void SetShowAllObjectLevelColors(bool show);
    void SetShowFrustum(bool show);
    void SetShowFill(bool show);
    void SetShowObjectBounds(bool show);
    void SetShow3D(bool show);
    void SetShowEdges(bool show);

protected:
    EGLDisplay m_eglDisplay = EGL_NO_DISPLAY;
    EGLSurface m_eglSurface = EGL_NO_SURFACE;
    EGLContext m_eglContext = EGL_NO_CONTEXT;
    Camera m_camera;
    bool m_isRotating = false;
    bool m_isPanning = false;
    CPoint m_lastMousePos;

    int m_clientWidth = 0;
    int m_clientHeight = 0;
    std::vector<RecordRange> m_recordRanges;

    const ShpDataset* m_pDataset = nullptr;
    GLuint m_shaderProgram = 0;
    GLuint m_vertexBuffer = 0;
    std::vector<DrawRange> m_drawRanges;
    GLuint m_fillVertexBuffer = 0;
    GLuint m_fillIndexBuffer = 0;
    std::vector<FillRange> m_fillRanges;
    GLuint m_extrudeVertexBuffer = 0;
    GLuint m_extrudeIndexBuffer = 0;
    std::vector<ExtrudeRange> m_extrudeRanges;
    GLuint m_edgeVertexBuffer = 0;
    std::vector<EdgeRange> m_edgeRanges;

    bool m_showAllObjectLevelColors = false;
    bool m_showAllNodes = false;
    bool m_showQuadTreeLevels = false;
    bool m_showFill = true;
    bool m_showObjectBounds = false;
    bool m_show3D = true;
    bool m_showEdges = true;
    GLuint m_nodeBoxVertexBuffer = 0;
    GLuint m_objectBoxVertexBuffer = 0;

    bool m_showFrustum = false;
    std::array<Vec3, 8> m_frustumCorners{};
    GLuint m_frustumVertexBuffer = 0;
    LARGE_INTEGER m_lastFrameTimestamp{};
    int32_t m_fpsFrameCount = 0;
    double m_fpsAccumulatedSeconds = 0.0f;
    float m_fps = 0.0f;

    void CaptureFrustumCorners();
    void RenderFrustum();
    void RenderObjectBounds(const std::vector<int32_t>& visible_indices, const std::vector<int32_t>& depths);

    bool InitShader();
    void BuildDebugGeometry();
    void RenderQuadTreeLevels(const std::vector<NodeDebugInfo>& nodes);

    DECLARE_MESSAGE_MAP()
    afx_msg void OnPaint();
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg BOOL OnEraseBkgnd(CDC* pDC);
public:
    afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
    afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
    afx_msg void OnRButtonDown(UINT nFlags, CPoint point);
    afx_msg void OnRButtonUp(UINT nFlags, CPoint point);
    afx_msg void OnMouseMove(UINT nFlags, CPoint point);
    afx_msg BOOL OnMouseWheel(UINT nFlags, short zDelta, CPoint pt);
    afx_msg void OnContextMenu(CWnd* /*pWnd*/, CPoint /*point*/);
};
