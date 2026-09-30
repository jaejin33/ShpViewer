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

    constexpr COLORREF kLevelColorsRGB[11] = {
        RGB(152, 66, 96),   // depth 0
        RGB(59, 170, 24),   // depth 1
        RGB(3, 63, 254),    // depth 2
        RGB(42, 155, 203),  // depth 3
        RGB(162, 91, 255),  // depth 4
        RGB(49, 42, 249),   // depth 5
        RGB(0, 129, 255),   // depth 6  
        RGB(46, 210, 191),  // depth 7
        RGB(16, 156, 0),    // depth 8 
        RGB(230, 215, 62),  // depth 9
        RGB(242, 130, 59),   // depth 10 
    };

    constexpr int kStatsBottom = 8 + 32 + 26 * 4 + 10;
    constexpr int kHeaderHeight = 28;
    constexpr int kButtonRowHeight = 40;   // 버튼 한 줄이 차지하는 높이(버튼 + 아래 여백)
    constexpr int kButtonHeight = 30;
    constexpr int kSectionGap = 8;
    constexpr int kInfoRowHeight = 26;     // DrawRow 한 줄 높이와 같아야 한다
    constexpr float kPi = 3.14159265358979323846f;
    constexpr int kPanelWidth = 220;       // 창 폭을 아직 모를 때 쓰는 기본값

    // 토글 버튼 이름 - 만들 때와 누를 때가 반드시 같은 문자열을 쓰도록 여기 한 군데에만 둔다
    const TCHAR* const kNameQuadTree          = _T("레벨 표시");
    const TCHAR* const kNameAllNodes          = _T("전체 노드 보기");
    const TCHAR* const kNameObjectColor       = _T("객체 레벨 색상");
    const TCHAR* const kNameObjectBounds      = _T("객체 MBR");
    const TCHAR* const kNameTriangulationLine = _T("삼각분할 선");
    const TCHAR* const kNameFill              = _T("채우기");
    const TCHAR* const kName3D                = _T("3D");
    const TCHAR* const kNameEdges             = _T("윤곽선");
    const TCHAR* const kNameOutline           = _T("객체 테두리");
    const TCHAR* const kNamePickRay           = _T("피킹 레이");

    // "이름: ON" / "이름: OFF" 를 만든다. 라벨 문자열이 만들어지는 유일한 곳.
    CString MakeToggleLabel(const TCHAR* name, bool on) {
        CString text;
        text.Format(_T("%s: %s"), name, on ? _T("ON") : _T("OFF"));
        return text;
    }

    struct SectionDef {
        LPCTSTR title;
        COLORREF color;
    };

    // CInspectorWnd::Section 순서와 반드시 일치해야 한다
    const SectionDef kSectionDefs[] = {
        { _T("객체 편집"),      RGB(255, 140, 140) },
        { _T("렌더"),           kColorAccent },
        { _T("쿼드트리·컬링"),  kColorGreen },
        { _T("피킹"),           kColorYellow },
    };

    // Section enum에 항목을 추가하고 kSectionDefs를 안 고치면 여기서 빌드가 멈춘다.
    static_assert(_countof(kSectionDefs) == CInspectorWnd::kSectionCount,
                  "kSectionDefs와 Section enum의 개수가 다릅니다");
}
CInspectorWnd::CInspectorWnd() {}
CInspectorWnd::~CInspectorWnd() {}

