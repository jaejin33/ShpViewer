#pragma once
#include "IGeometry.h"

namespace GeoData {

    // 단일 점(Point) 도형.
    // SHP Type 1 (Point)에 해당한다.
    struct PointShape : public IGeometry {
        double x;
        double y;
        double z; // Z 좌표 (2D 데이터면 0.0)

        // id: 부모로부터 받는 고유 ID (자동 초기화 불가)
        PointShape(int id, double _x, double _y, double _z = 0.0)
            : x(_x), y(_y), z(_z)
        {
            m_shapeId = id;
        }

        // 점은 너비/높이 0의 MBR (좌표 자체가 경계)
        virtual BoundingBox GetBounds() const override {
            return BoundingBox(x, y, x, y);
        }
    };

} // namespace GeoData
