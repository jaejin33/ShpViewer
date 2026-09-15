
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
#include <algorithm>

namespace {
	constexpr int kInspectorWidth = 220;
}
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
	int32_t objects_per_depth[kMaxQuadTreeDepth + 1] = {};
};

struct ObjectSizeStats {
	float min_width = FLT_MAX;
	float min_depth = FLT_MAX;
	std::vector<float> widths;
	std::vector<float> depths;
};

void CollectObjectSizeStats(const ShpDataset& dataset, ObjectSizeStats* out_stats) {
	out_stats->widths.reserve(dataset.records.size());
	out_stats->depths.reserve(dataset.records.size());

	for (const ShpPolygonRecord& record : dataset.records) {
		float width = record.bounds_max.x - record.bounds_min.x;
		float depth = record.bounds_max.z - record.bounds_min.z;
		out_stats->widths.push_back(width);
		out_stats->depths.push_back(depth);
		out_stats->min_width = min(out_stats->min_width, width);
		out_stats->min_depth = min(out_stats->min_depth, depth);
	}
}

// p: 0.0~1.0 (0.01 = 하위 1%)
float Percentile(std::vector<float> values, float p) {
	std::sort(values.begin(), values.end());
	size_t index = static_cast<size_t>(values.size() * p);
	if (index >= values.size()) index = values.size() - 1;
	return values[index];
}

void CollectQuadTreeStats(const QuadTreeNode* node, int32_t depth, QuadTreeStats* out_stats) {
	if (node == nullptr) {
		return;
	}

	out_stats->total_node_count++;

	int32_t object_count_here = static_cast<int32_t>(node->object_indices.size());
	out_stats->total_object_count += object_count_here;
	if (depth <= kMaxQuadTreeDepth) {
		out_stats->objects_per_depth[depth] += object_count_here;   // 추가
	}

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
	//bool success = BuildShpDataset("C:\\Users\\egis\\Desktop\\F_FAC_BUILDING_BUSAN\\F_FAC_BUILDING_26_202505.shp", &dataset);
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

	CRect rc;
	GetClientRect(&rc);

	int gl_width = (rc.Width() > kInspectorWidth) ? (rc.Width() - kInspectorWidth) : rc.Width();

	m_glView.Create(nullptr, _T("Shp Viewer"), WS_CHILD | WS_VISIBLE,
		CRect(0, 0, gl_width, rc.Height()), this, 1001);
	m_glView.InitEGL();

	m_inspector.Create(this);
	m_inspector.MoveWindow(gl_width, 0, kInspectorWidth, rc.Height());

	return 0;
}

void CShpViewerView::OnSize(UINT nType, int cx, int cy)
{
	CView::OnSize(nType, cx, cy);

	int gl_width = (cx > kInspectorWidth) ? (cx - kInspectorWidth) : cx;

	if (::IsWindow(m_glView.GetSafeHwnd()))
		m_glView.MoveWindow(0, 0, gl_width, cy);

	if (::IsWindow(m_inspector.GetSafeHwnd()))
		m_inspector.MoveWindow(gl_width, 0, kInspectorWidth, cy);
}


void CShpViewerView::OnUpdate(CView* /*pSender*/, LPARAM /*lHint*/, CObject* /*pHint*/)
{
	CShpViewerDoc* pDoc = GetDocument();
	if (pDoc) {
		m_glView.SetDataset(&pDoc->m_dataset);
		
		if (pDoc->m_dataset.quad_tree) {
			QuadTreeStats stats;
			CollectQuadTreeStats(pDoc->m_dataset.quad_tree.get(), 0, &stats);

			CString summary_msg;
			summary_msg.Format(_T("total_nodes=%d, leaf_nodes=%d, max_depth_reached=%d, total_objects=%d\n"),
				stats.total_node_count, stats.leaf_node_count, stats.max_depth_reached, stats.total_object_count);
			OutputDebugString(summary_msg);

			for (int32_t d = 0; d <= kMaxQuadTreeDepth; ++d) {
				CString depth_msg;
				depth_msg.Format(_T("  depth %d: %d objects\n"), d, stats.objects_per_depth[d]);
				OutputDebugString(depth_msg);
			}

			// 리프(depth 13) 타이트 셀 크기 — 루트 bounds에서 바로 계산
			const QuadBounds& root_bounds = pDoc->m_dataset.quad_tree->tight_bounds;
			float root_width = root_bounds.max_x - root_bounds.min_x;
			float root_depth = root_bounds.max_z - root_bounds.min_z;
			float leaf_width = root_width / static_cast<float>(1 << kMaxQuadTreeDepth);
			float leaf_depth = root_depth / static_cast<float>(1 << kMaxQuadTreeDepth);

			CString leaf_msg;
			leaf_msg.Format(_T("leaf(depth %d) tight size: width=%.3f, depth=%.3f\n"),
				kMaxQuadTreeDepth, leaf_width, leaf_depth);
			OutputDebugString(leaf_msg);

			// 객체 크기 분포
			ObjectSizeStats size_stats;
			CollectObjectSizeStats(pDoc->m_dataset, &size_stats);

			CString width_msg;
			width_msg.Format(_T("object width: min=%.3f, p1=%.3f, p5=%.3f, p10=%.3f, p20=%.3f, p50=%.3f\n"),
				size_stats.min_width,
				Percentile(size_stats.widths, 0.01f),
				Percentile(size_stats.widths, 0.05f),
				Percentile(size_stats.widths, 0.10f),
				Percentile(size_stats.widths, 0.20f),
				Percentile(size_stats.widths, 0.50f));
			OutputDebugString(width_msg);

			CString depth_msg2;
			depth_msg2.Format(_T("object depth: min=%.3f, p1=%.3f, p5=%.3f, p10=%.3f, p20=%.3f, p50=%.3f\n"),
				size_stats.min_depth,
				Percentile(size_stats.depths, 0.01f),
				Percentile(size_stats.depths, 0.05f),
				Percentile(size_stats.depths, 0.10f),
				Percentile(size_stats.depths, 0.20f),
				Percentile(size_stats.depths, 0.50f));
			OutputDebugString(depth_msg2);
		}
	}
	m_glView.Invalidate();
}

void CShpViewerView::UpdateInspector(int32_t visible_count, int32_t total_count, float fps) {
	m_inspector.UpdateStats(visible_count, total_count, fps);
}

void CShpViewerView::SetShowQuadTreeLevels(bool show) {
	m_glView.SetShowQuadTreeLevels(show);
}

void CShpViewerView::SetShowAllNodes(bool show) {
	m_glView.SetShowAllNodes(show);
}

void CShpViewerView::SetShowAllObjectLevelColors(bool show) {
	m_glView.SetShowAllObjectLevelColors(show);
}

void CShpViewerView::SetShowFill(bool show) {
	m_glView.SetShowFill(show);
}

void CShpViewerView::SetShowObjectBounds(bool show) {
	m_glView.SetShowObjectBounds(show);
}

void CShpViewerView::SetShow3D(bool show) {
	m_glView.SetShow3D(show);
}

void CShpViewerView::SetShowEdges(bool show) {
	m_glView.SetShowEdges(show);
}

void CShpViewerView::SetShowObjectOutline(bool show) {
	m_glView.SetShowObjectOutline(show);
}

void CShpViewerView::SetShowTriangulationLines(bool show) {
	m_glView.SetShowTriangulationLines(show);
}