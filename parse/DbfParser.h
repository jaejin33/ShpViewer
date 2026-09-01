#pragma once

#include <fstream>
#include <string>
#include <vector>
#include "DbfTypes.h"

bool ReadDbfHeader(std::ifstream& file, DbfHeader* out_header);

bool ReadDbfRecord(std::ifstream& file, const DbfHeader& header, DbfRecord* out_record);

bool ReadDbfFile(const std::string& path, DbfHeader* out_header, std::vector<DbfRecord>* out_records);

// header.fields에서 field_name과 이름이 같은 필드를 찾아 record의 값을 반환.
// 그런 필드가 없거나 index가 record.values 범위를 벗어나면 nullptr.
const std::string* FindDbfFieldValue(const DbfHeader& header, const DbfRecord& record, const std::string& field_name);
