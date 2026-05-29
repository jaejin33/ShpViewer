#include "pch.h"
#include "InspectorView.h"

IMPLEMENT_DYNCREATE(CInspectorView, CView)
BEGIN_MESSAGE_MAP(CInspectorView, CView)
    ON_WM_ERASEBKGND()
    ON_WM_LBUTTONDOWN()
END_MESSAGE_MAP()

// 색상 상수
static const COLORREF BG = RGB(18, 24, 38);
static const COLORREF BG2 = RGB(26, 36, 58);
static const COLORREF BORDER = RGB(42, 74, 112);
static const COLORREF WHITE = RGB(255, 255, 255);
static const COLORREF MUTED = RGB(140, 170, 200);
static const COLORREF ACCENT = RGB(0, 200, 255);
static const COLORREF GREEN = RGB(74, 222, 128);
static const COLORREF YELLOW = RGB(255, 220, 80);
static const COLORREF RED = RGB(255, 80, 80);
static const COLORREF ORANGE = RGB(255, 160, 0);

// depth별 색상 (LevelColors.h의 glm::vec4를 COLORREF로)
static const COLORREF LEVEL_COLORS[9] = {
    RGB(255, 51,  51),   // 0 red
    RGB(255, 153, 0),    // 1 orange
    RGB(255, 255, 0),    // 2 yellow
    RGB(51,  230, 51),   // 3 green
    RGB(0,   230, 230),  // 4 cyan
    RGB(77,  153, 255),  // 5 blue
    RGB(204, 51,  255),  // 6 purple
    RGB(255, 77,  179),  // 7 pink
    RGB(230, 230, 230),  // 8 white
};

BOOL CInspectorView::OnEraseBkgnd(CDC* pDC)
{
    return TRUE;
}

void CInspectorView::DrawSection(CDC* pDC, int& y, LPCTSTR title, COLORREF col, int w, CFont* pFont)
{
    // 섹션 헤더 배경
    CRect r(0, y, w, y + 24);
    pDC->FillSolidRect(&r, BG2);
    // 왼쪽 컬러 바
    pDC->FillSolidRect(CRect(0, y, 3, y + 24), col);
    // 텍스트
    pDC->SetTextColor(col);
    pDC->SetBkMode(TRANSPARENT);
    CFont* old = pDC->SelectObject(pFont);
    pDC->TextOut(10, y + 5, title);
    y += 28;
}

void CInspectorView::DrawRow(CDC* pDC, int& y, LPCTSTR label, CString value, COLORREF valCol, int w)
{
    pDC->SetBkMode(TRANSPARENT);
    pDC->SetTextColor(MUTED);
    pDC->TextOut(10, y, label);
    pDC->SetTextColor(valCol);
    pDC->TextOut(w - 80, y, value);
    y += 22;
}

void CInspectorView::DrawToggleBtn(CDC* pDC, int& y, int idx, LPCTSTR label, bool on, COLORREF col, int w)
{
    CRect r(8, y, w - 8, y + 28);
    m_btnRects[idx] = r;
    // 버튼 배경
    pDC->FillSolidRect(&r, on ? RGB(20, 50, 80) : BG2);
    // 테두리
    CPen pen(PS_SOLID, 1, on ? col : BORDER);
    CPen* oldPen = pDC->SelectObject(&pen);
    pDC->SelectStockObject(NULL_BRUSH);
    pDC->Rectangle(&r);
    pDC->SelectObject(oldPen);
    // 체크 표시
    pDC->SetTextColor(on ? col : MUTED);
    pDC->SetBkMode(TRANSPARENT);
    pDC->TextOut(r.left + 8, r.top + 6, on ? _T("[ON]") : _T("[OFF]"));
    pDC->SetTextColor(on ? WHITE : MUTED);
    pDC->TextOut(r.left + 52, r.top + 6, label);
    y += 34;
}

void CInspectorView::DrawLegend(CDC* pDC, int& y, int w)
{
    static const LPCTSTR names[9] = {
        _T("depth 0"), _T("depth 1"), _T("depth 2"),
        _T("depth 3"), _T("depth 4"), _T("depth 5"),
        _T("depth 6"), _T("depth 7"), _T("depth 8 (leaf)"),
    };
    for (int i = 0; i < 9; i++) {
        // 색상 사각형
        pDC->FillSolidRect(CRect(10, y + 3, 24, y + 17), LEVEL_COLORS[i]);
        // 이름
        pDC->SetTextColor(MUTED);
        pDC->SetBkMode(TRANSPARENT);
        pDC->TextOut(30, y, names[i]);
        y += 22;
    }
}

