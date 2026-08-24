#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <vector>
#include "../culling/Frustum.h"

enum class QuadChildIndex : int32_t {
	kTopLeft = 0,
	kTopRight = 1,
	kBottomLeft = 2,
	kBottomRight = 3,
};

struct QuadBounds {
	float min_x = 0.0f;
	float min_z = 0.0f;
	float max_x = 0.0f;
	float max_z = 0.0f;
};

struct QuadTreeNode {
	QuadBounds tight_bounds;
	QuadBounds loose_bounds;
	std::vector<int32_t> object_indices;
	std::array<std::unique_ptr<QuadTreeNode>, 4> children;
};

constexpr float kDefaultLoosenessFactor = 2.0f;
constexpr int32_t kMaxQuadTreeDepth = 9;

// tight_bounds 중심점 기준으로 (center_x, center_z)가 4분면 중 몇 번인지 반환.
QuadChildIndex SelectChildIndex(const QuadBounds& tight_bounds, float center_x, float center_z);

// 부모 tight_bounds를 절반으로 나눠서 child_index번째 자식의 tight_bounds 계산.
QuadBounds ComputeChildTightBounds(const QuadBounds& parent_tight_bounds, QuadChildIndex child_index);

// tight_bounds를 중심 기준 looseness_factor배로 확장한 loose_bounds 계산.
QuadBounds ComputeLooseBounds(const QuadBounds& tight_bounds, float looseness_factor);

// inner가 outer 안에 완전히 들어가는지 검사
bool IsFullInside(const QuadBounds& outer, const QuadBounds& inner);

// bounds의 중심점(min/max 평균)을 out 파라미터로 반환
void ComputeBoundsCenter(const QuadBounds& bounds, float* out_center_x, float* out_center_z);

// node를 루트로 하는 서브트리에 폴리곤 하나(인덱스+bbox)를 재귀적으로 삽입
void InsertObject(QuadTreeNode* node, int32_t object_index, const QuadBounds& object_bounds, int32_t depth = 0);

// world_bounds를 루트 tight_bounds로 하는 새 트리를 만들어서 루트노드 반환
std::unique_ptr<QuadTreeNode> BuildQuadTree(const QuadBounds& world_bounds);

void QueryVisibleObjects(const QuadTreeNode* node, const std::array<Plane, 6>& planes, std::vector<int32_t>* out_visible_indices);