
// ShpViewerDoc.cpp: CShpViewerDoc 클래스의 구현
//

#include "pch.h"
#include "framework.h"
// SHARED_HANDLERS는 미리 보기, 축소판 그림 및 검색 필터 처리기를 구현하는 ATL 프로젝트에서 정의할 수 있으며
// 해당 프로젝트와 문서 코드를 공유하도록 해 줍니다.
#ifndef SHARED_HANDLERS
#include "ShpViewer.h"
#endif

#include "ShpViewerDoc.h"
#include "ShpParser.h"

#include <propkey.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// CShpViewerDoc

IMPLEMENT_DYNCREATE(CShpViewerDoc, CDocument)

BEGIN_MESSAGE_MAP(CShpViewerDoc, CDocument)
END_MESSAGE_MAP()

BOOL CShpViewerDoc::OnOpenDocument(LPCTSTR lpszPathName)
{
	if (!CDocument::OnOpenDocument(lpszPathName))
		return FALSE;

	DeleteContents();

	CT2CA pszConvertedAnsiString(lpszPathName);
	std::string stdPathName(pszConvertedAnsiString);

	// 1. 파싱된 결과를 담을 빈 바구니(ShpData) 준비
	ShpIO::ShpData parsedData;

	// 2. 정적(Static) 함수 호출! 바구니를 넘겨서 채워오라고 지시
	if (!ShpIO::ShpParser::Parse(stdPathName, parsedData)) {
		AfxMessageBox(_T("SHP 파일을 읽는 데 실패했습니다."));
		return FALSE;
	}

	// 3. 바구니(parsedData)에서 전체 바운딩 박스를 꺼내서 쿼드트리 생성
	m_quadTree = std::make_shared<Spatial::QuadTree>(parsedData.globalBounds);

	// 4. 바구니(parsedData)에서 도형들을 하나씩 꺼내서 쿼드트리에 삽입
	for (const auto& shape : parsedData.geometries) {
		m_quadTree->Insert(shape);
	}

	// 5. 화면 갱신 지시
	UpdateAllViews(nullptr);

	return TRUE;
}

void CShpViewerDoc::DeleteContents()
{
	if (m_quadTree) {
		m_quadTree->Clear(); // 쿼드트리 내부 자식들과 데이터들 연쇄 파괴
		m_quadTree.reset();  // 루트 방 파괴
	}

	CDocument::DeleteContents();
}

// CShpViewerDoc 생성/소멸

CShpViewerDoc::CShpViewerDoc() noexcept
{
	// TODO: 여기에 일회성 생성 코드를 추가합니다.

}

CShpViewerDoc::~CShpViewerDoc()
{
}

BOOL CShpViewerDoc::OnNewDocument()
{
	if (!CDocument::OnNewDocument())
		return FALSE;

	// TODO: 여기에 재초기화 코드를 추가합니다.
	// SDI 문서는 이 문서를 다시 사용합니다.

	return TRUE;
}




// CShpViewerDoc serialization

void CShpViewerDoc::Serialize(CArchive& ar)
{
	if (ar.IsStoring())
	{
		// TODO: 여기에 저장 코드를 추가합니다.
	}
	else
	{
		// TODO: 여기에 로딩 코드를 추가합니다.
	}
}

#ifdef SHARED_HANDLERS

// 축소판 그림을 지원합니다.
void CShpViewerDoc::OnDrawThumbnail(CDC& dc, LPRECT lprcBounds)
{
	// 문서의 데이터를 그리려면 이 코드를 수정하십시오.
	dc.FillSolidRect(lprcBounds, RGB(255, 255, 255));

	CString strText = _T("TODO: implement thumbnail drawing here");
	LOGFONT lf;

	CFont* pDefaultGUIFont = CFont::FromHandle((HFONT) GetStockObject(DEFAULT_GUI_FONT));
	pDefaultGUIFont->GetLogFont(&lf);
	lf.lfHeight = 36;

	CFont fontDraw;
	fontDraw.CreateFontIndirect(&lf);

	CFont* pOldFont = dc.SelectObject(&fontDraw);
	dc.DrawText(strText, lprcBounds, DT_CENTER | DT_WORDBREAK);
	dc.SelectObject(pOldFont);
}

// 검색 처리기를 지원합니다.
void CShpViewerDoc::InitializeSearchContent()
{
	CString strSearchContent;
	// 문서의 데이터에서 검색 콘텐츠를 설정합니다.
	// 콘텐츠 부분은 ";"로 구분되어야 합니다.

	// 예: strSearchContent = _T("point;rectangle;circle;ole object;");
	SetSearchContent(strSearchContent);
}

void CShpViewerDoc::SetSearchContent(const CString& value)
{
	if (value.IsEmpty())
	{
		RemoveChunk(PKEY_Search_Contents.fmtid, PKEY_Search_Contents.pid);
	}
	else
	{
		CMFCFilterChunkValueImpl *pChunk = nullptr;
		ATLTRY(pChunk = new CMFCFilterChunkValueImpl);
		if (pChunk != nullptr)
		{
			pChunk->SetTextValue(PKEY_Search_Contents, value, CHUNK_TEXT);
			SetChunkValue(pChunk);
		}
	}
}

#endif // SHARED_HANDLERS

// CShpViewerDoc 진단

#ifdef _DEBUG
void CShpViewerDoc::AssertValid() const
{
	CDocument::AssertValid();
}

void CShpViewerDoc::Dump(CDumpContext& dc) const
{
	CDocument::Dump(dc);
}
#endif //_DEBUG


// CShpViewerDoc 명령
