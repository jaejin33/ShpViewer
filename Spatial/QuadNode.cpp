#include "pch.h"
#include "QuadNode.h"

namespace Spatial {

    QuadNode::QuadNode(const GeoData::BoundingBox& bounds, int depth)
        : m_bounds(bounds), m_depth(depth) {}

    // 이 노드와 모든 자식 노드를 재귀적으로 비운다
    void QuadNode::Clear() {
        m_items.clear();
        for (auto& child : m_children) {
            if (child) { child->Clear(); child.reset(); }
        }
    }

    // 객체의 AABB가 4 사분면 중 하나에 완전히 들어가는지 확인한다.
    // 여러 사분면에 걸치면 -1을 반환해 현재 노드에 보관하도록 한다.
    // 사분면 번호: 0=NE, 1=NW, 2=SW, 3=SE
    int QuadNode::GetQuadrant(const GeoData::BoundingBox& b) const {
        double midX = (m_bounds.minX + m_bounds.maxX) / 2.0;
        double midY = (m_bounds.minY + m_bounds.maxY) / 2.0;
        bool fitsTop    = (b.minY >= midY); // 완전히 위쪽 절반에 위치
        bool fitsBottom = (b.maxY <= midY); // 완전히 아래쪽 절반에 위치
        bool fitsRight  = (b.minX >= midX); // 완전히 오른쪽 절반에 위치
        bool fitsLeft   = (b.maxX <= midX); // 완전히 왼쪽 절반에 위치
        if (fitsTop    && fitsRight) return 0; // NE (북동)
        if (fitsTop    && fitsLeft)  return 1; // NW (북서)
        if (fitsBottom && fitsLeft)  return 2; // SW (남서)
        if (fitsBottom && fitsRight) return 3; // SE (남동)
        return -1; // 경계에 걸침 -> 현재 노드에 보관
    }

    // 객체를 적절한 노드에 삽입한다.
    // 리프 노드에서 MAX_ITEMS를 초과하면 Subdivide() 후 자식에게 재배분한다.
    bool QuadNode::Insert(std::shared_ptr<GeoData::IGeometry> item) {
        if (!m_bounds.Intersects(item->GetBounds())) return false; // 영역 밖이면 거부

        // 내부 노드라면 해당 사분면 자식에게 위임
        if (!IsLeaf()) {
            int q = GetQuadrant(item->GetBounds());
            if (q != -1) return m_children[q]->Insert(item);
        }

        // 현재 노드에 직접 보관
        m_items.push_back(item);

        // 리프 노드가 꽉 찼고 최대 깊이 미만이면 4분할 후 자식에게 재배분
        if (IsLeaf() && m_items.size() > MAX_ITEMS && m_depth < MAX_DEPTH) {
            Subdivide();
            auto it = m_items.begin();
            while (it != m_items.end()) {
                int q = GetQuadrant((*it)->GetBounds());
                if (q != -1) { m_children[q]->Insert(*it); it = m_items.erase(it); }
                else          { ++it; } // 여전히 경계에 걸치면 현재 노드에 유지
            }
        }
        return true;
    }

    // 이 노드를 4개 자식 노드(NE/NW/SW/SE)로 분할한다.
    void QuadNode::Subdivide() {
        double mx = (m_bounds.minX + m_bounds.maxX) / 2.0; // X 중앙
        double my = (m_bounds.minY + m_bounds.maxY) / 2.0; // Y 중앙
        // 0:NE, 1:NW, 2:SW, 3:SE 순서로 자식 생성
        m_children[0] = std::make_unique<QuadNode>(GeoData::BoundingBox(mx, my, m_bounds.maxX, m_bounds.maxY), m_depth + 1);
        m_children[1] = std::make_unique<QuadNode>(GeoData::BoundingBox(m_bounds.minX, my, mx, m_bounds.maxY), m_depth + 1);
        m_children[2] = std::make_unique<QuadNode>(GeoData::BoundingBox(m_bounds.minX, m_bounds.minY, mx, my), m_depth + 1);
        m_children[3] = std::make_unique<QuadNode>(GeoData::BoundingBox(mx, m_bounds.minY, m_bounds.maxX, my), m_depth + 1);
    }

