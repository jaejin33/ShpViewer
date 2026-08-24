#pragma once
#include <afxwin.h>
#include <cstdint>

class CInspectorWnd : public CWnd {
public :
	CInspectorWnd();
	virtual ~CInspectorWnd();

	BOOL Create(CWnd* parent_wnd);
	void UpdateStats(int32_t visible_count, int32_t total_count);

protected:
	CStatic m_wndStatsLabel;

	DECLARE_MESSAGE_MAP()
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg HBRUSH OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor);
	afx_msg void OnSize(UINT nType, int cx, int cy);
};