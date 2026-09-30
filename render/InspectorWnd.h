#pragma once
#include <afxwin.h>
#include <cstdint>
#include <vector>
#include <utility>

class CInspectorWnd : public CWnd
{
public:
    CInspectorWnd();
    virtual ~CInspectorWnd();

    enum ButtonId {
        kToggleQuadTree = 2001,
        kToggleAllNodes,          // 2002 - 앞 값 + 1이 자동으로 들어간다
        kToggleObjectColor,
        kToggleFill,
        kToggleObjectBounds,
        kToggle3D,
        kToggleEdges,
        kToggleOutline,
        kToggleTriangulationLines,
        kTogglePickRay,
        kEditReset,
        kToggleHidden,
        kRestoreAll,
    };

    enum Section {
        kSectionSelected = 0,   // 선택 객체 (E0-5에서 채운다)
        kSectionRender,         // 렌더
        kSectionQuadTree,       // 쿼드트리·컬링
        kSectionPicking,        // 피킹
        kSectionCount,          // ← 섹션 개수. 항목을 추가하면 자동으로 늘어난다
    };

    // 선택된 건물 하나를 인스펙터에 보여주기 위해 GLView가 채워서 건네주는 값.
    // 인스펙터가 ShpDataset을 직접 들여다보지 않게 하려고 중간에 두는 자료형이다.
    struct SelectionInfo {
        int32_t record_index = -1;      // -1이면 선택 없음
        float height = 0.0f;            // 지금 적용 중인 높이
        bool height_overridden = false; // 사용자가 편집한 값인지(.dbf 원본이 아닌지)
        bool hidden = false;            // 숨김 처리된 객체인지
        float rotate_y = 0.0f;          // 현재 회전각(라디안)
        float scale = 1.0f;             // 현재 배율
        std::vector<std::pair<CString, CString>> attributes;   // (필드명, 값)
    };

    BOOL Create(CWnd* parent_wnd);
    void UpdateStats(int32_t visible_count, int32_t total_count, float fps);
    void UpdateSelection(const SelectionInfo& info);
    void DrawLevel(CDC* dc, int& y, int width);

    // 배치 전용. 상태가 바뀔 때만 부른다(생성 / 창 크기 변경 / 섹션 접기·펴기).
    // OnPaint 안에서는 절대 부르지 않는다 - 그리는 중에 창을 움직이면 페인트가 꼬인다.
    void RelayoutPanel();

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
    CButton m_heightUpButton;
    CButton m_heightDownButton;
    CEdit   m_heightEdit;
    CButton m_heightApplyButton;
    CButton m_heightResetButton;
    CButton m_editResetButton;
    CButton m_toggleHiddenButton;
    CButton m_restoreAllButton;

    SelectionInfo m_selection;
    int m_selectionTextTop = -1;    // -1이면 선택 정보 텍스트를 그리지 않는다
    bool m_showPickRay = false;
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

    enum Column { kFull, kLeft, kRight };

    // 버튼과 입력칸을 함께 담아야 해서 공통 조상인 CWnd*로 받는다.
    // MoveWindow / ShowWindow 모두 CWnd의 멤버라 이걸로 충분하다.
    struct ControlEntry {
        CWnd* control;
        Section section;
        Column column;
        bool needs_selection;   // 선택된 객체가 있어야만 보이는 컨트롤인가
    };

    bool m_sectionExpanded[kSectionCount] = { true, true, false, false };
    CRect m_sectionHeaderRects[kSectionCount];

    int m_legendTop = -1;   // -1이면 범례를 그리지 않는다

    // 저장된 헤더 rect을 현재 창 폭에 맞춰 돌려준다.
    // 그리기(OnPaint)와 클릭 판정(OnLButtonDown)이 반드시 같은 영역을 보게 하려고 한 군데로 모았다.
    CRect GetSectionHeaderRect(int section_index) const;
    void DrawSectionHeader(CDC* dc, Section section, CFont* font);

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
    afx_msg void OnEditResetClicked();
    afx_msg void OnToggleHiddenClicked();
    afx_msg void OnRestoreAllClicked();
    afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
    afx_msg void OnSize(UINT nType, int cx, int cy);
    
};