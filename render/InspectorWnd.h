#pragma once
#include <afxwin.h>
#include <cstdint>

class CInspectorWnd : public CWnd
{
public:
    CInspectorWnd();
    virtual ~CInspectorWnd();

    BOOL Create(CWnd* parent_wnd);
    void UpdateStats(int32_t visible_count, int32_t total_count);

protected:
    int32_t m_visibleCount = 0;
    int32_t m_totalCount = 0;

    void DrawSection(CDC* dc, int& y, LPCTSTR title, COLORREF color, int width, CFont* font);
    void DrawRow(CDC* dc, int& y, LPCTSTR label, const CString& value, COLORREF value_color, int width);

    DECLARE_MESSAGE_MAP()
    afx_msg void OnPaint();
    afx_msg BOOL OnEraseBkgnd(CDC* pDC);
};