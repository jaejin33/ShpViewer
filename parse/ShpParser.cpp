#include "pch.h"
#include "ShpParser.h"

namespace {
	int32_t ReadInt32BigEndian(std::ifstream& file) {
		uint8_t bytes[4];
		file.read(reinterpret_cast<char*>(bytes), 4);
		return (static_cast<int32_t>(bytes[0]) << 24) | (static_cast<int32_t>(bytes[1]) << 16) |
			(static_cast<int32_t>(bytes[2]) << 8) | (static_cast<int32_t>(bytes[3]));
	}

	int32_t ReadInt32LittleEndian(std::ifstream& file) {
		int32_t value = 0;
		file.read(reinterpret_cast<char*>(&value), sizeof(value));
		return value;
	}

	double ReadDoubleLittleEndian(std::ifstream& file) {
		double value = 0.0;
		file.read(reinterpret_cast<char*>(&value), sizeof(value));
		return value;
	}
}

Vec3 ConvertShpPointToWorld(double shp_x, double shp_y, double shp_z) {
	return Vec3(static_cast<float>(shp_x), static_cast<float>(shp_z), static_cast<float>(-shp_y));
}

bool ReadShpHeader(std::ifstream& file, ShpHeader* out_header) {
	ReadInt32BigEndian(file);
	for (int i = 0; i < 5; ++i) ReadInt32BigEndian(file);

	const int32_t file_length_words = ReadInt32BigEndian(file);
	out_header->file_length_bytes = file_length_words * 2;

	ReadInt32LittleEndian(file);
	out_header->shape_type = ReadInt32LittleEndian(file);

	const double shp_xmin = ReadDoubleLittleEndian(file);
	const double shp_ymin = ReadDoubleLittleEndian(file);
	const double shp_xmax = ReadDoubleLittleEndian(file);
	const double shp_ymax = ReadDoubleLittleEndian(file);
	const double shp_zmin = ReadDoubleLittleEndian(file);
	const double shp_zmax = ReadDoubleLittleEndian(file);
	ReadDoubleLittleEndian(file);	// Mmin
	ReadDoubleLittleEndian(file);	// Mmax

	out_header->world_bbox_min = ConvertShpPointToWorld(shp_xmin, shp_ymax, shp_zmin);
	out_header->world_bbox_max = ConvertShpPointToWorld(shp_xmax, shp_ymin, shp_zmax);

	return file.good();
}

bool ReadPolygonRecord(std::ifstream& file, ShpPolygonRecord* out_record) {
	const double xmin = ReadDoubleLittleEndian(file);
	const double ymin = ReadDoubleLittleEndian(file);
	const double xmax = ReadDoubleLittleEndian(file);
	const double ymax = ReadDoubleLittleEndian(file);
	
	out_record->bounds_min = ConvertShpPointToWorld(xmin, ymax, 0.0);
	out_record->bounds_max = ConvertShpPointToWorld(xmax, ymin, 0.0);

	const int32_t num_parts = ReadInt32LittleEndian(file);
	const int32_t num_points = ReadInt32LittleEndian(file);

	out_record->part_start_indices.resize(num_parts);  // 아래 반복문에서 인덱스 접근을 하기위해 미리 사이즈를 잡아두는것
	for (int32_t i = 0; i < num_parts; ++i) {
		out_record->part_start_indices[i] = ReadInt32LittleEndian(file);
	}

	out_record->points.resize(num_points);
	for (int32_t i = 0; i < num_points; ++i) {
		const double x = ReadDoubleLittleEndian(file);
		const double y = ReadDoubleLittleEndian(file);
		out_record->points[i] = ConvertShpPointToWorld(x, y, 0.0);
	}

	return file.good();
}

bool ReadShpFile(const std::string& path, ShpHeader* out_header, std::vector<ShpPolygonRecord>* out_records) {
	std::ifstream file(path, std::ios::binary);
	if (!file.is_open()) return false;

	if (!ReadShpHeader(file, out_header)) return false;

	switch (static_cast<ShpShapeType>(out_header->shape_type)) {
	case ShpShapeType::kPolygon:
			break;
		default:
			return false;
	}

	out_records->clear();

	while (file.peek() != std::ifstream::traits_type::eof()) {
		ReadInt32BigEndian(file);
		const int32_t content_length_words = ReadInt32BigEndian(file);
		const std::streampos record_end = file.tellg() + static_cast<std::streamoff>(content_length_words * 2); // streamoff로 변환 해줘야 + 연산이 수행됨

		const int32_t record_shape_type = ReadInt32LittleEndian(file);
		if (record_shape_type == static_cast<int32_t>(ShpShapeType::kPolygon)) {
			ShpPolygonRecord record;
			if (!ReadPolygonRecord(file, &record)) return false;
			out_records->push_back(std::move(record));
		}

		file.seekg(record_end);
	}

	return true;
}