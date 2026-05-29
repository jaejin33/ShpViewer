#pragma once
#include "IGeometry.h"
#include "PointShape.h"
#include <vector>

namespace GeoData {

    // 폴리곤(Polygon) 도형 — 하나 이상의 링(ring)으로 구성된 면형 도형.
    // SHP Type 5 (Polygon)에 해당한다.
    // parts[i] = points 배열에서 i번째 링의 첫 번째 점 인덱스 (첫 링=외곽, 나머지=홀)
    class PolygonShape : public IGeometry {
    public:
        // 링(ring) 시작 인덱스 배열 (첫 번째 링 = 외곽 경계, 나머지 = 내부 홀)
        std::vector<int> parts;

        // 전체 정점 배열 (모든 링의 포인트가 연속으로 저장)
        std::vector<PointShape> points;

    public:
        // 생성자: ID를 받아 부모에게 전달
        PolygonShape(int id) {
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