void CInspectorView::OnDraw(CDC* pDC)
{
    CRect client; GetClientRect(&client);
    int w = client.Width();
    int y = 8;

    // ── 더블 버퍼링 ──
    CDC memDC;
    memDC.CreateCompatibleDC(pDC);
    CBitmap bitmap;
    bitmap.CreateCompatibleBitmap(pDC, client.Width(), client.Height());
    CBitmap* pOldBmp = memDC.SelectObject(&bitmap);

    // 배경
    memDC.FillSolidRect(&client, BG);

    // 폰트 설정
    CFont font;
    font.CreateFont(14, 0, 0, 0, FW_NORMAL, 0, 0, 0,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH, _T("Consolas"));
    CFont fontBold;
    fontBold.CreateFont(14, 0, 0, 0, FW_BOLD, 0, 0, 0,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH, _T("Consolas"));
    CFont* oldFont = memDC.SelectObject(&font);

    // ── Statistics ──
    DrawSection(&memDC, y, _T("Statistics"), ACCENT, w, &fontBold);
    memDC.SelectObject(&font);

    CString s;
    s.Format(_T("%d"), m_totalCount);
    DrawRow(&memDC, y, _T("Total"), s, WHITE, w);
    s.Format(_T("%d"), m_renderingCount);
    DrawRow(&memDC, y, _T("Render"), s, GREEN, w);
    s.Format(_T(">= %.1f"), m_minObjectSize);
    DrawRow(&memDC, y, _T("LOD"), s, YELLOW, w);
    COLORREF fpsCol = m_fps >= 30 ? GREEN : m_fps >= 15 ? YELLOW : RED;
    s.Format(_T("%.0f"), m_fps);
    DrawRow(&memDC, y, _T("FPS"), s, fpsCol, w);
    y += 8;

    // ── Display ──
    DrawSection(&memDC, y, _T("Display"), ORANGE, w, &fontBold);
    memDC.SelectObject(&font);
    DrawToggleBtn(&memDC, y, 0, _T("Level Color [1]"), m_showLevelColors, ACCENT, w);
    DrawToggleBtn(&memDC, y, 1, _T("Item MBR   [2]"), m_showItemMBR, GREEN, w);
    DrawToggleBtn(&memDC, y, 2, _T("Node MBR   [3]"), m_showNodeMBR, ORANGE, w);
    DrawToggleBtn(&memDC, y, 3, _T("Frustum    [4]"), m_showFrustum, YELLOW, w);
    y += 8;

    // ── Level Colors ──
    DrawSection(&memDC, y, _T("Level Colors"), YELLOW, w, &fontBold);
    memDC.SelectObject(&font);
    DrawLegend(&memDC, y, w);
    y += 8;

    // ── Controls ──
    DrawSection(&memDC, y, _T("Controls"), MUTED, w, &fontBold);
    memDC.SelectObject(&font);

    struct { LPCTSTR key; LPCTSTR desc; } controls[] = {
        { _T("W/S"),   _T("Forward/Back") },
        { _T("A/D"),   _T("Left/Right")   },
        { _T("Q/E"),   _T("Yaw")          },
        { _T("R/F"),   _T("Pitch")        },
        { _T("Wheel"), _T("Zoom")         },
        { _T("Drag"),  _T("Pan")          },
    };
    for (auto& c : controls) {
        memDC.SetTextColor(ACCENT);
        memDC.SetBkMode(TRANSPARENT);
        memDC.TextOut(10, y, c.key);
        memDC.SetTextColor(MUTED);
        memDC.TextOut(70, y, c.desc);
        y += 22;
    }

    memDC.SelectObject(oldFont);

    // ── 한 번에 화면에 복사 ──
    pDC->BitBlt(0, 0, client.Width(), client.Height(), &memDC, 0, 0, SRCCOPY);
    memDC.SelectObject(pOldBmp);
}

void CInspectorView::OnLButtonDown(UINT nFlags, CPoint point)
{
    for (int i = 0; i < 4; i++) {
        if (m_btnRects[i].PtInRect(point)) {
            if (m_onButtonClick) m_onButtonClick(i);
            return;
        }
    }
    CView::OnLButtonDown(nFlags, point);
}