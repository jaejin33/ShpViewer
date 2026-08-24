#pragma once
#include <afxwin.h>
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include "../camera/Camera.h"
#include "../parse/ShpDataset.h"

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

public:
    CGLView();
    virtual ~CGLView();

    BOOL InitEGL();
    void Render();
    void Cleanup();
    void SetDataset(const ShpDataset* dataset);

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

    bool InitShader();
    void BuildDebugGeometry();

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
};
