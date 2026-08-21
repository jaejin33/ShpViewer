
// ShpViewerView.cpp: CShpViewerView 클래스의 구현
//

#include "pch.h"
#include "framework.h"
// SHARED_HANDLERS는 미리 보기, 축소판 그림 및 검색 필터 처리기를 구현하는 ATL 프로젝트에서 정의할 수 있으며
// 해당 프로젝트와 문서 코드를 공유하도록 해 줍니다.
#ifndef SHARED_HANDLERS
#include "ShpViewer.h"
#endif

#include "ShpViewerDoc.h"
#include "ShpViewerView.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif
#include <parse/ShpDataset.h>


// CShpViewerView

IMPLEMENT_DYNCREATE(CShpViewerView, CView)

BEGIN_MESSAGE_MAP(CShpViewerView, CView)
	// 표준 인쇄 명령입니다.
	ON_COMMAND(ID_FILE_PRINT, &CView::OnFilePrint)
	ON_COMMAND(ID_FILE_PRINT_DIRECT, &CView::OnFilePrint)
	ON_COMMAND(ID_FILE_PRINT_PREVIEW, &CShpViewerView::OnFilePrintPreview)
	ON_WM_CONTEXTMENU()
	ON_WM_RBUTTONUP()
	ON_WM_CREATE()
	ON_WM_SIZE()
END_MESSAGE_MAP()

struct QuadTreeStats {
	int32_t total_node_count = 0;
	int32_t leaf_node_count = 0;
	int32_t max_depth_reached = 0;
	int32_t total_object_count = 0;
};

void CollectQuadTreeStats(const QuadTreeNode* node, int32_t depth, QuadTreeStats* out_stats) {
	out_stats->total_node_count++;
	out_stats->total_object_count += static_cast<int32_t>(node->object_indices.size());

	if (depth > out_stats->max_depth_reached) {
		out_stats->max_depth_reached = depth;
	}

	bool is_leaf = true;
	for (const std::unique_ptr<QuadTreeNode>& child : node->children) {
		if (child) {
			is_leaf = false;
			CollectQuadTreeStats(child.get(), depth + 1, out_stats);
		}
	}

	if (is_leaf) {
		out_stats->leaf_node_count++;
	}
}

// CShpViewerView 생성/소멸

CShpViewerView::CShpViewerView() noexcept
{
	// TODO: 여기에 생성 코드를 추가합니다.

}

CShpViewerView::~CShpViewerView()
{
}

BOOL CShpViewerView::PreCreateWindow(CREATESTRUCT& cs)
{
	// TODO: CREATESTRUCT cs를 수정하여 여기에서
	//  Window 클래스 또는 스타일을 수정합니다.

	return CView::PreCreateWindow(cs);
}

// CShpViewerView 그리기

void CShpViewerView::OnDraw(CDC* /*pDC*/)
{
	CShpViewerDoc* pDoc = GetDocument();
	ASSERT_VALID(pDoc);
	if (!pDoc)
		return;

	// TODO: 여기에 원시 데이터에 대한 그리기 코드를 추가합니다.
}


// CShpViewerView 인쇄

void CShpViewerView::OnInitialUpdate() {
	CView::OnInitialUpdate();

	//ShpDataset dataset;
	//bool success = BuildShpDataset("C:\\Users\\egis\\Desktop\\F_FAC_BUILDING_26_202505.shp", &dataset);
	//if (!success) {
	//	return;
	//}
	//QuadTreeStats stats;
	//CollectQuadTreeStats(dataset.quad_tree.get(), 0, &stats);
}

void CShpViewerView::OnFilePrintPreview()
{
#ifndef SHARED_HANDLERS
	AFXPrintPreview(this);
#endif
}

BOOL CShpViewerView::OnPreparePrinting(CPrintInfo* pInfo)
{
	// 기본적인 준비
	return DoPreparePrinting(pInfo);
}

void CShpViewerView::OnBeginPrinting(CDC* /*pDC*/, CPrintInfo* /*pInfo*/)
{
	// TODO: 인쇄하기 전에 추가 초기화 작업을 추가합니다.
}

void CShpViewerView::OnEndPrinting(CDC* /*pDC*/, CPrintInfo* /*pInfo*/)
{
	// TODO: 인쇄 후 정리 작업을 추가합니다.
}

void CShpViewerView::OnRButtonUp(UINT /* nFlags */, CPoint point)
{
	ClientToScreen(&point);
	OnContextMenu(this, point);
}

void CShpViewerView::OnContextMenu(CWnd* /* pWnd */, CPoint point)
{
#ifndef SHARED_HANDLERS
	theApp.GetContextMenuManager()->ShowPopupMenu(IDR_POPUP_EDIT, point.x, point.y, this, TRUE);
#endif
}


// CShpViewerView 진단

#ifdef _DEBUG
void CShpViewerView::AssertValid() const
{
	CView::AssertValid();
}

void CShpViewerView::Dump(CDumpContext& dc) const
{
	CView::Dump(dc);
}

CShpViewerDoc* CShpViewerView::GetDocument() const // 디버그되지 않은 버전은 인라인으로 지정됩니다.
{
	ASSERT(m_pDocument->IsKindOf(RUNTIME_CLASS(CShpViewerDoc)));
	return (CShpViewerDoc*)m_pDocument;
}
#endif //_DEBUG


// CShpViewerView 메시지 처리기

int CShpViewerView::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (CView::OnCreate(lpCreateStruct) == -1)
		return -1;

	// TODO:  여기에 특수화된 작성 코드를 추가합니다.
	CRect rc;
	GetClientRect(&rc);
	m_glView.Create(nullptr, _T("Shp Viewer"), WS_CHILD | WS_VISIBLE, rc, this, 1001);
	m_glView.InitEGL();

	return 0;
}

void CShpViewerView::OnSize(UINT nType, int cx, int cy)
{
	CView::OnSize(nType, cx, cy);

	// TODO: 여기에 메시지 처리기 코드를 추가합니다.
	if (::IsWindow(m_glView.GetSafeHwnd()))  // OnCreate가 끝나기전에 실행될 수 있어서 안전장치 역할을 함
		m_glView.MoveWindow(0, 0, cx, cy);
}


void CShpViewerView::OnUpdate(CView* /*pSender*/, LPARAM /*lHint*/, CObject* /*pHint*/)
{
	CShpViewerDoc* pDoc = GetDocument();
	if (pDoc) {
		m_glView.SetDataset(&pDoc->m_dataset);
	}
	m_glView.Invalidate();
}
