#pragma once
#include <cstdint>
#include <string>
#include <vector>

// DBF(dBASE III) 파일의 필드 하나를 설명하는 디스크립터.
// SHP를 전혀 모르는 범용 DBF 리더용 타입 — ShpTypes.h와는 독립적으로 유지한다
// (CLAUDE.md의 계층 분리 원칙: DbfTypes.h/DbfParser.h/.cpp는 SHP를 모르고,
//  ShpTypes.h도 DBF를 모른다. 둘을 인덱스로 짝짓는 조립 코드는 ShpDataset.cpp에 둔다).
struct DbfFieldDescriptor {
	std::string name;
	char type = 'C';           // 'C' 문자, 'N' 숫자, 'D' 날짜, 'L' 논리 등
	uint8_t length = 0;        // 필드 폭(바이트)
	uint8_t decimal_count = 0; // N/F 타입에서만 의미 있음
};

struct DbfHeader {
	int32_t record_count = 0;
	int16_t header_size_bytes = 0;  // 32(고정 헤더) + 필드 디스크립터들 + 터미네이터 1바이트
	int16_t record_size_bytes = 0;  // 삭제 플래그 1바이트 포함
	std::vector<DbfFieldDescriptor> fields;
};

// 레코드 하나 = fields와 같은 순서/개수의 문자열 값.
// trim까지 끝난 텍스트 그대로 보관 (숫자 타입도 아직 문자열 — 실제 float/int 변환은
// 이 값을 쓰는 쪽, 예: ShpDataset.cpp의 GetRecordHeight에서 함).
struct DbfRecord {
	bool deleted = false;
	std::vector<std::string> values;
};
