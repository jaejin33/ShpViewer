
// ShpViewerDoc.h: CShpViewerDoc 클래스의 인터페이스
//


#pragma once
#include <memory>
#include "QuadTree.h"
#include <thread>
#include <atomic>


class CShpViewerDoc : public CDocument
{
protected: // serialization에서만 만들어집니다.
	CShpViewerDoc() noexcept;
	DECLARE_DYNCREATE(CShpViewerDoc)

private:
	std::shared_ptr<Spatial::QuadTree> m_quadTree;
	

// 특성입니다.
public:

// 작업입니다.
public:
	std::shared_ptr<Spatial::QuadTree> GetQuadTree() const {
		return m_quadTree;
	}
// 재정의입니다.
public:
	virtual BOOL OnNewDocument();
	virtual BOOL OnOpenDocument(LPCTSTR lpszPathName); // 파일 열기 이벤트
	virtual void DeleteContents();					   // 데이터 초기화 이벤트
	virtual void Serialize(CArchive& ar);
#ifdef SHARED_HANDLERS
	virtual void InitializeSearchContent();
	virtual void OnDrawThumbnail(CDC& dc, LPRECT lprcBounds);
#endif // SHARED_HANDLERS

// 구현입니다.
public:
	virtual ~CShpViewerDoc();
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif

protected:

// 생성된 메시지 맵 함수
protected:
	DECLARE_MESSAGE_MAP()

#ifdef SHARED_HANDLERS
	// 검색 처리기에 대한 검색 콘텐츠를 설정하는 도우미 함수
	void SetSearchContent(const CString& value);
#endif // SHARED_HANDLERS
};
