#pragma once
#include "LevelColors.h"

class CInspectorView : public CView
{
    DECLARE_DYNCREATE(CInspectorView)
public:
    // ShpViewerView에서 이 값들을 업데이트해줌
    int   m_totalCount = 0;
    int   m_renderingCount = 0;
    float m_minObjectSize = 0.0f;
    float m_fps = 0.0f;
    bool  m_showLevelColors = true;
    bool  m_showItemMBR = false;
    bool  m_showNodeMBR = false;
    bool  m_showFrustum = false;

    // 버튼 클릭 콜백 (ShpViewerView로 전달)
    std::function<void(int)> m_onButtonClick;

    void Refresh() { Invalidate(FALSE); }

protected:
    virtual void OnDraw(CDC* pDC) override;
    DECLARE_MESSAGE_MAP()
    afx_msg BOOL OnEraseBkgnd(CDC* pDC);
    afx_msg void OnLButtonDown(UINT nFlags, CPoint point);

private:
    CRect m_btnRects[4];  // 버튼 4개 hit-test 영역
    void DrawSection(CDC* pDC, int& y, LPCTSTR title, COLORREF col, int w, CFont* pFont);
    void DrawRow(CDC* pDC, int& y, LPCTSTR label, CString value, COLORREF valCol, int w);
    void DrawToggleBtn(CDC* pDC, int& y, int idx, LPCTSTR label, bool on, COLORREF col, int w);
    void DrawLegend(CDC* pDC, int& y, int w);
};