BEGIN_MESSAGE_MAP(CInspectorWnd, CWnd)
    ON_WM_PAINT()
    ON_WM_ERASEBKGND()
    ON_BN_CLICKED(kToggleQuadTree, &CInspectorWnd::OnToggleQuadTreeClicked)
    ON_BN_CLICKED(kToggleAllNodes, &CInspectorWnd::OnToggleAllNodesClicked)
    ON_BN_CLICKED(kToggleObjectColor, &CInspectorWnd::OnToggleObjectColorClicked)
    ON_BN_CLICKED(kToggleFill, &CInspectorWnd::OnToggleFillClicked)
    ON_BN_CLICKED(kToggleObjectBounds, &CInspectorWnd::OnToggleObjectBoundsClicked)
    ON_BN_CLICKED(kToggle3D, &CInspectorWnd::OnToggle3DClicked)
    ON_BN_CLICKED(kToggleEdges, &CInspectorWnd::OnToggleEdgesClicked)
    ON_BN_CLICKED(kToggleOutline, &CInspectorWnd::OnToggleOutlineClicked)
    ON_BN_CLICKED(kToggleTriangulationLines, &CInspectorWnd::OnToggleTriangulationLinesClicked)   
    ON_BN_CLICKED(kTogglePickRay, &CInspectorWnd::OnTogglePickRayClicked)
    ON_BN_CLICKED(kEditReset, &CInspectorWnd::OnEditResetClicked)
    ON_BN_CLICKED(kToggleHidden, &CInspectorWnd::OnToggleHiddenClicked)
    ON_BN_CLICKED(kRestoreAll, &CInspectorWnd::OnRestoreAllClicked)
    ON_WM_SIZE()
    ON_WM_LBUTTONDOWN()
END_MESSAGE_MAP()

BOOL CInspectorWnd::Create(CWnd* parent_wnd) {
    if (!CWnd::Create(nullptr, _T(""), WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN,
        CRect(0, 0, 0, 0), parent_wnd, 0)) {
        return FALSE;
    }

    // 위치는 RelayoutPanel이 정한다. 여기서는 0으로 두고 만들기만 한다.
    auto make_button = [this](CButton& btn, LPCTSTR label, ButtonId id) {
        btn.Create(label, WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            CRect(0, 0, 0, 0), this, id);
        };

    // 토글은 라벨을 직접 넘기지 않는다. 이름과 현재 상태만 주면 라벨은 MakeToggleLabel이 만든다.
    auto make_toggle = [this](CButton& btn, const TCHAR* name, bool on, ButtonId id) {
        btn.Create(MakeToggleLabel(name, on), WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            CRect(0, 0, 0, 0), this, id);
        };

    make_toggle(m_toggleQuadTreeButton,           kNameQuadTree,          m_showQuadTreeLevels,       kToggleQuadTree);
    make_toggle(m_toggleAllNodesButton,           kNameAllNodes,          m_showAllNodes,             kToggleAllNodes);
    make_toggle(m_toggleObjectColorButton,        kNameObjectColor,       m_showAllObjectLevelColors, kToggleObjectColor);
    make_toggle(m_toggleObjectBoundsButton,       kNameObjectBounds,      m_showObjectBounds,         kToggleObjectBounds);
    make_toggle(m_toggleTriangulationLinesButton, kNameTriangulationLine, m_showTriangulationLines,   kToggleTriangulationLines);
    make_toggle(m_toggleFillButton,               kNameFill,              m_showFill,                 kToggleFill);
    make_toggle(m_toggle3DButton,                 kName3D,                m_show3D,                   kToggle3D);
    make_toggle(m_toggleEdgesButton,              kNameEdges,             m_showEdges,                kToggleEdges);
    make_toggle(m_toggleOutlineButton,            kNameOutline,           m_showObjectOutline,        kToggleOutline);
    make_toggle(m_togglePickRayButton,            kNamePickRay,           m_showPickRay,              kTogglePickRay);

    make_button(m_editResetButton,    _T("편집 원래대로"), kEditReset);
    make_button(m_toggleHiddenButton, _T("숨기기"),        kToggleHidden);
    make_button(m_restoreAllButton,   _T("전체 보이기"),   kRestoreAll);

    RelayoutPanel();
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
    const int width = client_rect.Width();
    if (width <= 0 || client_rect.Height() <= 0) return;   // 창이 아직 크기를 못 받은 상태

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

    CFont* old_font = mem_dc.SelectObject(&font_header);

    // ── 통계 (항상 맨 위, 접히지 않음)
    int y = 8;
    DrawSection(&mem_dc, y, _T("통계"), kColorAccent, width, &font_header);

    mem_dc.SelectObject(&font_label);

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

    // ── 섹션 헤더들 (위치는 RelayoutPanel이 정해둔 것을 읽기만 한다)
    for (int s = 0; s < kSectionCount; ++s) {
        DrawSectionHeader(&mem_dc, static_cast<Section>(s), &font_header);
    }

    // ── 선택 객체 정보 (선택 객체 섹션이 펼쳐져 있을 때만)
    if (m_selectionTextTop >= 0) {
        int info_y = m_selectionTextTop;
        mem_dc.SelectObject(&font_label);

        if (m_selection.record_index < 0) {
            mem_dc.SetBkMode(TRANSPARENT);
            mem_dc.SetTextColor(kColorMuted);
            mem_dc.TextOut(12, info_y, _T("선택된 객체 없음"));
        }
        else {
            CString value;
            value.Format(_T("#%d"), m_selection.record_index);
            DrawRow(&mem_dc, info_y, _T("레코드"), value, kColorWhite, width);

            DrawRow(&mem_dc, info_y, _T("상태"),
                m_selection.hidden ? _T("숨김") : _T("표시"),
                m_selection.hidden ? kColorYellow : kColorGreen, width);

            for (const auto& pair : m_selection.attributes) {
                DrawRow(&mem_dc, info_y, pair.first, pair.second, kColorMuted, width);
            }
        }
    }

    // ── 색상범례표 (쿼드트리 섹션이 펼쳐져 있을 때만)
    if (m_legendTop >= 0) {
        int legend_y = m_legendTop;
        mem_dc.SelectObject(&font_header);
        mem_dc.SetBkMode(TRANSPARENT);
        mem_dc.SetTextColor(kColorAccent);
        mem_dc.TextOut(12, legend_y, _T("색상범례표"));
        legend_y += 30;

        mem_dc.SelectObject(&font_label);
        DrawLevel(&mem_dc, legend_y, width);
    }

    // 메모리 비트맵을 화면으로 한 번에 옮긴다. 이 줄이 없으면 아무것도 안 보인다.
    dc.BitBlt(0, 0, client_rect.Width(), client_rect.Height(), &mem_dc, 0, 0, SRCCOPY);

    // 지역 CFont / CBitmap이 소멸하기 전에 DC에서 떼어낸다 (GDI 객체 누수 방지)
    mem_dc.SelectObject(old_font);
    mem_dc.SelectObject(old_bitmap);
}

