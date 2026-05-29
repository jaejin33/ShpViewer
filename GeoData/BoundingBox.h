#pragma once

namespace GeoData {

    // 공간 객체의 축-정렬 경계 박스(Axis-Aligned Bounding Box).
    // 쿼드트리 삽입/검색 및 카메라 컬링에 사용된다.
    struct BoundingBox {
        double minX;
        double minY;
        double maxX;
        double maxY;

        // 기본 생성자 및 초기화 생성자
        BoundingBox(double x1 = 0.0, double y1 = 0.0,
                    double x2 = 0.0, double y2 = 0.0)
            : minX(x1), minY(y1), maxX(x2), maxY(y2) {}

        // 두 BoundingBox가 서로 겹치거나 충돌하는지 검사 (Frustum Culling 및 인터섹트 검사용)
        bool Intersects(const BoundingBox& other) const {
            return !(minX > other.maxX || maxX < other.minX ||
                     minY > other.maxY || maxY < other.minY);
        }

        // 경계를 확장하는 병합 함수 (여러 Polyline, Polygon 통합 시 사용)
        void Merge(const BoundingBox& other) {
            if (other.minX < minX) minX = other.minX;
            if (other.minY < minY) minY = other.minY;
            if (other.maxX > maxX) maxX = other.maxX;
            if (other.maxY > maxY) maxY = other.maxY;
        }
    };

} // namespace GeoData
