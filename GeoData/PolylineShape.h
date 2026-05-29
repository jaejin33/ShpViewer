#pragma once
#include "IGeometry.h"
#include "PointShape.h"
#include <vector>

namespace GeoData {

    // 폴리라인(Polyline) 도형 — 하나 이상의 파트(part)로 구성된 선형 도형.
    // SHP Type 3 (PolyLine)에 해당한다.
    class PolylineShape : public IGeometry {
    public:
        // 하나의 파트 내에 속하는 포인트들의 시작 인덱스
        // parts[i] = points 배열에서 i번째 파트의 첫 번째 점 인덱스
        std::vector<int> parts;

        // 전체 선형 포인트
        std::vector<PointShape> points;

    public:
        // 생성자: ID를 받아 부모에게 전달
        PolylineShape(int id) {
            m_shapeId = id;
        }

        // 이 도형 전체를 감싸는 최소 MBR(BoundingBox)을 계산하여 반환
        virtual BoundingBox GetBounds() const override {
            if (points.empty()) return BoundingBox(0, 0, 0, 0);

            double minX = points[0].x, minY = points[0].y;
            double maxX = points[0].x, maxY = points[0].y;

            for (const auto& pt : points) {
                if (pt.x < minX) minX = pt.x;
                if (pt.y < minY) minY = pt.y;
                if (pt.x > maxX) maxX = pt.x;
                if (pt.y > maxY) maxY = pt.y;
            }
            return BoundingBox(minX, minY, maxX, maxY);
        }
    };

} // namespace GeoData
