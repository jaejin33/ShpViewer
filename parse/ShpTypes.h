#pragma once
#include <cstdint>
#include <vector>
#include "../math/Vec3.h"

enum class ShpShapeType : int32_t {
	kNullShape = 0,
	kPoint = 1,
	kPolyLine = 3,
	kPolygon = 5,
	kMultiPoint = 8,
	kPointZ = 11,
	kPolyLineZ = 13,
	kPolygonZ = 15,
};

struct ShpHeader {
	int32_t file_length_bytes = 0;
	int32_t shape_type = 0;
	Vec3 world_bbox_min;	// 월드 좌표로 변환된 bbox 최소점
	Vec3 world_bbox_max;	// 월드 좌표로 변환된 bbox 최대점
};

struct ShpPolygonRecord {
	std::vector<int32_t> part_start_indices;
	std::vector<Vec3> points;
	Vec3 bounds_min;
	Vec3 bounds_max;
};