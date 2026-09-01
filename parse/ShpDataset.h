#pragma once
#include "ShpTypes.h"
#include <memory>
#include <vector>
#include "QuadTree.h"
#include "DbfParser.h"
#include <string>

struct ShpDataset {
	ShpHeader header;
	std::vector<ShpPolygonRecord> records;
	std::unique_ptr<QuadTreeNode> quad_tree;

	// .dbf 속성 — records와 인덱스로 1:1 대응(레코드 i의 속성 = attributes[i]).
	// Shapefile 스펙상 .shp/.dbf 레코드 순서가 항상 같기 때문에 인덱스로만 짝지으면 됨.
	// .dbf가 없거나 파싱에 실패해도 attributes가 비어있을 뿐 BuildShpDataset 자체는 계속 진행.
	DbfHeader dbf_header;
	std::vector<DbfRecord> attributes;
};

bool BuildShpDataset(const std::string& path, ShpDataset* out_dataset);

// index번째 레코드의 HEIGHT 속성을 float로 읽어서 반환.
// - index가 attributes 범위를 벗어나거나, HEIGHT 필드 자체가 없으면 fallback_height.
// - 실데이터(F_FAC_BUILDING) 확인 결과 HEIGHT=0은 "값 없음"으로 쓰이고 있어서
//   0 이하인 경우도 fallback_height로 취급한다(0을 유효한 건물 높이로 보지 않음).
float GetRecordHeight(const ShpDataset& dataset, size_t index, float fallback_height);
