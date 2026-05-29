// ShpViewerView.h
#pragma once
#include "Renderer.h"
#include <memory>
#include <EGL/egl.h>
#include <GLES3/gl3.h>

// MFC CView를 상속받은 메인 뷰 클래스
// EGL/OpenGL ES 3.0 컨텍스트를 직접 관리하며, 카메라 조작과 인스펙터 오버레이를 담당한다.
class CShpViewerView : public CView
{
protected:
    CShpViewerView() noexcept;
    DECLARE_DYNCREATE(CShpViewerView)

public:
    CShpViewerDoc* GetDocument() const;

private:
    // EGL 핸들 (OpenGL ES 컨텍스트 관리)
    EGLDisplay m_eglDisplay = EGL_NO_DISPLAY;
    EGLSurface m_eglSurface = EGL_NO_SURFACE;
    EGLContext m_eglContext = EGL_NO_CONTEXT;

    // 렌더러와 카메라 (공유 포인터로 생명주기 관리)
    std::shared_ptr<Render::Renderer> m_renderer;
    std::shared_ptr<Render::Camera>   m_camera;

    std::atomic<bool> m_isLoading = false; // 데이터 로딩 중 여부 (미사용 예약)

    // 마우스 드래그 상태
    bool   m_isDragging = false;
    CPoint m_lastMousePos;

    // 인스펙터 표시 정보
    int    m_totalCount = 0;       // 전체 객체 수
    int    m_renderingCount = 0;   // 현재 렌더링 중인 객체 수
    float  m_minObjectSize = 0.0f; // 현재 LOD 임계값 (월드 단위)

    // 렌더링 옵션 토글 상태
    bool   m_showLevelColors = true;  // 레벨(깊이)별 색상 사용 여부
    bool   m_showItemMBR = false;     // 객체 MBR 표시 여부
    bool   m_showNodeMBR = false;     // 쿼드트리 노드 MBR 표시 여부
    bool   m_showFrustum = false;     // 카메라 프러스텀 표시 여부

    // 인스펙터 토글 버튼의 화면 클릭 영역 (매 프레임 DrawInspectorOverlay에서 갱신)
    CRect  m_btnLevelColors{};
    CRect  m_btnItemMBR{};
    CRect  m_btnNodeMBR{};
    CRect  m_btnFrustum{};

    // FPS 계산 (지수 이동 평균)
    ULONGLONG m_lastFrameTick = 0; // 직전 프레임의 GetTickCount64() 값
    float     m_fps = 0.0f;        // 평활화된 FPS

    // 현재 가시 범위의 객체를 쿼드트리에서 조회하고 GPU에 업로드한다
    void UpdateVisibleItems();

    // 화면 좌상단에 GDI로 인스펙터 오버레이와 토글 버튼을 그린다
    void DrawInspectorOverlay();

    // remaining declarations
public:
    virtual void OnDraw(CDC* pDC);
    virtual void OnInitialUpdate();
    virtual void OnUpdate(CView* pSender, LPARAM lHint, CObject* pHint);
    virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
protected:
    virtual BOOL OnPreparePrinting(CPrintInfo* pInfo);
    virtual void OnBeginPrinting(CDC* pDC, CPrintInfo* pInfo);
    virtual void OnEndPrinting(CDC* pDC, CPrintInfo* pInfo);
public:
    virtual ~CShpViewerView();
#ifdef _DEBUG
    virtual void AssertValid() const;
    virtual void Dump(CDumpContext& dc) const;
#endif
protected:
    afx_msg void OnFilePrintPreview();
    afx_msg void OnRButtonUp(UINT nFlags, CPoint point);
    afx_msg void OnContextMenu(CWnd* pWnd, CPoint point);
    DECLARE_MESSAGE_MAP()
public:
    afx_msg int  OnCreate(LPCREATESTRUCT lpCreateStruct);
    afx_msg void OnDestroy();
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg BOOL OnEraseBkgnd(CDC* pDC);
    afx_msg void OnPaint();
    afx_msg void OnTimer(UINT_PTR nIDEvent);
    afx_msg LRESULT OnDataLoaded(WPARAM wParam, LPARAM lParam);
    afx_msg void OnMouseMove(UINT nFlags, CPoint point);
    afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
    afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
    afx_msg BOOL OnMouseWheel(UINT nFlags, short zDelta, CPoint pt);
    afx_msg void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
    afx_msg void OnKeyUp(UINT nChar, UINT nRepCnt, UINT nFlags);
};

#ifndef _DEBUG
inline CShpViewerDoc* CShpViewerView::GetDocument() const
{
    return reinterpret_cast<CShpViewerDoc*>(m_pDocument);
}
#endif
