#pragma once

#include <fstream>
#include <vector>
#include <string>
#include "ShpTypes.h"

Vec3 ConvertShpPointToWorld(double shp_x, double shp_y, double shp_z);

bool ReadShpHeader(std::ifstream& file, ShpHeader* out_header);

bool ReadPolygonRecord(std::ifstream& file, ShpPolygonRecord* out_record);

bool ReadShpFile(const std::string& path, ShpHeader* out_header, std::vector<ShpPolygonRecord>* out_records);