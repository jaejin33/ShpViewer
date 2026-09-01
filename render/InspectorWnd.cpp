#include "pch.h"
#include "InspectorWnd.h"
#include "../ShpViewerView.h"

namespace {
    constexpr COLORREF kColorBackground = RGB(18, 24, 38);
    constexpr COLORREF kColorSectionBg = RGB(26, 36, 58);
    constexpr COLORREF kColorAccent = RGB(0, 200, 255);
    constexpr COLORREF kColorWhite = RGB(255, 255, 255);
    constexpr COLORREF kColorMuted = RGB(140, 170, 200);
    constexpr COLORREF kColorGreen = RGB(74, 222, 128);
    constexpr COLORREF kColorYellow = RGB(255, 220, 80);
}

CInspectorWnd::CInspectorWnd() {}
CInspectorWnd::~CInspectorWnd() {}

BEGIN_MESSAGE_MAP(CInspectorWnd, CWnd)
    ON_WM_PAINT()
    ON_WM_ERASEBKGND()
    ON_BN_CLICKED(kToggleQuadTreeButtonId, &CInspectorWnd::OnToggleQuadTreeClicked)
    ON_BN_CLICKED(kToggleAllNodesButtonId, &CInspectorWnd::OnToggleAllNodesClicked)
    ON_BN_CLICKED(kToggleObjectColorButtonId, &CInspectorWnd::OnToggleObjectColorClicked)
    ON_BN_CLICKED(kToggleFillButtonId, &CInspectorWnd::OnToggleFillClicked)
    ON_BN_CLICKED(kToggleObjectBoundsButtonId, &CInspectorWnd::OnToggleObjectBoundsClicked)
    ON_BN_CLICKED(kToggle3DButtonId, &CInspectorWnd::OnToggle3DClicked)
    ON_BN_CLICKED(kToggleEdgesButtonId, &CInspectorWnd::OnToggleEdgesClicked)
END_MESSAGE_MAP()

BOOL CInspectorWnd::Create(CWnd* parent_wnd) {
    if (!CWnd::Create(nullptr, _T(""), WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN,
        CRect(0, 0, 0, 0), parent_wnd, 0)) {
        return FALSE;
    }

    m_toggleQuadTreeButton.Create(
        _T("레벨 표시: OFF"),
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        CRect(10, 220, 210, 250),
        this,
        kToggleQuadTreeButtonId);

    m_toggleAllNodesButton.Create(_T("전체 노드 보기: OFF"),
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        CRect(10, 260, 210, 290), this, kToggleAllNodesButtonId);

    m_toggleObjectColorButton.Create(_T("객체 레벨 색상: OFF"),
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        CRect(10, 300, 210, 330), this, kToggleObjectColorButtonId);

    m_toggleFillButton.Create(
        m_showFill ? _T("채우기: ON") : _T("채우기: OFF"),
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        CRect(10, 340, 210, 370),
        this,
        kToggleFillButtonId);

    m_toggleObjectBoundsButton.Create(_T("객체 MBR: OFF"),   
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        CRect(10, 380, 210, 410), this, kToggleObjectBoundsButtonId);

    m_toggle3DButton.Create(
        m_show3D ? _T("3D: ON") : _T("3D: OFF"),
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        CRect(10, 420, 108, 450),
        this,
        kToggle3DButtonId);

    m_toggleEdgesButton.Create(
        m_showEdges ? _T("윤곽선: ON") : _T("윤곽선: OFF"),
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        CRect(112, 420, 210, 450),
        this,
        kToggleEdgesButtonId);

    return TRUE;
}

BOOL CInspectorWnd::OnEraseBkgnd(CDC* /*pDC*/) {
    return TRUE;   // 배경은 OnPaint에서 통째로 칠할 거라, 여기서 미리 지우면 깜빡임만 늘어남
}

void CInspectorWnd::DrawSection(CDC* dc, int& y, LPCTSTR title, COLORREF color, int width, CFont* font) {
    CRect header_rect(0, y, width, y + 28);
    dc->FillSolidRect(&header_rect, kColorSectionBg);
    dc->FillSolidRect(CRect(0, y, 3, y + 28), color);   // 왼쪽 컬러 바

    CFont* old_font = dc->SelectObject(font);
    dc->SetBkMode(TRANSPARENT);
    dc->SetTextColor(color);
    dc->TextOut(10, y + 6, title);
    dc->SelectObject(old_font);

    y += 32;
}

void CInspectorWnd::DrawRow(CDC* dc, int& y, LPCTSTR label, const CString& value, COLORREF value_color, int width) {
    dc->SetBkMode(TRANSPARENT);

    dc->SetTextColor(kColorMuted);
    dc->TextOut(12, y, label);

    dc->SetTextColor(value_color);
    CSize value_size = dc->GetTextExtent(value);
    dc->TextOut(width - value_size.cx - 12, y, value);   // 값은 오른쪽 정렬

    y += 26;
}

