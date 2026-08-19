#include "pch.h"
#include "ShpDataset.h"
#include "ShpParser.h"

bool BuildShpDataset(const std::string& path, ShpDataset* out_dataset) {
	if (!ReadShpFile(path, &out_dataset->header, &out_dataset->records)) {
		return false;
	}

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