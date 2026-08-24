#include "pch.h"
#include "InspectorWnd.h"

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
END_MESSAGE_MAP()

BOOL CInspectorWnd::Create(CWnd* parent_wnd) {
    return CWnd::Create(nullptr, _T(""), WS_CHILD | WS_VISIBLE,
        CRect(0, 0, 0, 0), parent_wnd, 0);
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

    float culled_ratio = (m_totalCount > 0)
        ? static_cast<float>(culled_count) / static_cast<float>(m_totalCount) * 100.0f
        : 0.0f;
    text.Format(_T("%.1f%%"), culled_ratio);
    DrawRow(&mem_dc, y, _T("컬링 비율"), text, kColorAccent, width);

    mem_dc.SelectObject(old_font);

    dc.BitBlt(0, 0, client_rect.Width(), client_rect.Height(), &mem_dc, 0, 0, SRCCOPY);

    mem_dc.SelectObject(old_bitmap);
}

void CInspectorWnd::UpdateStats(int32_t visible_count, int32_t total_count) {
    m_visibleCount = visible_count;
    m_totalCount = total_count;
    Invalidate(FALSE);   // 다시 그려달라고 요청만 함(배경은 어차피 안 지우니 FALSE로 충분)
}