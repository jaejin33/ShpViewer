#pragma once
#include "QuadNode.h"
#include <memory>
#include <vector>

namespace Spatial {

    // 2D 공간 인덱스 -- 쿼드트리(QuadTree) 최상위 컨테이너
    //
    // 사용 방법:
    //   1. QuadTree qt(전체영역);
    //   2. qt.Insert(geometry);  // 객체 삽입
    //   3. qt.GetVisibleItemsWithDepth(뷰영역, 결과, eye, lodScale);  // 가시 객체 조회
    class QuadTree {
    private:
        std::unique_ptr<QuadNode> m_root;   // 루트 노드
        GeoData::BoundingBox m_totalBounds; // 전체 데이터 범위 (Clear 후 재생성에 사용)
        int m_totalCount = 0;               // 삽입된 총 객체 수

    public:
        // bounds : 이 쿼드트리가 커버하는 전체 공간 영역
        QuadTree(const GeoData::BoundingBox& bounds);

        // 객체를 트리에 삽입 (bounds 밖이면 false 반환)
        bool Insert(std::shared_ptr<GeoData::IGeometry> item);

        // 검색 영역과 교차하는 객체를 수집 (고정 크기 LOD)
        void GetVisibleItems(const GeoData::BoundingBox& searchArea,
            std::vector<std::shared_ptr<GeoData::IGeometry>>& results,
            float minObjectSize = 0.0f) const;

        // 검색 영역과 교차하는 객체를 깊이 정보와 함께 수집 (거리 기반 적응형 LOD)
        //   eyeX/Y/Z : 카메라 눈 위치 (월드 공간)
        //   lodScale : 2*tan(fov/2)*MIN_PIXELS/screenHeight
        void GetVisibleItemsWithDepth(const GeoData::BoundingBox& searchArea,
            std::vector<ItemInfo>& results,
            float eyeX, float eyeY, float eyeZ,
            float lodScale = 0.0f) const;

        // 검색 영역과 교차하는 모든 노드를 수집 (MBR 시각화에 사용)
        void GetVisibleNodes(const GeoData::BoundingBox& searchArea,
            std::vector<NodeInfo>& nodes) const;

        // 트리를 초기 상태로 비운다 (루트를 새로 생성)
        void Clear();

        // 전체 데이터 범위 조회 (카메라 초기 위치 계산에 사용)
        GeoData::BoundingBox GetBounds() const { return m_totalBounds; }

        // 삽입된 총 객체 수 (인스펙터 표시용)
        int GetTotalCount() const { return m_totalCount; }
    };
}
