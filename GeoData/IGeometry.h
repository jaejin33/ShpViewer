#pragma once
#include "BoundingBox.h"

namespace GeoData {

    // 모든 공간 도형(Geometry)의 추상 기반 클래스.
    // PointShape, PolylineShape, PolygonShape가 이 인터페이스를 구현한다.
    class IGeometry {
    protected:
        // .dbf 파일과 연결된 이 도형의 고유 ID
        int m_shapeId = 0;

    public:
        virtual ~IGeometry() = default;

        // 이 도형을 감싸는 최소 경계 직사각형(MBR)을 반환 (순수 가상)
        virtual BoundingBox GetBounds() const = 0;

        int  GetShapeId() const       { return m_shapeId; }
        void SetShapeId(int id)       { m_shapeId = id; }
    };

} // namespace GeoData