    // --- 가시 객체 수집 (고정 크기 LOD 필터링) ---
    // minObjectSize: 이보다 작은 객체(가로, 세로 모두)는 건너뜀
    void QuadNode::GetVisibleItems(const GeoData::BoundingBox& searchArea,
        std::vector<std::shared_ptr<GeoData::IGeometry>>& results,
        float minObjectSize) const
    {
        if (!m_bounds.Intersects(searchArea)) return; // 검색 영역 밖이면 조기 종료
        for (const auto& item : m_items) {
            if (!searchArea.Intersects(item->GetBounds())) continue;
            if (minObjectSize > 0.0f) {
                const auto& b = item->GetBounds();
                float w = (float)(b.maxX - b.minX);
                float h = (float)(b.maxY - b.minY);
                if (w < minObjectSize && h < minObjectSize) continue; // 너무 작으면 스킵
            }
            results.push_back(item);
        }
        // 자식 노드 재귀 탐색
        if (!IsLeaf()) {
            for (const auto& child : m_children)
                if (child) child->GetVisibleItems(searchArea, results, minObjectSize);
        }
    }

    // --- 거리 기반 적응형 LOD로 가시 객체 수집 (깊이 정보 포함) ---
    //
    // 노드 레벨 조기 종료(Early Exit):
    //   카메라(eye)에서 이 노드 중심까지의 거리가 멀고 노드 크기가 작으면
    //   서브트리 전체를 건너뜀. 이를 통해 원거리 서브트리를 O(1)에 제거한다.
    //
    //   조건: nodeSize^2 < distSq * lodScale^2
    //         (제곱 비교로 sqrt 연산 회피)
    //
    // 객체 레벨 LOD:
    //   각 객체에 대해서도 동일한 기준으로 필터링한다.
    void QuadNode::GetVisibleItemsWithDepth(const GeoData::BoundingBox& searchArea,
        std::vector<ItemInfo>& results,
        float eyeX, float eyeY, float eyeZ,
        float lodScale) const
    {
        if (!m_bounds.Intersects(searchArea)) return; // 검색 영역 밖이면 조기 종료

        // 노드 레벨 조기 종료 판단
        if (lodScale > 0.0f) {
            float nw = (float)(m_bounds.maxX - m_bounds.minX);
            float nh = (float)(m_bounds.maxY - m_bounds.minY);
            float nodeSize = nw > nh ? nw : nh; // 노드의 최대 변 길이
            float cx = (float)((m_bounds.minX + m_bounds.maxX) * 0.5f); // 노드 중심 X
            float cy = (float)((m_bounds.minY + m_bounds.maxY) * 0.5f); // 노드 중심 Y
            float dx = cx - eyeX, dy = cy - eyeY;
            float distSq = dx*dx + dy*dy + eyeZ*eyeZ; // eye -> 노드 중심 거리^2
            // nodeSize^2 < distSq * lodScale^2 이면 서브트리 전체 스킵
            if (nodeSize * nodeSize < distSq * lodScale * lodScale) return;
        }

        // 이 노드에 직접 저장된 객체들 검사
        for (const auto& item : m_items) {
            const auto& b = item->GetBounds();
            if (!searchArea.Intersects(b)) continue;

            // 객체 레벨 거리 기반 LOD 필터
            if (lodScale > 0.0f) {
                float w    = (float)(b.maxX - b.minX);
                float h    = (float)(b.maxY - b.minY);
                float size = w > h ? w : h; // 객체의 최대 변 길이
                float cx   = (float)((b.minX + b.maxX) * 0.5f); // 객체 중심 X
                float cy   = (float)((b.minY + b.maxY) * 0.5f); // 객체 중심 Y
                float dx   = cx - eyeX, dy = cy - eyeY;
                float distSq = dx*dx + dy*dy + eyeZ*eyeZ; // eye -> 객체 중심 거리^2
                if (size * size < distSq * lodScale * lodScale) continue; // 너무 작으면 스킵
            }

            results.push_back({ item, m_depth });
        }

        // 자식 노드 재귀 탐색 (동일한 LOD 파라미터 전달)
        if (!IsLeaf()) {
            for (const auto& child : m_children)
                if (child) child->GetVisibleItemsWithDepth(
                    searchArea, results, eyeX, eyeY, eyeZ, lodScale);
        }
    }

    // --- 화면에 걸치는 모든 쿼드트리 노드를 수집 (MBR 시각화용) ---
    void QuadNode::GetVisibleNodes(const GeoData::BoundingBox& searchArea,
        std::vector<NodeInfo>& nodes) const
    {
        if (!m_bounds.Intersects(searchArea)) return; // 검색 영역 밖이면 조기 종료
        nodes.push_back({ m_bounds, m_depth }); // 현재 노드 기록
        // 자식 노드 재귀 탐색
        if (!IsLeaf()) {
            for (const auto& child : m_children)
                if (child) child->GetVisibleNodes(searchArea, nodes);
        }
    }
}
