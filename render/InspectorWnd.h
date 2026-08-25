#pragma once
#include <afxwin.h>
#include <cstdint>

namespace {
    constexpr int kToggleQuadTreeButtonId = 2001;
    constexpr int kToggleAllNodesButtonId = 2002;
    constexpr int kToggleObjectColorButtonId = 2003;
}

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
    float m_elapsedMs = 0.0f;

    CButton m_toggleQuadTreeButton;
    CButton m_toggleAllNodesButton;
    CButton m_toggleObjectColorButton;
    bool m_showQuadTreeLevels = false;
    bool m_showAllNodes = false;
    bool m_showAllObjectLevelColors = false;

    void DrawSection(CDC* dc, int& y, LPCTSTR title, COLORREF color, int width, CFont* font);
    void DrawRow(CDC* dc, int& y, LPCTSTR label, const CString& value, COLORREF value_color, int width);

    DECLARE_MESSAGE_MAP()
    afx_msg void OnPaint();
    afx_msg BOOL OnEraseBkgnd(CDC* pDC);
    afx_msg void OnToggleQuadTreeClicked();
    afx_msg void OnToggleAllNodesClicked();
    afx_msg void OnToggleObjectColorClicked();
};