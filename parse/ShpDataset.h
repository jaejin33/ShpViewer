#pragma once
#include "ShpTypes.h"
#include <memory>
#include <vector>
#include "QuadTree.h"
#include <string>

struct ShpDataset {
	ShpHeader header;
	std::vector<ShpPolygonRecord> records;
	std::unique_ptr<QuadTreeNode> quad_tree;
};

bool BuildShpDataset(const std::string& path, ShpDataset* out_dataset);