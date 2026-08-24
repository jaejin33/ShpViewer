#include "pch.h"
#include "InspectorWnd.h"

namespace {
    constexpr UINT kStatsLabelId = 2001;
}

CInspectorWnd::CInspectorWnd() {}
CInspectorWnd::~CInspectorWnd() {}

BEGIN_MESSAGE_MAP(CInspectorWnd, CWnd)
    ON_WM_CREATE()
    ON_WM_CTLCOLOR()
    ON_WM_SIZE()
END_MESSAGE_MAP()

BOOL CInspectorWnd::Create(CWnd* parent_wnd) {
    return CWnd::Create(nullptr, _T(""), WS_CHILD | WS_VISIBLE,
        CRect(0, 0, 0, 0), parent_wnd, 0);
}

int CInspectorWnd::OnCreate(LPCREATESTRUCT lpCreateStruct) {
    if (CWnd::OnCreate(lpCreateStruct) == -1) return -1;

    m_wndStatsLabel.Create(_T(""), WS_CHILD | WS_VISIBLE | SS_LEFT,
        CRect(0, 0, 0, 0), this, kStatsLabelId);

    return 0;
}

HBRUSH CInspectorWnd::OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor) {
    HBRUSH hbr = CWnd::OnCtlColor(pDC, pWnd, nCtlColor);

    if (pWnd->GetDlgCtrlID() == kStatsLabelId) {
        pDC->SetTextColor(RGB(255, 255, 0));
        pDC->SetBkColor(RGB(0, 0, 0));

        static CBrush s_labelBrush(RGB(0, 0, 0));
        return (HBRUSH)s_labelBrush;
    }

    return hbr;
}

void CInspectorWnd::UpdateStats(int32_t visible_count, int32_t total_count) {
    CString text;
    text.Format(_T("렌더링 중 객체 수: %d\r\n전체 객체 수: %d"), visible_count, total_count);
    m_wndStatsLabel.SetWindowText(text);
}

void CInspectorWnd::OnSize(UINT nType, int cx, int cy) {
    CWnd::OnSize(nType, cx, cy);
    if (::IsWindow(m_wndStatsLabel.GetSafeHwnd())) {
        m_wndStatsLabel.MoveWindow(0, 0, cx, cy);
    }
}