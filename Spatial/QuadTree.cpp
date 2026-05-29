#include "pch.h"
#include "QuadTree.h"

namespace Spatial {

    // 생성자: 전체 범위로 루트 노드를 초기화
    QuadTree::QuadTree(const GeoData::BoundingBox& bounds)
        : m_totalBounds(bounds), m_totalCount(0)
    {
        m_root = std::make_unique<QuadNode>(bounds, 0);
    }

    // 객체를 쿼드트리에 삽입한다.
    // 삽입 성공 시 총 객체 수(m_totalCount)를 증가시킨다.
    bool QuadTree::Insert(std::shared_ptr<GeoData::IGeometry> item) {
        if (!m_root || !item) return false;
        bool inserted = m_root->Insert(item);
        if (inserted) ++m_totalCount;
        return inserted;
    }

    // 검색 영역과 교차하는 객체를 수집한다 (고정 크기 LOD).
    void QuadTree::GetVisibleItems(const GeoData::BoundingBox& searchArea,
        std::vector<std::shared_ptr<GeoData::IGeometry>>& results,
        float minObjectSize) const
    {
        if (m_root) m_root->GetVisibleItems(searchArea, results, minObjectSize);
    }

    // 검색 영역과 교차하는 객체를 깊이 정보와 함께 수집한다 (거리 기반 적응형 LOD).
    void QuadTree::GetVisibleItemsWithDepth(const GeoData::BoundingBox& searchArea,
        std::vector<ItemInfo>& results,
        float eyeX, float eyeY, float eyeZ,
        float lodScale) const
    {
        if (m_root) m_root->GetVisibleItemsWithDepth(
            searchArea, results, eyeX, eyeY, eyeZ, lodScale);
    }

    // 검색 영역과 교차하는 모든 쿼드트리 노드를 수집한다 (MBR 시각화용).
    void QuadTree::GetVisibleNodes(const GeoData::BoundingBox& searchArea,
        std::vector<NodeInfo>& nodes) const
    {
        if (m_root) m_root->GetVisibleNodes(searchArea, nodes);
    }

    // 트리를 초기 상태로 비운다.
    // 루트를 새로 생성하여 전체 범위(m_totalBounds)를 다시 커버한다.
    void QuadTree::Clear() {
        if (m_root) {
            m_root->Clear();
            m_root = std::make_unique<QuadNode>(m_totalBounds, 0);
        }
        m_totalCount = 0;
    }
}
