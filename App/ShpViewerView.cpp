// ShpViewerView.cpp

#include "pch.h"
#include "framework.h"
#ifndef SHARED_HANDLERS
#include "ShpViewer.h"
#endif

#include "ShpViewerDoc.h"
#include "ShpViewerView.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

IMPLEMENT_DYNCREATE(CShpViewerView, CView)

BEGIN_MESSAGE_MAP(CShpViewerView, CView)
    ON_COMMAND(ID_FILE_PRINT, &CView::OnFilePrint)
    ON_COMMAND(ID_FILE_PRINT_DIRECT, &CView::OnFilePrint)
    ON_COMMAND(ID_FILE_PRINT_PREVIEW, &CShpViewerView::OnFilePrintPreview)
    ON_WM_CONTEXTMENU()
    ON_WM_RBUTTONUP()
    ON_WM_CREATE()
    ON_WM_DESTROY()
    ON_WM_SIZE()
    ON_WM_ERASEBKGND()
    ON_WM_PAINT()
    ON_WM_TIMER()
    ON_MESSAGE(WM_USER + 1, &CShpViewerView::OnDataLoaded)
    ON_WM_MOUSEMOVE()
    ON_WM_LBUTTONUP()
    ON_WM_LBUTTONDOWN()
    ON_WM_MOUSEWHEEL()
    ON_WM_KEYDOWN()
    ON_WM_KEYUP()
END_MESSAGE_MAP()

CShpViewerView::CShpViewerView() noexcept
{
    m_eglDisplay = EGL_NO_DISPLAY;
    m_eglSurface = EGL_NO_SURFACE;
    m_eglContext = EGL_NO_CONTEXT;
}

CShpViewerView::~CShpViewerView() {}

BOOL CShpViewerView::PreCreateWindow(CREATESTRUCT& cs)
{
    return CView::PreCreateWindow(cs);
}

// WM_CREATE: EGL 초기화 및 OpenGL ES 3.0 컨텍스트 생성
int CShpViewerView::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
    if (CView::OnCreate(lpCreateStruct) == -1) return -1;

    HWND hWnd = GetSafeHwnd();

    // EGL 디스플레이 연결 및 초기화
    m_eglDisplay = eglGetDisplay(::GetDC(hWnd));
    EGLBoolean initOk = eglInitialize(m_eglDisplay, nullptr, nullptr);
    TRACE(_T("eglInitialize: %d, error: %x\n"), initOk, eglGetError());

    // EGL 설정 선택: RGBA 8888, 24비트 깊이 버퍼, OpenGL ES 3.0
    EGLint attribs[] = {
        EGL_RED_SIZE,        8,
        EGL_GREEN_SIZE,      8,
        EGL_BLUE_SIZE,       8,
        EGL_ALPHA_SIZE,      8,
        EGL_DEPTH_SIZE,      24,
        EGL_SURFACE_TYPE,    EGL_WINDOW_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
        EGL_NONE
    };
    EGLConfig config;
    EGLint numConfigs;
    EGLBoolean chooseOk = eglChooseConfig(m_eglDisplay, attribs, &config, 1, &numConfigs);
    TRACE(_T("eglChooseConfig: %d, numConfigs: %d, error: %x\n"), chooseOk, numConfigs, eglGetError());

    // 윈도우 서피스 생성
    m_eglSurface = eglCreateWindowSurface(m_eglDisplay, config, hWnd, nullptr);
    TRACE(_T("eglSurface: %p, error: %x\n"), m_eglSurface, eglGetError());

    // OpenGL ES 3.0 컨텍스트 생성
    EGLint contextAttribs[] = {
        EGL_CONTEXT_CLIENT_VERSION, 3,
        EGL_NONE
    };
    m_eglContext = eglCreateContext(m_eglDisplay, config, EGL_NO_CONTEXT, contextAttribs);
    TRACE(_T("eglContext: %p, error: %x\n"), m_eglContext, eglGetError());

    // 현재 스레드에 EGL 컨텍스트 바인딩
    EGLBoolean mcOk = eglMakeCurrent(m_eglDisplay, m_eglSurface, m_eglSurface, m_eglContext);
    TRACE(_T("eglMakeCurrent: %d, error: %x\n"), mcOk, eglGetError());

    // GL 드라이버 정보 출력 (디버그용)
    const char* vendor   = (const char*)glGetString(GL_VENDOR);
    const char* renderer = (const char*)glGetString(GL_RENDERER);
    const char* version  = (const char*)glGetString(GL_VERSION);
    TRACE(_T("GL_VENDOR: %S\n"),   vendor   ? vendor   : "null");
    TRACE(_T("GL_RENDERER: %S\n"), renderer ? renderer : "null");
    TRACE(_T("GL_VERSION: %S\n"),  version  ? version  : "null");

    // 렌더러와 카메라 생성 및 연결
    m_renderer = std::make_shared<Render::Renderer>();
    m_camera   = std::make_shared<Render::Camera>(1.0f);
    m_renderer->SetCamera(m_camera);
    m_renderer->Initialize();

    return 0;
}

