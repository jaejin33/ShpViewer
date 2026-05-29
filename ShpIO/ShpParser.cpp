#include "pch.h"
#include "ShpParser.h"
#include "PointShape.h"
#include "PolyLineShape.h"
#include "PolygonShape.h"
#include <iostream>

namespace ShpIO {

    // 4바이트를 뒤집어 빅 엔디안 → 리틀 엔디안 변환
    inline int32_t SwapEndian(int32_t val) {
        uint32_t uval = static_cast<uint32_t>(val);
        return static_cast<int32_t>(
            ((uval & 0x000000FF) << 24) |
            ((uval & 0x0000FF00) <<  8) |
            ((uval & 0x00FF0000) >>  8) |
            ((uval & 0xFF000000) >> 24)
        );
    }

    // 빅 엔디안 정수 읽기 (SHP 파일 헤더/레코드 헤더에 사용)
    int32_t ShpParser::ReadBigEndianInt(std::ifstream& stream) {
        int32_t value;
        stream.read(reinterpret_cast<char*>(&value), sizeof(int32_t));
        return SwapEndian(value);
    }

    // 리틀 엔디안 정수 읽기 (도형 데이터에 사용)
    int32_t ShpParser::ReadLittleEndianInt(std::ifstream& stream) {
        int32_t value;
        stream.read(reinterpret_cast<char*>(&value), sizeof(int32_t));
        return value;
    }

    // 리틀 엔디안 배정밀도 부동소수 읽기 (좌표값에 사용)
    double ShpParser::ReadLittleEndianDouble(std::ifstream& stream) {
        double value;
        stream.read(reinterpret_cast<char*>(&value), sizeof(double));
        return value;
    }

    // SHP 파일 전체를 파싱해 outData에 저장
    bool ShpParser::Parse(const std::string& filePath, ShpData& outData) {

        std::ifstream stream(filePath, std::ios::binary);
        if (!stream.is_open()) return false;

        // 파일 코드 확인 (SHP 파일은 항상 9994로 시작)
        int32_t fileCode = ReadBigEndianInt(stream);
        if (fileCode != 9994) {
            stream.close();
            return false; // SHP 파일이 아니면 false
        }

        // 전역 도형 타입 및 전체 범위 MBR 읽기 (오프셋 32)
        stream.seekg(32, std::ios::beg);
        int32_t globalShapeType = ReadLittleEndianInt(stream);

        double minX = ReadLittleEndianDouble(stream);
        double minY = ReadLittleEndianDouble(stream);
        double maxX = ReadLittleEndianDouble(stream);
        double maxY = ReadLittleEndianDouble(stream);
        outData.globalBounds = GeoData::BoundingBox(minX, minY, maxX, maxY);

        // 도형 타입에 따른 파서 함수 포인터 선택
        using ParseFunction = std::shared_ptr<GeoData::IGeometry>(*)(std::ifstream&, int32_t);
        ParseFunction shapeParser = nullptr;

        if      (globalShapeType == static_cast<int32_t>(ShpType::Point))    shapeParser = ParsePoint;
        else if (globalShapeType == static_cast<int32_t>(ShpType::PolyLine)) shapeParser = ParsePolyLine;
        else if (globalShapeType == static_cast<int32_t>(ShpType::Polygon))  shapeParser = ParsePolygon;
        else {
            return false; // 지원하지 않는 타입이면 바로 종료
        }

        // 파일 헤더(100바이트) 이후 첫 번째 레코드로 커서 이동
        stream.seekg(100, std::ios::beg);

        // 레코드 파싱 루프
        while (stream.peek() != EOF && stream.good()) {
            int32_t recordNumber   = ReadBigEndianInt(stream);  // 레코드 번호 (1 기반)
            int32_t contentLength  = ReadBigEndianInt(stream);  // 콘텐츠 길이 (16비트 워드 단위)
            int32_t recordShapeType = ReadLittleEndianInt(stream);

            // SHP 레코드는 '혼합 NullShape' 가 포함될 수 있다.
            if (recordShapeType != static_cast<int32_t>(ShpType::NullShape)) {
                // 타입 검증 없이, 파서 함수에 직접 위임
                outData.geometries.push_back(shapeParser(stream, recordNumber));
            } else {
                // NullShape인 경우 해당 레코드 바이트를 건너뜀
                stream.seekg((contentLength * 2) - 4, std::ios::cur);
            }
        }

        stream.close();
        return true;
    }

    // 점(Point) 레코드 파싱: X, Y 두 배정밀도 좌표를 읽는다
    std::shared_ptr<GeoData::IGeometry>
    ShpParser::ParsePoint(std::ifstream& stream, int32_t recordId) {
        double x = ReadLittleEndianDouble(stream);
        double y = ReadLittleEndianDouble(stream);
        return std::make_shared<GeoData::PointShape>(recordId, x, y);
    }

    // 폴리라인(PolyLine) 레코드 파싱
    std::shared_ptr<GeoData::IGeometry>
    ShpParser::ParsePolyLine(std::ifstream& stream, int32_t recordId) {
        // MBR 32바이트 (MinX, MinY, MaxX, MaxY) 건너뛰기 — 전체 파일 범위로 대체 가능
        stream.seekg(32, std::ios::cur);

        int32_t numParts  = ReadLittleEndianInt(stream);
        int32_t numPoints = ReadLittleEndianInt(stream);

        auto polyline = std::make_shared<GeoData::PolylineShape>(recordId);
        polyline->parts.resize(numParts);
        for (int i = 0; i < numParts; ++i)
            polyline->parts[i] = ReadLittleEndianInt(stream);

        polyline->points.reserve(numPoints);
        for (int i = 0; i < numPoints; ++i) {
            double x = ReadLittleEndianDouble(stream);
            double y = ReadLittleEndianDouble(stream);
            polyline->points.emplace_back(recordId, x, y);
        }
        return polyline;
    }

    // 폴리곤(Polygon) 레코드 파싱 — 파싱 방식은 Polyline과 동일
    std::shared_ptr<GeoData::IGeometry>
    ShpParser::ParsePolygon(std::ifstream& stream, int32_t recordId) {
        stream.seekg(32, std::ios::cur); // MBR 32바이트 건너뛰기

        int32_t numParts  = ReadLittleEndianInt(stream);
        int32_t numPoints = ReadLittleEndianInt(stream);

        auto polygon = std::make_shared<GeoData::PolygonShape>(recordId);
        polygon->parts.resize(numParts);
        for (int i = 0; i < numParts; ++i)
            polygon->parts[i] = ReadLittleEndianInt(stream);

        polygon->points.reserve(numPoints);
        for (int i = 0; i < numPoints; ++i) {
            double x = ReadLittleEndianDouble(stream);
            double y = ReadLittleEndianDouble(stream);
            polygon->points.emplace_back(recordId, x, y);
        }
        return polygon;
    }

} // namespace ShpIO
