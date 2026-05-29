#pragma once
#include <string>
#include <vector>
#include <memory>
#include <fstream>
#include "IGeometry.h"

namespace ShpIO {

    // SHP 파일에서 지원하는 도형 타입 열거형 (ESRI Shapefile 스펙)
    enum class ShpType : int {
        NullShape = 0,  // 빈 레코드
        Point     = 1,  // 점
        PolyLine  = 3,  // 폴리라인 (선)
        Polygon   = 5   // 폴리곤 (면)
    };

    // SHP 파일 파싱 결과를 담는 구조체
    struct ShpData {
        GeoData::BoundingBox globalBounds;                          // 파일 전체의 MBR
        std::vector<std::shared_ptr<GeoData::IGeometry>> geometries; // 파싱된 도형 목록
    };

    // SHP 바이너리 파일 파서 (정적 메서드만 제공)
    class ShpParser {
    public:
        // filePath에 있는 .shp 파일을 파싱해 outData에 결과를 저장한다.
        // 성공하면 true, 파일 없음/형식 오류이면 false 반환
        static bool Parse(const std::string& filePath, ShpData& outData);

    private:
        // 빅 엔디안 32비트 정수 읽기 (SHP 헤더/레코드 헤더에 사용)
        static int32_t ReadBigEndianInt(std::ifstream& stream);

        // 리틀 엔디안 32비트 정수 읽기 (도형 데이터에 사용)
        static int32_t ReadLittleEndianInt(std::ifstream& stream);

        // 리틀 엔디안 64비트 부동소수 읽기 (좌표값에 사용)
        static double  ReadLittleEndianDouble(std::ifstream& stream);

        // 레코드 파싱 함수 (도형 타입별)
        static std::shared_ptr<GeoData::IGeometry> ParsePoint(   std::ifstream& stream, int32_t recordId);
        static std::shared_ptr<GeoData::IGeometry> ParsePolyLine(std::ifstream& stream, int32_t recordId);
        static std::shared_ptr<GeoData::IGeometry> ParsePolygon( std::ifstream& stream, int32_t recordId);
    };

} // namespace ShpIO