void CInspectorWnd::OnPaint() {
    CPaintDC dc(this);

    CRect client_rect;
    GetClientRect(&client_rect);
    int width = client_rect.Width();

    // 더블 버퍼링: 화면 DC에 바로바로 그리면 그리는 중간 과정이 눈에 보여서 깜빡일 수 있어서,
    // 메모리 DC(화면 밖 비트맵)에 다 그린 다음 한 번에 통째로 복사(BitBlt)함
    CDC mem_dc;
    mem_dc.CreateCompatibleDC(&dc);
    CBitmap bitmap;
    bitmap.CreateCompatibleBitmap(&dc, client_rect.Width(), client_rect.Height());
    CBitmap* old_bitmap = mem_dc.SelectObject(&bitmap);

    mem_dc.FillSolidRect(&client_rect, kColorBackground);

    CFont font_label;
    font_label.CreateFont(20, 0, 0, 0, FW_NORMAL, 0, 0, 0,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH, _T("맑은 고딕"));
    CFont font_header;
    font_header.CreateFont(17, 0, 0, 0, FW_BOLD, 0, 0, 0,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH, _T("맑은 고딕"));

    int y = 8;
    DrawSection(&mem_dc, y, _T("통계"), kColorAccent, width, &font_header);

    CFont* old_font = mem_dc.SelectObject(&font_label);

    CString text;
    text.Format(_T("%d"), m_totalCount);
    DrawRow(&mem_dc, y, _T("전체 객체 수"), text, kColorWhite, width);

    text.Format(_T("%d"), m_visibleCount);
    DrawRow(&mem_dc, y, _T("렌더링 중"), text, kColorGreen, width);

    int32_t culled_count = m_totalCount - m_visibleCount;
    text.Format(_T("%d"), culled_count);
    DrawRow(&mem_dc, y, _T("컬링됨"), text, kColorYellow, width);

    text.Format(_T("%.1f"), m_fps);
    DrawRow(&mem_dc, y, _T("FPS"), text, kColorAccent, width);

    mem_dc.SelectObject(old_font);

    dc.BitBlt(0, 0, client_rect.Width(), client_rect.Height(), &mem_dc, 0, 0, SRCCOPY);

    mem_dc.SelectObject(old_bitmap);
}

void CInspectorWnd::UpdateStats(int32_t visible_count, int32_t total_count, float fps) {
    
    m_visibleCount = visible_count;
    m_totalCount = total_count;
    m_fps = fps;
    Invalidate(FALSE);   // 다시 그려달라고 요청만 함(배경은 어차피 안 지우니 FALSE로 충분)
}

void CInspectorWnd::OnToggleQuadTreeClicked() {
    m_showQuadTreeLevels = !m_showQuadTreeLevels;
    m_toggleQuadTreeButton.SetWindowText(
        m_showQuadTreeLevels ? _T("레벨 표시: ON") : _T("레벨 표시: OFF"));

    if (CShpViewerView* view = dynamic_cast<CShpViewerView*>(GetParent())) {
        view->SetShowQuadTreeLevels(m_showQuadTreeLevels);
    }
}

void CInspectorWnd::OnToggleAllNodesClicked() {
    m_showAllNodes = !m_showAllNodes;
    m_toggleAllNodesButton.SetWindowText(
        m_showAllNodes ? _T("전체 노드 보기: ON") : _T("전체 노드 보기: OFF"));

    if (CShpViewerView* view = dynamic_cast<CShpViewerView*>(GetParent())) {
        view->SetShowAllNodes(m_showAllNodes);
    }
}

void CInspectorWnd::OnToggleObjectColorClicked() {
    m_showAllObjectLevelColors = !m_showAllObjectLevelColors;
    m_toggleObjectColorButton.SetWindowText(
        m_showAllObjectLevelColors ? _T("객체 레벨 색상: ON") : _T("객체 레벨 색상: OFF"));

    if (CShpViewerView* view = dynamic_cast<CShpViewerView*>(GetParent())) {
        view->SetShowAllObjectLevelColors(m_showAllObjectLevelColors);
    }
}

void CInspectorWnd::OnToggleFillClicked() {
    m_showFill = !m_showFill;
    m_toggleFillButton.SetWindowText(
        m_showFill ? _T("채우기: ON") : _T("채우기: OFF"));

    if (CShpViewerView* view = dynamic_cast<CShpViewerView*>(GetParent())) {
        view->SetShowFill(m_showFill);
    }
}

void CInspectorWnd::OnToggleObjectBoundsClicked() {
    m_showObjectBounds = !m_showObjectBounds;
    m_toggleObjectBoundsButton.SetWindowText(
        m_showObjectBounds ? _T("객체 MBR: ON") : _T("객체 MBR: OFF"));

    if (CShpViewerView* view = dynamic_cast<CShpViewerView*>(GetParent())) {
        view->SetShowObjectBounds(m_showObjectBounds);
    }
}

void CInspectorWnd::OnToggle3DClicked() {
    m_show3D = !m_show3D;
    m_toggle3DButton.SetWindowText(
        m_show3D ? _T("3D: ON") : _T("3D: OFF"));

    if (CShpViewerView* view = dynamic_cast<CShpViewerView*>(GetParent())) {
        view->SetShow3D(m_show3D);
    }
}

void CInspectorWnd::OnToggleEdgesClicked() {
    m_showEdges = !m_showEdges;
    m_toggleEdgesButton.SetWindowText(
        m_showEdges ? _T("윤곽선: ON") : _T("윤곽선: OFF"));

    if (CShpViewerView* view = dynamic_cast<CShpViewerView*>(GetParent())) {
        view->SetShowEdges(m_showEdges);
    }
}