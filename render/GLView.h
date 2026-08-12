#pragma once
#include <afxwin.h>
#include <EGL/egl.h>
#include <GLES3/gl3.h>

class CGLView :
    public CWnd
{
    DECLARE_DYNAMIC(CGLView)

public:
    CGLView();
    virtual ~CGLView();

    BOOL InitEGL();
    void Render();
    void Cleanup();

protected:
    EGLDisplay m_eglDisplay = EGL_NO_DISPLAY;
    EGLSurface m_eglSurface = EGL_NO_SURFACE;
    EGLContext m_eglContext = EGL_NO_CONTEXT;

    DECLARE_MESSAGE_MAP()
    afx_msg void OnPaint();
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg BOOL OnEraseBkgnd(CDC* pDC);
};
