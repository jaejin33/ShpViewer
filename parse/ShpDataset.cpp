#include "pch.h"
#include "ShpDataset.h"
#include "ShpParser.h"
#include <stdexcept>

namespace {
	// "...\\XXX.shp" -> "...\\XXX.dbf" (마지막 '.' 뒤 확장자만 교체)
	std::string ReplaceExtensionWithDbf(const std::string& shp_path) {
		const size_t dot = shp_path.find_last_of('.');
		if (dot == std::string::npos) return shp_path + ".dbf";
		return shp_path.substr(0, dot) + ".dbf";
	}

	constexpr float kAverageFloorHeightMeters = 3.0f;

	bool TryParsePositiveFloat(const std::string* text, float* out_value) {
		if (text == nullptr || text->empty()) return false;
		try {
			const float parsed = std::stof(*text);
			if (parsed <= 0.0f) return false;
			*out_value = parsed;
			return true;
		}
		catch (const std::exception&) {
			return false;
		}
	}
}

bool BuildShpDataset(const std::string& path, ShpDataset* out_dataset) {
	if (!ReadShpFile(path, &out_dataset->header, &out_dataset->records)) {
		return false;
	}

	// .dbf는 선택 사항으로 취급: 없거나 읽기 실패해도 여기서 false를 반환하지 않고
	// SHP 지오메트리만으로 계속 진행한다. attributes가 비어있으면 GetRecordHeight가
	// 항상 fallback_height를 돌려주게 되어 있어서 호출부가 별도로 널 체크할 필요는 없음.
	const std::string dbf_path = ReplaceExtensionWithDbf(path);
	ReadDbfFile(dbf_path, &out_dataset->dbf_header, &out_dataset->attributes);

	float min_x = out_dataset->records[0].bounds_min.x;
	float min_z = out_dataset->records[0].bounds_min.z;
	float max_x = out_dataset->records[0].bounds_max.x;
	float max_z = out_dataset->records[0].bounds_max.z;

	for (size_t i = 0; i < out_dataset->records.size();i++) {
		const ShpPolygonRecord& record = out_dataset->records[i];

		if (record.bounds_min.x < min_x) {
			min_x = record.bounds_min.x;
		}
		if (record.bounds_min.z < min_z) {
			min_z = record.bounds_min.z;
		}
		if (record.bounds_max.x > max_x) {
			max_x = record.bounds_max.x;
		}
		if (record.bounds_max.z > max_z) {
			max_z = record.bounds_max.z;
		}
	}
	
	QuadBounds world_bounds;
	world_bounds.max_x = max_x;
	world_bounds.max_z = max_z;
	world_bounds.min_x = min_x;
	world_bounds.min_z = min_z;

	out_dataset->quad_tree = BuildQuadTree(world_bounds);

	for (size_t i = 0; i < out_dataset->records.size();i++) {
		const ShpPolygonRecord& record = out_dataset->records[i];

		QuadBounds object_bounds;
		object_bounds.min_x = record.bounds_min.x;
		object_bounds.min_z = record.bounds_min.z;
		object_bounds.max_x = record.bounds_max.x;
		object_bounds.max_z = record.bounds_max.z;

		InsertObject(out_dataset->quad_tree.get(), static_cast<int32_t>(i), object_bounds);
	}

	return true;
}

float GetRecordHeight(const ShpDataset& dataset, size_t index, float fallback_height) {
	if (index >= dataset.attributes.size()) return fallback_height;
	const DbfRecord& record = dataset.attributes[index];

	float height = 0.0f;
	if (TryParsePositiveFloat(FindDbfFieldValue(dataset.dbf_header, record, "HEIGHT"), &height)) {
		return height;
	}

	float floor_count = 0.0f;
	if (TryParsePositiveFloat(FindDbfFieldValue(dataset.dbf_header, record, "GRND_FLR"), &floor_count)) {
		return floor_count * kAverageFloorHeightMeters;
	}

	return fallback_height;
}
