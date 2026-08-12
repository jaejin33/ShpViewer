#include "pch.h"
#include "GLView.h"

IMPLEMENT_DYNAMIC(CGLView, CWnd)

BEGIN_MESSAGE_MAP(CGLView, CWnd)
	ON_WM_PAINT()
	ON_WM_SIZE()
	ON_WM_ERASEBKGND()
END_MESSAGE_MAP()

CGLView::CGLView() {}
CGLView::~CGLView() { Cleanup(); }

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

    return eglMakeCurrent(m_eglDisplay, m_eglSurface, m_eglSurface, m_eglContext);
}

void CGLView::Render()
{
    glClearColor(0.1f, 0.1f, 0.15f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    // 여기에 실제 지오메트리 그리기 코드 추가 예정
    eglSwapBuffers(m_eglDisplay, m_eglSurface);
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

	// TODO: 여기에 메시지 처리기 코드를 추가합니다.
    if (m_eglDisplay != EGL_NO_DISPLAY) Render();
}

BOOL CGLView::OnEraseBkgnd(CDC* pDC)
{
	// TODO: 여기에 메시지 처리기 코드를 추가 및/또는 기본값을 호출합니다.

    return TRUE;
}