void CInspectorWnd::DrawLevel(CDC* dc, int& y, int width) {
    constexpr int kLevelCount = 11;
    constexpr int kSwatchSize = 14;
    constexpr int kSwatchWidth = 80;
    constexpr int kSwatchGap = 4;    // 숫자 라벨과 색상 사이 간격
    constexpr int kLegendRowHeight = 20;
    constexpr int kLeftMargin = 12;

    CFont font_num;
    font_num.CreateFont(15, 0, 0, 0, FW_NORMAL, 0, 0, 0,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH, _T("맑은 고딕"));
    CFont* old_font = dc->SelectObject(&font_num);
    dc->SetBkMode(TRANSPARENT);
    dc->SetTextColor(kColorMuted);

    // "13 :" 처럼 가장 넓은 라벨 기준으로 칸 폭과 열 개수를 계산
    CSize max_label_size = dc->GetTextExtent(_T("13 :"));
    for (int depth = 0; depth < kLevelCount; ++depth) {
        int row_y = y + depth * kLegendRowHeight;

        CString label;
        label.Format(_T("%d :"), depth);
        dc->TextOut(kLeftMargin, row_y, label);

        CRect swatch_rect(
            kLeftMargin + max_label_size.cx + kSwatchGap, row_y + 2,
            kLeftMargin + max_label_size.cx + kSwatchGap + kSwatchWidth, row_y + 2 + kSwatchSize);
        dc->FillSolidRect(&swatch_rect, kLevelColorsRGB[depth]);
    }

    dc->SelectObject(old_font);
    y += kLevelCount * kLegendRowHeight + 4;
}

