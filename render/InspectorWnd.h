#pragma once
#include <afxwin.h>
#include <cstdint>

namespace {
    constexpr int kToggleQuadTreeButtonId = 2001;
    constexpr int kToggleAllNodesButtonId = 2002;
    constexpr int kToggleObjectColorButtonId = 2003;
    constexpr int kToggleFillButtonId = 2004;
    constexpr int kToggleObjectBoundsButtonId = 2005;
    constexpr int kToggle3DButtonId = 2006;
    constexpr int kToggleEdgesButtonId = 2007;
    constexpr int kToggleOutlineButtonId = 2008;
    constexpr int kToggleTriangulationLinesButtonId = 2009;
    constexpr int kTogglePickRayButtonId = 2010;
}

class CInspectorWnd : public CWnd
{
public:
    CInspectorWnd();
    virtual ~CInspectorWnd();

    BOOL Create(CWnd* parent_wnd);
    void UpdateStats(int32_t visible_count, int32_t total_count, float fps);
    void DrawLevel(CDC* dc, int& y, int width);
    int LayoutButtons(int start_y);

protected:
    int32_t m_visibleCount = 0;
    int32_t m_totalCount = 0;
    float m_elapsedMs = 0.0f;
    float m_fps = 0.0f;

    CButton m_toggleQuadTreeButton;
    CButton m_toggleAllNodesButton;
    CButton m_toggleObjectColorButton;
    CButton m_toggleFillButton;
    CButton m_toggleObjectBoundsButton;
    CButton m_toggle3DButton;
    CButton m_toggleEdgesButton;
    CButton m_toggleOutlineButton;
    CButton m_toggleTriangulationLinesButton;
    CButton m_togglePickRayButton;
    bool m_showPickRay = true;
    bool m_showFill = false;
    bool m_showQuadTreeLevels = false;
    bool m_showAllNodes = false;
    bool m_showAllObjectLevelColors = false;
    bool m_showObjectBounds = false;
    bool m_show3D = true;
    bool m_showEdges = true;
    bool m_showObjectOutline = false;
    bool m_showTriangulationLines = false;

    void DrawSection(CDC* dc, int& y, LPCTSTR title, COLORREF color, int width, CFont* font);
    void DrawRow(CDC* dc, int& y, LPCTSTR label, const CString& value, COLORREF value_color, int width);

    DECLARE_MESSAGE_MAP()
    afx_msg void OnPaint();
    afx_msg BOOL OnEraseBkgnd(CDC* pDC);
    afx_msg void OnToggleQuadTreeClicked();
    afx_msg void OnToggleAllNodesClicked();
    afx_msg void OnToggleObjectColorClicked();
    afx_msg void OnToggleFillClicked();
    afx_msg void OnToggleObjectBoundsClicked();
    afx_msg void OnToggle3DClicked();
    afx_msg void OnToggleEdgesClicked();
    afx_msg void OnToggleOutlineClicked();
    afx_msg void OnToggleTriangulationLinesClicked();
    afx_msg void OnTogglePickRayClicked();
};