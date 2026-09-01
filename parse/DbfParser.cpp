#include "pch.h"
#include "DbfParser.h"

namespace {
	int16_t ReadInt16LittleEndian(std::ifstream& file) {
		int16_t value = 0;
		file.read(reinterpret_cast<char*>(&value), sizeof(value));
		return value;
	}

	int32_t ReadInt32LittleEndian(std::ifstream& file) {
		int32_t value = 0;
		file.read(reinterpret_cast<char*>(&value), sizeof(value));
		return value;
	}

	// 필드명(11바이트, 남는 자리 0x00 패딩)을 문자열로 변환.
	// buffer를 12바이트로 잡아 마지막 한 칸을 항상 0으로 남겨두면, 11바이트가 전부
	// 글자로 꽉 차 있는 경우에도 std::string(buffer)가 끝을 못 찾고 넘어가는 일이 없음.
	std::string ReadFieldName(std::ifstream& file) {
		char buffer[12] = {};
		file.read(buffer, 11);
		return std::string(buffer);  // 첫 0x00에서 자동으로 끊김
	}

	// 값 필드는 공백(0x20) 패딩이 DBF 표준이지만, 실제 F_FAC_BUILDING 데이터를 보면
	// 비어있는 문자 필드(BLD_NM 등)가 0x00으로 패딩된 경우도 있어서 공백/널 둘 다 trim한다.
	// find_first_not_of(const char*, pos, n) 오버로드를 쓰는 이유: 인자로 주는 "잘라낼 문자
	// 집합" 자체에 '\0'이 섞여 있어서, 길이를 자동으로 재는 보통의 const char* 오버로드로는
	// 그 집합 문자열이 첫 '\0'에서 끊겨버려 공백만 취급하게 됨 — n을 명시해서 그걸 막는다.
	std::string TrimFieldValue(const char* buffer, int32_t length) {
		const std::string value(buffer, length);
		const char trim_chars[2] = { ' ', '\0' };
		const size_t begin = value.find_first_not_of(trim_chars, 0, 2);
		if (begin == std::string::npos) return "";
		const size_t end = value.find_last_not_of(trim_chars, std::string::npos, 2);
		return value.substr(begin, end - begin + 1);
	}
}

bool ReadDbfHeader(std::ifstream& file, DbfHeader* out_header) {
	file.seekg(0);

	file.get();                     // 버전 번호 — 지금은 사용 안 함
	file.seekg(3, std::ios::cur);   // 마지막 수정일(YY/MM/DD) 스킵

	out_header->record_count = ReadInt32LittleEndian(file);
	out_header->header_size_bytes = ReadInt16LittleEndian(file);
	out_header->record_size_bytes = ReadInt16LittleEndian(file);

	file.seekg(20, std::ios::cur);  // 예약/멀티유저 영역(오프셋 12~31) 스킵

	out_header->fields.clear();
	while (file.peek() != 0x0D) {
		DbfFieldDescriptor field;
		field.name = ReadFieldName(file);
		field.type = static_cast<char>(file.get());
		file.seekg(4, std::ios::cur);   // 필드 데이터 주소 — 파일 읽을 땐 의미 없음
		field.length = static_cast<uint8_t>(file.get());
		field.decimal_count = static_cast<uint8_t>(file.get());
		file.seekg(14, std::ios::cur);  // 예약 영역 스킵

		out_header->fields.push_back(std::move(field));
	}
	file.get();  // 터미네이터(0x0D) 소비

	return file.good();
}

bool ReadDbfRecord(std::ifstream& file, const DbfHeader& header, DbfRecord* out_record) {
	const int deletion_flag = file.get();
	out_record->deleted = (deletion_flag == '*');

	out_record->values.clear();
	out_record->values.reserve(header.fields.size());

	std::vector<char> buffer;
	for (const DbfFieldDescriptor& field : header.fields) {
		buffer.resize(field.length);
		file.read(buffer.data(), field.length);
		out_record->values.push_back(TrimFieldValue(buffer.data(), field.length));
	}

	return file.good();
}

bool ReadDbfFile(const std::string& path, DbfHeader* out_header, std::vector<DbfRecord>* out_records) {
	std::ifstream file(path, std::ios::binary);
	if (!file.is_open()) return false;

	if (!ReadDbfHeader(file, out_header)) return false;

	out_records->clear();
	out_records->reserve(out_header->record_count);

	// ReadDbfHeader가 끝난 시점 위치 == header_size_bytes 이어야 정상이지만,
	// 혹시 모를 어긋남에 대비해 명시적으로 헤더 크기만큼 seek 해서 레코드 시작 위치를 보장.
	file.seekg(out_header->header_size_bytes);

	for (int32_t i = 0; i < out_header->record_count; ++i) {
		DbfRecord record;
		if (!ReadDbfRecord(file, *out_header, &record)) return false;
		out_records->push_back(std::move(record));
	}

	return true;
}

const std::string* FindDbfFieldValue(const DbfHeader& header, const DbfRecord& record, const std::string& field_name) {
	for (size_t i = 0; i < header.fields.size(); ++i) {
		if (header.fields[i].name == field_name) {
			if (i >= record.values.size()) return nullptr;
			return &record.values[i];
		}
	}
	return nullptr;
}