void CInspectorWnd::RelayoutPanel() {
    // 각 버튼이 어느 섹션의 어느 칸에 들어가는지. 순서가 곧 화면에 놓이는 순서다.
    // kLeft / kRight가 짝을 이루면 한 줄에 둘이 나란히 놓인다.
    const ControlEntry entries[] = {
        { &m_editResetButton,                kSectionSelected, kFull,  true  },
        { &m_toggleHiddenButton,             kSectionSelected, kFull,  true  },
        // 선택이 없어도 눌러야 한다 - 숨긴 객체는 다시 클릭할 수 없으니까
        { &m_restoreAllButton,               kSectionSelected, kFull,  false },

        { &m_toggle3DButton,                 kSectionRender,   kLeft,  false },
        { &m_toggleEdgesButton,              kSectionRender,   kRight, false },
        { &m_toggleFillButton,               kSectionRender,   kFull,  false },
        { &m_toggleOutlineButton,            kSectionRender,   kFull,  false },
        { &m_toggleTriangulationLinesButton, kSectionRender,   kFull,  false },

        { &m_toggleQuadTreeButton,           kSectionQuadTree, kFull,  false },
        { &m_toggleAllNodesButton,           kSectionQuadTree, kFull,  false },
        { &m_toggleObjectColorButton,        kSectionQuadTree, kFull,  false },
        { &m_toggleObjectBoundsButton,       kSectionQuadTree, kFull,  false },

        { &m_togglePickRayButton,            kSectionPicking,  kFull,  false },
    };

    // CWnd::Create 도중에도 WM_SIZE가 한 번 날아온다. 그때는 버튼이 아직 없으므로
    // 창/버튼 핸들이 없으면 조용히 빠진다 - Create 끝에서 다시 부르니 문제없다.
    if (!GetSafeHwnd()) return;

    CRect client_rect;
    GetClientRect(&client_rect);
    const int width = (client_rect.Width() > 0) ? client_rect.Width() : kPanelWidth;

    int y = kStatsBottom;
    m_legendTop = -1;
    m_selectionTextTop = -1;

    for (int s = 0; s < kSectionCount; ++s) {
        const Section section = static_cast<Section>(s);

        // 1) 헤더 자리를 잡아 기억해둔다 (OnPaint와 OnLButtonDown이 이 값을 함께 쓴다)
        m_sectionHeaderRects[s] = CRect(0, y, width, y + kHeaderHeight);
        y += kHeaderHeight + 4;

        const bool expanded = m_sectionExpanded[s];

        // 2) 선택 객체 섹션은 버튼들보다 먼저 정보 텍스트가 온다 - 그 자리를 먼저 비워둔다
        if (section == kSectionSelected && expanded) {
            m_selectionTextTop = y;
            const int row_count = (m_selection.record_index >= 0)
                ? 2 + static_cast<int>(m_selection.attributes.size())   // 번호 + 상태 + 속성들
                : 1;                                                    // "선택된 객체 없음"
            y += row_count * kInfoRowHeight + 6;
        }

        // 3) 이 섹션에 속한 컨트롤들
        // 선택된 게 없으면 편집 컨트롤은 보여줄 이유가 없다
        const bool has_selection = (m_selection.record_index >= 0);

        for (const ControlEntry& entry : entries) {
            if (entry.section != section) continue;
            if (!entry.control->GetSafeHwnd()) continue;   // 아직 만들어지지 않은 컨트롤

            const bool visible = expanded && (has_selection || !entry.needs_selection);

            if (!visible) {
                entry.control->ShowWindow(SW_HIDE);
                continue;                       // 숨긴 것은 자리를 차지하지 않는다
            }

            entry.control->ShowWindow(SW_SHOW);
            switch (entry.column) {
            case kFull:
                entry.control->MoveWindow(CRect(10, y, 210, y + kButtonHeight));
                y += kButtonRowHeight;
                break;
            case kLeft:
                entry.control->MoveWindow(CRect(10, y, 108, y + kButtonHeight));
                break;                          // 오른쪽 짝이 놓일 때까지 y를 올리지 않는다
            case kRight:
                entry.control->MoveWindow(CRect(112, y, 210, y + kButtonHeight));
                y += kButtonRowHeight;
                break;
            }
        }

        // 4) 색상범례표는 쿼드트리 섹션 소속 - 그 섹션이 펼쳐졌을 때만 자리를 잡는다
        if (section == kSectionQuadTree && expanded) {
            m_legendTop = y + 10;
            y = m_legendTop + 30 + 11 * 20 + 10;   // 제목 30 + 색상 11줄 + 여백
        }

        y += kSectionGap;
    }

    Invalidate(FALSE);
}

