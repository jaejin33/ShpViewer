#pragma once

#include "IGeometry.h"
#include <array>
#include <memory>
#include <vector>

namespace Spatial {

    // 쿼드트리 노드 정보 (GetVisibleNodes 결과용)
    struct NodeInfo {
        GeoData::BoundingBox bounds; // 이 노드의 영역(AABB)
        int depth;                   // 트리 내 깊이 (0 = 루트)
    };

    // 객체 + 깊이 정보 묶음 (GetVisibleItemsWithDepth 결과용)
    struct ItemInfo {
        std::shared_ptr<GeoData::IGeometry> item; // 실제 지오메트리 객체
        int depth;                                 // 해당 객체가 저장된 노드 깊이
    };

    // 쿼드트리의 단일 노드 — 재귀적으로 4개의 자식 노드를 가진다.
    //
    // 분할 규칙:
    //   - 한 노드에 MAX_ITEMS개를 초과하면 Subdivide()로 4분할
    //   - 깊이가 MAX_DEPTH에 도달하면 더 이상 분할하지 않음
    //   - 여러 사분면에 걸치는 객체는 현재 노드에 직접 저장됨
    class QuadNode {
    private:
        static constexpr size_t MAX_ITEMS = 10; // 분할 기준 최대 객체 수
        static constexpr int    MAX_DEPTH = 8;  // 최대 트리 깊이

        GeoData::BoundingBox m_bounds; // 이 노드가 관리하는 공간 영역
        int m_depth;                   // 트리 내 현재 깊이

        std::array<std::unique_ptr<QuadNode>, 4> m_children; // 4개 자식 (NE, NW, SW, SE)
        std::vector<std::shared_ptr<GeoData::IGeometry>> m_items; // 이 노드에 직접 저장된 객체

    public:
        QuadNode(const GeoData::BoundingBox& bounds, int depth = 0);

        // 객체를 트리에 삽입 (성공하면 true)
        bool Insert(std::shared_ptr<GeoData::IGeometry> item);

        // 검색 영역과 교차하는 객체를 수집 (고정 크기 LOD 필터링)
        void GetVisibleItems(const GeoData::BoundingBox& searchArea,
            std::vector<std::shared_ptr<GeoData::IGeometry>>& results,
            float minObjectSize) const;

        // 검색 영역과 교차하는 객체를 깊이 정보와 함께 수집 (거리 기반 적응형 LOD)
        //   eyeX/Y/Z : 카메라 눈 위치 (월드 공간)
        //   lodScale : 2*tan(fov/2)*MIN_PIXELS/screenHeight
        //              이 값과 눈-객체 거리의 곱이 객체 크기보다 크면 건너뜀
        void GetVisibleItemsWithDepth(const GeoData::BoundingBox& searchArea,
            std::vector<ItemInfo>& results,
            float eyeX, float eyeY, float eyeZ,
            float lodScale) const;

        // 검색 영역과 교차하는 모든 노드를 수집 (MBR 시각화에 사용)
        void GetVisibleNodes(const GeoData::BoundingBox& searchArea,
            std::vector<NodeInfo>& nodes) const;

        // 이 노드와 모든 자식 노드의 데이터를 비움
        void Clear();

    private:
        // 이 노드를 4개 자식 노드로 분할
        void Subdivide();

        // 자식 노드가 없으면 리프 노드
        bool IsLeaf() const { return m_children[0] == nullptr; }

        // 객체의 AABB가 어느 사분면에 완전히 속하는지 반환
        // 여러 사분면에 걸치면 -1 반환 (현재 노드에 보관)
        int  GetQuadrant(const GeoData::BoundingBox& itemBounds) const;
    };
}