// WM_DESTROY: EGL 컨텍스트 해제
void CShpViewerView::OnDestroy()
{
    eglMakeCurrent(m_eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    if (m_eglContext != EGL_NO_CONTEXT) eglDestroyContext(m_eglDisplay, m_eglContext);
    if (m_eglSurface != EGL_NO_SURFACE) eglDestroySurface(m_eglDisplay, m_eglSurface);
    if (m_eglDisplay != EGL_NO_DISPLAY) eglTerminate(m_eglDisplay);
    CView::OnDestroy();
}

// 한 프레임을 렌더링한다.
//   1. FPS 지수 이동 평균(EMA) 갱신
//   2. GL 렌더 (RenderFrame)
//   3. eglSwapBuffers로 화면 표시
//   4. GDI로 인스펙터 오버레이 그리기
void CShpViewerView::OnDraw(CDC* pDC)
{
    if (m_eglDisplay == EGL_NO_DISPLAY) return;

    // FPS 갱신: 이전 프레임과의 간격으로 순간 FPS 계산 후 EMA 평활화
    ULONGLONG now = GetTickCount64();
    if (m_lastFrameTick != 0) {
        ULONGLONG dt = now - m_lastFrameTick;
        if (dt > 0) {
            float instant = 1000.0f / (float)dt;
            // 첫 프레임은 그냥 대입, 이후는 EMA (가중치 0.15)
            m_fps = (m_fps < 1.0f) ? instant : (m_fps * 0.85f + instant * 0.15f);
        }
    }
    m_lastFrameTick = now;

    eglMakeCurrent(m_eglDisplay, m_eglSurface, m_eglSurface, m_eglContext);

    // 뷰포트를 현재 클라이언트 영역 크기로 설정
    CRect rect;
    GetClientRect(&rect);
    glViewport(0, 0, rect.Width(), rect.Height());

    glClearColor(0.0f, 1.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (m_renderer) m_renderer->RenderFrame();

    eglSwapBuffers(m_eglDisplay, m_eglSurface); // GL 백버퍼 -> 화면

    DrawInspectorOverlay(); // GDI 오버레이 (텍스트 + 버튼)
}

// WM_SIZE: 화면 크기 변경 시 카메라 종횡비 업데이트
void CShpViewerView::OnSize(UINT nType, int cx, int cy)
{
    CView::OnSize(nType, cx, cy);

    if (m_eglDisplay != EGL_NO_DISPLAY && cx > 0 && cy > 0) {
        eglMakeCurrent(m_eglDisplay, m_eglSurface, m_eglSurface, m_eglContext);
        glViewport(0, 0, cx, cy);

        if (m_camera) {
            m_camera->SetAspectRatio((float)cx, (float)cy);
            m_camera->RecalculateMatrix();
        }
    }
}

// 도큐먼트가 변경되었을 때 호출 (새 파일 로드 후 카메라 초기 위치 설정)
void CShpViewerView::OnUpdate(CView* pSender, LPARAM lHint, CObject* pHint)
{
    CView::OnUpdate(pSender, lHint, pHint);

    CShpViewerDoc* pDoc = GetDocument();
    if (!pDoc) return;

    auto quadTree = pDoc->GetQuadTree();
    if (!quadTree) return;

    // 데이터 전체 범위의 중심을 타겟으로, 전체 범위가 화면에 들어오도록 거리 설정
    // FOV=45도, pitch=60도 기준: distance = extent / (2*tan(22.5)*sin(60)) ~ extent * 1.32
    // 여기에 여유 마진 1.4x를 곱해 초기 줌 설정
    auto bounds = quadTree->GetBounds();
    if (bounds.maxX > bounds.minX && bounds.maxY > bounds.minY) {
        float midX   = static_cast<float>((bounds.minX + bounds.maxX) / 2.0);
        float midY   = static_cast<float>((bounds.minY + bounds.maxY) / 2.0);
        double dw = bounds.maxX - bounds.minX, dh = bounds.maxY - bounds.minY;
        float extent = static_cast<float>(dw > dh ? dw : dh);
        float distance = extent * 1.4f;

        m_camera->SetPosition(glm::vec2(midX, midY));
        m_camera->SetZoom(distance);
    }

    UpdateVisibleItems();
}

// 백그라운드 데이터 로드 완료 메시지 핸들러 (현재 미사용)
LRESULT CShpViewerView::OnDataLoaded(WPARAM wParam, LPARAM lParam)
{
    return 0;
}

// WM_PAINT: OnDraw를 호출해 한 프레임을 렌더링
void CShpViewerView::OnPaint()
{
    ValidateRect(nullptr);
    OnDraw(nullptr);
}

// WM_TIMER:
//   타이머 ID=2 : 마우스 휠 정지 후 약간의 지연을 두고 UpdateVisibleItems 호출
void CShpViewerView::OnTimer(UINT_PTR nIDEvent)
{
    if (nIDEvent == 2) {
        KillTimer(2);
        UpdateVisibleItems(); // 휠 멈춘 후 한 번만 재업로드
    }
    CView::OnTimer(nIDEvent);
}

// 배경 지우기 비활성화 (OpenGL이 직접 배경을 채우므로 GDI 지우기 불필요)
BOOL CShpViewerView::OnEraseBkgnd(CDC* pDC) { return TRUE; }

// 뷰가 처음 표시될 때 호출 — 30ms 타이머 시작 (연속 렌더링)
void CShpViewerView::OnInitialUpdate()
{
    CView::OnInitialUpdate();
    SetTimer(1, 30, NULL);
}

// 인쇄 관련 함수 (기본 구현 유지)
void CShpViewerView::OnFilePrintPreview()
{
#ifndef SHARED_HANDLERS
    AFXPrintPreview(this);
#endif
}

BOOL CShpViewerView::OnPreparePrinting(CPrintInfo* pInfo)
{
    return DoPreparePrinting(pInfo);
}

void CShpViewerView::OnBeginPrinting(CDC*, CPrintInfo*) {}
void CShpViewerView::OnEndPrinting(CDC*, CPrintInfo*) {}

void CShpViewerView::OnRButtonUp(UINT, CPoint point)
{
    ClientToScreen(&point);
    OnContextMenu(this, point);
}

void CShpViewerView::OnContextMenu(CWnd*, CPoint point)
{
#ifndef SHARED_HANDLERS
    theApp.GetContextMenuManager()->ShowPopupMenu(IDR_POPUP_EDIT, point.x, point.y, this, TRUE);
#endif
}

#ifdef _DEBUG
void CShpViewerView::AssertValid() const { CView::AssertValid(); }
void CShpViewerView::Dump(CDumpContext& dc) const { CView::Dump(dc); }
CShpViewerDoc* CShpViewerView::GetDocument() const
{
    ASSERT(m_pDocument->IsKindOf(RUNTIME_CLASS(CShpViewerDoc)));
    return (CShpViewerDoc*)m_pDocument;
}
#endif

// 마우스 이동: 드래그 중이면 3D 지면(Z=0) 레이캐스트로 카메라 타겟을 이동한다.
//
// 현재/이전 마우스 위치를 각각 NDC로 변환 후 Z=0 평면에 역투영(unproject)하여
// 월드 좌표 차이만큼 카메라 타겟을 이동시킨다.
// 이렇게 하면 마우스로 잡은 지점이 커서 아래에 고정된다.
void CShpViewerView::OnMouseMove(UINT nFlags, CPoint point)
{
    if (!m_camera) return;

    if (m_isDragging) {
        CRect rect;
        GetClientRect(&rect);
        if (rect.Width() > 0 && rect.Height() > 0) {
            // 화면 픽셀 좌표 -> NDC [-1,1] 변환 람다
            auto toNDC = [&](CPoint p, float& nx, float& ny) {
                nx =  2.0f * p.x / rect.Width()  - 1.0f;
                ny = -2.0f * p.y / rect.Height() + 1.0f; // Y축 반전
            };
            float cx, cy, px, py;
            toNDC(point,          cx, cy); // 현재 마우스 위치 (NDC)
            toNDC(m_lastMousePos, px, py); // 이전 마우스 위치 (NDC)

            // 두 NDC 좌표를 Z=0 지면으로 역투영
            glm::vec3 worldCur  = m_camera->UnprojectToGround(cx, cy);
            glm::vec3 worldPrev = m_camera->UnprojectToGround(px, py);

            // 이전 - 현재 = 카메라가 이동해야 할 방향 (잡은 점 고정)
            glm::vec2 delta(worldPrev.x - worldCur.x,
                            worldPrev.y - worldCur.y);
            m_camera->Move(delta);
            Invalidate(FALSE);
        }
    }

    m_lastMousePos = point;
}

// 마우스 왼쪽 버튼 뗌: 드래그 종료 후 가시 객체 재업로드
void CShpViewerView::OnLButtonUp(UINT nFlags, CPoint point)
{
    m_isDragging = false;
    ReleaseCapture();
    UpdateVisibleItems();
}

// 마우스 왼쪽 버튼 누름: 인스펙터 토글 버튼 클릭 확인 또는 드래그 시작
void CShpViewerView::OnLButtonDown(UINT nFlags, CPoint point)
{
    // 인스펙터 버튼 영역 클릭 확인 (드래그보다 우선)
    if (m_btnLevelColors.PtInRect(point)) {
        m_showLevelColors = !m_showLevelColors;
        UpdateVisibleItems();
        return;
    }
    if (m_btnItemMBR.PtInRect(point)) {
        m_showItemMBR = !m_showItemMBR;
        m_renderer->SetShowItemMBR(m_showItemMBR);
        UpdateVisibleItems();
        return;
    }
    if (m_btnNodeMBR.PtInRect(point)) {
        m_showNodeMBR = !m_showNodeMBR;
        m_renderer->SetShowNodeMBR(m_showNodeMBR);
        UpdateVisibleItems();
        return;
    }
    if (m_btnFrustum.PtInRect(point)) {
        m_showFrustum = !m_showFrustum;
        m_renderer->SetShowFrustum(m_showFrustum);
        UpdateVisibleItems();
        return;
    }

    // 버튼 영역이 아니면 드래그 시작
    m_isDragging = true;
    m_lastMousePos = point;
    SetCapture();
}

// 마우스 휠: 거리(distance) 비율로 줌인/줌아웃
// 위로 굴리면 0.9배(줌인), 아래로 굴리면 1.1배(줌아웃)
// 타이머 2를 사용해 400ms 후 UpdateVisibleItems 호출 (성능 최적화)
BOOL CShpViewerView::OnMouseWheel(UINT nFlags, short zDelta, CPoint pt)
{
    if (!m_camera) return TRUE;

    float factor = (zDelta > 0) ? 0.9f : 1.1f;
    m_camera->SetZoom(m_camera->GetZoom() * factor);
    Invalidate(FALSE);

    SetTimer(2, 400, NULL); // 휠 멈춤 감지용 타이머 재설정
    return TRUE;
}

// 가시 객체를 쿼드트리에서 조회하고 GPU에 업로드한다.
//
// LOD 계산 (거리 기반 적응형):
//   lodScale = 2*tan(fov/2) * MIN_SCREEN_PIXELS / screenH
//   각 객체에 대해: objectSize >= distToEye * lodScale 이어야 렌더링
//   이 방식은 카메라 pitch와 무관하게 안정적이다.
//
// 쿼드트리 노드 조기 종료로 서브트리를 O(1)에 제거한다.
void CShpViewerView::UpdateVisibleItems()
{
    CShpViewerDoc* pDoc = GetDocument();
    if (!pDoc || !m_camera) return;

    auto quadTree = pDoc->GetQuadTree();
    if (!quadTree) return;

    auto viewBounds = m_camera->GetViewBounds(); // 현재 카메라 가시 범위 AABB

    // 거리 기반 LOD 스케일 계산
    // MIN_SCREEN_PIXELS : 객체가 화면에서 최소 N픽셀 이상이어야 표시
    constexpr float MIN_SCREEN_PIXELS = 4.0f;
    CRect rect;
    GetClientRect(&rect);
    float screenH  = (rect.Height() > 0) ? (float)rect.Height() : 800.0f;
    float lodScale = 2.0f * tan(m_camera->GetFov() * 0.5f) / screenH * MIN_SCREEN_PIXELS;

    // 인스펙터 표시용: 타겟 거리에서의 LOD 임계값 (월드 단위)
    m_minObjectSize = m_camera->GetZoom() * lodScale;

    glm::vec3 eye = m_camera->GetEyePosition();

    // 쿼드트리에서 가시 객체 수집
    std::vector<Spatial::ItemInfo> visibleItems;
    quadTree->GetVisibleItemsWithDepth(viewBounds, visibleItems,
        eye.x, eye.y, eye.z, lodScale);

    TRACE(_T("visible: %zu  minSize: %.1f\n"), visibleItems.size(), m_minObjectSize);

    m_totalCount     = quadTree->GetTotalCount();
    m_renderingCount = (int)visibleItems.size();

    if (m_eglDisplay != EGL_NO_DISPLAY) {
        eglMakeCurrent(m_eglDisplay, m_eglSurface, m_eglSurface, m_eglContext);

        // 폴리곤 업로드 (레벨 색상 or 플랫 색상)
        m_renderer->LoadGeometries(visibleItems, m_showLevelColors);

        // MBR 업로드 (토글 ON인 레이어만)
        if (m_showItemMBR || m_showNodeMBR) {
            std::vector<Spatial::NodeInfo> nodes;
            if (m_showNodeMBR) quadTree->GetVisibleNodes(viewBounds, nodes);
            std::vector<Spatial::ItemInfo> mbrItems;
            if (m_showItemMBR) mbrItems = visibleItems;
            m_renderer->LoadMBR(nodes, mbrItems);
        } else {
            m_renderer->ClearMBR();
        }

        // 프러스텀 업로드 (토글 ON일 때만)
        if (m_showFrustum) {
            glm::vec3 corners[4];
            m_camera->GetFrustumCorners(corners);
            m_renderer->LoadFrustum(eye, corners);
        } else {
            m_renderer->ClearFrustum();
        }
    }

    Invalidate(FALSE);
}

// 키 누름 이벤트 처리
//   Q/E  : yaw 좌우 회전
//   R/F  : pitch 위아래 기울기
//   W/S  : 앞뒤 이동 (Pan)
//   A/D  : 좌우 이동 (Pan)
//   1~4  : 각 레이어 토글
void CShpViewerView::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags)
{
    if (!m_camera) return;

    // 이동 단위: 현재 거리의 2% (거리가 멀수록 빠르게 이동)
    float panStep = m_camera->GetZoom() * 0.02f;

    switch (nChar) {
    // --- yaw 회전 ---
    case 'Q': m_camera->AddRotation( 0.05f); Invalidate(FALSE); break; // 왼쪽 회전
    case 'E': m_camera->AddRotation(-0.05f); Invalidate(FALSE); break; // 오른쪽 회전

    // --- pitch 기울기 ---
    case 'R': m_camera->AddPitch( 0.05f); Invalidate(FALSE); break; // 더 위에서 보기
    case 'F': m_camera->AddPitch(-0.05f); Invalidate(FALSE); break; // 더 비스듬히 보기

    // --- 이동 (WASD) ---
    case 'W': m_camera->Pan( panStep, 0.0f); Invalidate(FALSE); break; // 앞으로
    case 'S': m_camera->Pan(-panStep, 0.0f); Invalidate(FALSE); break; // 뒤로
    case 'A': m_camera->Pan(0.0f, -panStep); Invalidate(FALSE); break; // 왼쪽
    case 'D': m_camera->Pan(0.0f,  panStep); Invalidate(FALSE); break; // 오른쪽

    // --- 인스펙터 토글 ---
    case '1':
        m_showLevelColors = !m_showLevelColors;
        UpdateVisibleItems();
        return;
    case '2':
        m_showItemMBR = !m_showItemMBR;
        m_renderer->SetShowItemMBR(m_showItemMBR);
        UpdateVisibleItems();
        return;
    case '3':
        m_showNodeMBR = !m_showNodeMBR;
        m_renderer->SetShowNodeMBR(m_showNodeMBR);
        UpdateVisibleItems();
        return;
    case '4':
        m_showFrustum = !m_showFrustum;
        m_renderer->SetShowFrustum(m_showFrustum);
        UpdateVisibleItems();
        return;

    default:
        CView::OnKeyDown(nChar, nRepCnt, nFlags);
        return;
    }
}

// 키 뗌 이벤트: 카메라 조작 키를 떼면 가시 객체 재업로드 (LOD 재계산)
void CShpViewerView::OnKeyUp(UINT nChar, UINT nRepCnt, UINT nFlags)
{
    if (!m_camera) return;
    switch (nChar) {
    case 'Q': case 'E':  // yaw
    case 'R': case 'F':  // pitch
    case 'W': case 'S':  // 앞뒤 이동
    case 'A': case 'D':  // 좌우 이동
        UpdateVisibleItems(); // 키를 뗄 때 한 번만 재업로드
        break;
    }
    CView::OnKeyUp(nChar, nRepCnt, nFlags);
}

// 화면 좌상단에 GDI로 인스펙터 오버레이를 그린다.
//
// 구성:
//   정보 4줄 : 총 객체 수, 렌더링 중 객체 수, LOD 임계값, FPS
//   버튼 4개 : 레벨색상[1], 객체MBR[2], 노드MBR[3], 프러스텀[4]
//              ON 상태: 어두운 초록 배경 + 밝은 초록 텍스트
//              OFF 상태: 어두운 회색 배경 + 회색 텍스트
//   FPS 색상  : 30fps 이상=초록, 15~30=노랑, 15 미만=빨강
void CShpViewerView::DrawInspectorOverlay()
{
    CDC* pDC = GetDC();
    if (!pDC) return;

    const int x = 10, y = 10;         // 오버레이 좌상단 위치
    const int lineH = 18, padding = 6; // 줄 높이, 내부 여백
    const int boxW = 195;              // 박스 가로 크기
    // 4 정보줄 + 4 버튼줄
    const int boxH = lineH * 8 + padding * 2;

    // 메인 배경 (어두운 검정)
    CBrush bgBrush(RGB(20, 20, 20));
    CRect bgRect(x, y, x + boxW, y + boxH);
    pDC->FillRect(&bgRect, &bgBrush);

    // 버튼 영역 시작 Y (정보 4줄 아래)
    const int btnTopY = y + padding + lineH * 4;

    // 정보 영역과 버튼 영역 사이 구분선
    CPen sepPen(PS_SOLID, 1, RGB(55, 55, 55));
    CPen* pOldPen = pDC->SelectObject(&sepPen);
    pDC->MoveTo(x + 4,        btnTopY);
    pDC->LineTo(x + boxW - 4, btnTopY);
    pDC->SelectObject(pOldPen);

    // 버튼 배경 그리기 (ON: 어두운 초록, OFF: 어두운 회색)
    struct BtnDef { CRect* rect; bool state; };
    BtnDef btnDefs[4] = {
        { &m_btnLevelColors, m_showLevelColors },
        { &m_btnItemMBR,     m_showItemMBR     },
        { &m_btnNodeMBR,     m_showNodeMBR     },
        { &m_btnFrustum,     m_showFrustum     },
    };
    for (int i = 0; i < 4; ++i) {
        *btnDefs[i].rect = CRect(x, btnTopY + lineH * i,
                                 x + boxW, btnTopY + lineH * (i + 1));
        CBrush btnBrush(btnDefs[i].state ? RGB(20, 45, 20) : RGB(32, 32, 32));
        pDC->FillRect(btnDefs[i].rect, &btnBrush);
    }

    // 폰트 설정 (Consolas 9pt)
    CFont font;
    font.CreatePointFont(90, _T("Consolas"));
    CFont* pOldFont = pDC->SelectObject(&font);
    pDC->SetBkMode(TRANSPARENT);

    // --- 정보 줄 (밝은 파란빛 흰색) ---
    pDC->SetTextColor(RGB(200, 230, 255));
    CString text;
    text.Format(_T("총 객체:    %d"), m_totalCount);
    pDC->TextOut(x + padding, y + padding + lineH * 0, text);
    text.Format(_T("렌더링:     %d"), m_renderingCount);
    pDC->TextOut(x + padding, y + padding + lineH * 1, text);
    text.Format(_T("LOD:    >= %.1f"), m_minObjectSize);
    pDC->TextOut(x + padding, y + padding + lineH * 2, text);

    // FPS 색상: 30fps 이상=초록, 15~30=노랑, 15 미만=빨강
    if (m_fps >= 30.0f)      pDC->SetTextColor(RGB(80, 255, 120));
    else if (m_fps >= 15.0f) pDC->SetTextColor(RGB(255, 210, 60));
    else                     pDC->SetTextColor(RGB(255, 90,  70));
    text.Format(_T("FPS:        %.0f"), m_fps);
    pDC->TextOut(x + padding, y + padding + lineH * 3, text);

    // --- 토글 버튼 텍스트 ---
    struct BtnLabel { const TCHAR* labelOn; const TCHAR* labelOff; bool state; int row; };
    BtnLabel btns[4] = {
        { _T("레벨색상   ON [1]"), _T("레벨색상  OFF [1]"), m_showLevelColors, 0 },
        { _T("객체 MBR   ON [2]"), _T("객체 MBR  OFF [2]"), m_showItemMBR,     1 },
        { _T("노드 MBR   ON [3]"), _T("노드 MBR  OFF [3]"), m_showNodeMBR,     2 },
        { _T("프러스텀   ON [4]"), _T("프러스텀  OFF [4]"), m_showFrustum,     3 },
    };
    for (auto& b : btns) {
        // ON: 밝은 초록, OFF: 회색
        pDC->SetTextColor(b.state ? RGB(80, 255, 120) : RGB(145, 145, 145));
        pDC->TextOut(x + padding, btnTopY + lineH * b.row + 1, b.state ? b.labelOn : b.labelOff);
    }

    pDC->SelectObject(pOldFont);
    ReleaseDC(pDC);
}