CRect CInspectorWnd::GetSectionHeaderRect(int section_index) const {
    CRect rect = m_sectionHeaderRects[section_index];

    // 저장 시점 이후 창 폭이 달라졌을 수 있으므로 현재 폭으로 맞춘다.
    // 그리는 영역과 클릭 판정 영역이 어긋나지 않게 하려고 이 한 군데서만 계산한다.
    CRect client_rect;
    GetClientRect(&client_rect);
    if (client_rect.Width() > 0) {
        rect.right = client_rect.Width();
    }
    return rect;
}

void CInspectorWnd::DrawSectionHeader(CDC* dc, Section section, CFont* font) {
    const int s = static_cast<int>(section);
    const CRect rect = GetSectionHeaderRect(s);
    const SectionDef& def = kSectionDefs[s];

    dc->FillSolidRect(&rect, kColorSectionBg);
    dc->FillSolidRect(CRect(rect.left, rect.top, rect.left + 3, rect.bottom), def.color);

    CFont* old_font = dc->SelectObject(font);
    dc->SetBkMode(TRANSPARENT);
    dc->SetTextColor(def.color);

    // 펼쳐져 있으면 아래쪽 삼각형, 접혀 있으면 오른쪽 삼각형
    CString title;
    title.Format(_T("%s  %s"), m_sectionExpanded[s] ? _T("\u25be") : _T("\u25b8"), def.title);
    dc->TextOut(10, rect.top + 6, title);

    dc->SelectObject(old_font);
}

void CInspectorWnd::UpdateStats(int32_t visible_count, int32_t total_count, float fps) {
    
    m_visibleCount = visible_count;
    m_totalCount = total_count;
    m_fps = fps;
    Invalidate(FALSE);   // 다시 그려달라고 요청만 함(배경은 어차피 안 지우니 FALSE로 충분)
}

void CInspectorWnd::UpdateSelection(const SelectionInfo& info) {
    m_selection = info;

    if (m_toggleHiddenButton.GetSafeHwnd()) {
        m_toggleHiddenButton.SetWindowText(info.hidden ? _T("보이기") : _T("숨기기"));
    }


    // 속성 줄 수가 바뀌므로 아래 섹션들의 위치가 전부 달라진다 -> 다시 배치
    RelayoutPanel();
}

void CInspectorWnd::OnEditResetClicked() {
    if (m_selection.record_index < 0) return;
    if (CShpViewerView* view = dynamic_cast<CShpViewerView*>(GetParent())) {
        view->ResetSelectedRecordEdit();
    }
}

void CInspectorWnd::OnToggleHiddenClicked() {
    if (m_selection.record_index < 0) return;

    if (CShpViewerView* view = dynamic_cast<CShpViewerView*>(GetParent())) {
        view->ToggleSelectedRecordHidden();
    }
}

void CInspectorWnd::OnRestoreAllClicked() {
    // 숨김만 해제한다. 높이 편집은 [높이 원래대로]가 따로 담당한다.
    if (CShpViewerView* view = dynamic_cast<CShpViewerView*>(GetParent())) {
        view->RestoreAllRecords();
    }
}

void CInspectorWnd::OnToggleQuadTreeClicked() {
    m_showQuadTreeLevels = !m_showQuadTreeLevels;
    m_toggleQuadTreeButton.SetWindowText(MakeToggleLabel(kNameQuadTree, m_showQuadTreeLevels));

    if (CShpViewerView* view = dynamic_cast<CShpViewerView*>(GetParent())) {
        view->SetShowQuadTreeLevels(m_showQuadTreeLevels);
    }
}

void CInspectorWnd::OnToggleAllNodesClicked() {
    m_showAllNodes = !m_showAllNodes;
    m_toggleAllNodesButton.SetWindowText(MakeToggleLabel(kNameAllNodes, m_showAllNodes));

    if (CShpViewerView* view = dynamic_cast<CShpViewerView*>(GetParent())) {
        view->SetShowAllNodes(m_showAllNodes);
    }
}

