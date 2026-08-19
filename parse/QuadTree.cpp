#include "pch.h"
#include "QuadTree.h"

void InsertObject(QuadTreeNode* node, int32_t object_index, const QuadBounds& object_bounds, int32_t depth) {
	if (depth >= kMaxQuadTreeDepth) {
		node->object_indices.push_back(object_index);
		return;
	}

	float object_center_x = 0.0f, object_center_z = 0.0f;
	ComputeBoundsCenter(object_bounds, &object_center_x, &object_center_z);
	
	QuadChildIndex child_index = SelectChildIndex(node->tight_bounds, object_center_x, object_center_z);

	int32_t index = static_cast<int32_t>(child_index);

	if (!node->children[index]) {
		node->children[index] = std::make_unique<QuadTreeNode>();
		node->children[index]->tight_bounds = ComputeChildTightBounds(node->tight_bounds, child_index);
		node->children[index]->loose_bounds = ComputeLooseBounds(node->children[index]->tight_bounds, kDefaultLoosenessFactor);
	}
	if (IsFullInside(node->children[index]->loose_bounds, object_bounds)) {
		InsertObject(node->children[index].get(), object_index, object_bounds, depth + 1);
	}
	else {
		node->object_indices.push_back(object_index);
	}
}

void ComputeBoundsCenter(const QuadBounds& bounds, float* out_center_x, float* out_center_z) {
	*out_center_x = (bounds.min_x + bounds.max_x) / 2.0f;
	*out_center_z = (bounds.min_z + bounds.max_z) / 2.0f;
}

bool IsFullInside(const QuadBounds& outer, const QuadBounds& inner) {
	if (outer.max_x < inner.max_x || outer.min_x > inner.min_x
		|| outer.max_z < inner.max_z || outer.min_z > inner.min_z)
		return false;
	return true;
}

QuadBounds ComputeLooseBounds(const QuadBounds& tight_bounds, float looseness_factor) {
	QuadBounds loose_bounds;
	float center_x = 0.0f, center_z = 0.0f;

	ComputeBoundsCenter(tight_bounds, &center_x, &center_z);

	loose_bounds.max_x = center_x + (tight_bounds.max_x - center_x) * looseness_factor;
	loose_bounds.max_z = center_z + (tight_bounds.max_z - center_z) * looseness_factor;
	loose_bounds.min_x = center_x + (tight_bounds.min_x - center_x) * looseness_factor;
	loose_bounds.min_z = center_z + (tight_bounds.min_z - center_z) * looseness_factor;

	return loose_bounds;
}

QuadChildIndex SelectChildIndex(const QuadBounds& tight_bounds, float center_x, float center_z) {
	float node_center_x = 0.0f, node_center_z = 0.0f;
	ComputeBoundsCenter(tight_bounds, &node_center_x, &node_center_z);
	
	bool isRight = center_x >= node_center_x;
	bool isUp = center_z >= node_center_z;

	if (isRight) {
		if(isUp)
			return QuadChildIndex::kTopRight;
		return QuadChildIndex::kBottomRight;
	}
	else {
		if (isUp)
			return QuadChildIndex::kTopLeft;
		return QuadChildIndex::kBottomLeft;
	}
}

QuadBounds ComputeChildTightBounds(const QuadBounds& parent_tight_bounds, QuadChildIndex child_index) {
	float parent_center_z = 0.0f, parent_center_x = 0.0f;

	ComputeBoundsCenter(parent_tight_bounds, &parent_center_x, &parent_center_z);

	QuadBounds result;
	
	switch (child_index) {
	case QuadChildIndex::kTopRight:
		result.max_x = parent_tight_bounds.max_x;
		result.max_z = parent_tight_bounds.max_z;
		result.min_x = parent_center_x;
		result.min_z = parent_center_z;
		break;
	case QuadChildIndex::kBottomRight:
		result.max_x = parent_tight_bounds.max_x;
		result.max_z = parent_center_z;
		result.min_x = parent_center_x;
		result.min_z = parent_tight_bounds.min_z;
		break;
	case QuadChildIndex::kTopLeft:
		result.max_x = parent_center_x;
		result.max_z = parent_tight_bounds.max_z;
		result.min_x = parent_tight_bounds.min_x;
		result.min_z = parent_center_z;
		break;
	case QuadChildIndex::kBottomLeft:
		result.max_x = parent_center_x;
		result.max_z = parent_center_z;
		result.min_x = parent_tight_bounds.min_x;
		result.min_z = parent_tight_bounds.min_z;
		break;
	}

	return result;
}

std::unique_ptr<QuadTreeNode> BuildQuadTree(const QuadBounds& world_bounds) {
	std::unique_ptr<QuadTreeNode> root_node = std::make_unique<QuadTreeNode>();
	root_node->tight_bounds = world_bounds;

	root_node->loose_bounds = ComputeLooseBounds(root_node->tight_bounds, kDefaultLoosenessFactor);

	return root_node;
}