void CInspectorWnd::OnToggleObjectColorClicked() {
    m_showAllObjectLevelColors = !m_showAllObjectLevelColors;
    m_toggleObjectColorButton.SetWindowText(MakeToggleLabel(kNameObjectColor, m_showAllObjectLevelColors));

    if (CShpViewerView* view = dynamic_cast<CShpViewerView*>(GetParent())) {
        view->SetShowAllObjectLevelColors(m_showAllObjectLevelColors);
    }
}

void CInspectorWnd::OnToggleFillClicked() {
    m_showFill = !m_showFill;
    m_toggleFillButton.SetWindowText(MakeToggleLabel(kNameFill, m_showFill));

    if (CShpViewerView* view = dynamic_cast<CShpViewerView*>(GetParent())) {
        view->SetShowFill(m_showFill);
    }
}

void CInspectorWnd::OnToggleObjectBoundsClicked() {
    m_showObjectBounds = !m_showObjectBounds;
    m_toggleObjectBoundsButton.SetWindowText(MakeToggleLabel(kNameObjectBounds, m_showObjectBounds));

    if (CShpViewerView* view = dynamic_cast<CShpViewerView*>(GetParent())) {
        view->SetShowObjectBounds(m_showObjectBounds);
    }
}

void CInspectorWnd::OnToggle3DClicked() {
    m_show3D = !m_show3D;
    m_toggle3DButton.SetWindowText(MakeToggleLabel(kName3D, m_show3D));

    if (CShpViewerView* view = dynamic_cast<CShpViewerView*>(GetParent())) {
        view->SetShow3D(m_show3D);
    }
}

void CInspectorWnd::OnToggleEdgesClicked() {
    m_showEdges = !m_showEdges;
    m_toggleEdgesButton.SetWindowText(MakeToggleLabel(kNameEdges, m_showEdges));

    if (CShpViewerView* view = dynamic_cast<CShpViewerView*>(GetParent())) {
        view->SetShowEdges(m_showEdges);
    }
}

void CInspectorWnd::OnToggleOutlineClicked() {
    m_showObjectOutline = !m_showObjectOutline;
    m_toggleOutlineButton.SetWindowText(MakeToggleLabel(kNameOutline, m_showObjectOutline));

    if (CShpViewerView* view = dynamic_cast<CShpViewerView*>(GetParent())) {
        view->SetShowObjectOutline(m_showObjectOutline);
    }
}

void CInspectorWnd::OnToggleTriangulationLinesClicked() {
    m_showTriangulationLines = !m_showTriangulationLines;
    m_toggleTriangulationLinesButton.SetWindowText(MakeToggleLabel(kNameTriangulationLine, m_showTriangulationLines));

    if (CShpViewerView* view = dynamic_cast<CShpViewerView*>(GetParent())) {
        view->SetShowTriangulationLines(m_showTriangulationLines);
    }
}

void CInspectorWnd::OnTogglePickRayClicked() {
    m_showPickRay = !m_showPickRay;
    m_togglePickRayButton.SetWindowText(MakeToggleLabel(kNamePickRay, m_showPickRay));

    if (CShpViewerView* view = dynamic_cast<CShpViewerView*>(GetParent())) {
        view->SetShowPickRay(m_showPickRay);
    }
}

void CInspectorWnd::OnSize(UINT nType, int cx, int cy)
{
    CWnd::OnSize(nType, cx, cy);
    RelayoutPanel();
}

void CInspectorWnd::OnLButtonDown(UINT nFlags, CPoint point) {
    // 버튼은 별개의 자식 창이라 자기 클릭을 직접 받는다.
    // 여기로 오는 건 버튼이 없는 빈 영역 - 섹션 헤더를 눌렀는지만 보면 된다.
    for (int s = 0; s < kSectionCount; ++s) {
        if (GetSectionHeaderRect(s).PtInRect(point)) {
            m_sectionExpanded[s] = !m_sectionExpanded[s];
            RelayoutPanel();                    // 배치 다시 + Invalidate
            return;
        }
    }
    CWnd::OnLButtonDown(nFlags, point);
}
