#pragma once
#include "Parse/parse.hpp"
#include "Help/Show/show.hpp"
#include "Parse/sense.hpp"
#include <string>

void serialize(std::string& input, InsertNode& insert, const Table* table);

void deserialize(const Table* table,std::vector<uint32_t>& need,std::string& input, Info* output);

bool serialize_row_from_strings(const Table* table, const std::vector<std::string>& values, std::string& output);

void serialize_system(const std::string& table_name, CreateNode& create, std::string& input, uint32_t newpage);

bool deserialize_system(const std::string& input, std::string& out_table_name, uint32_t& out_root_page,
                        std::vector<std::string>& out_col_names,
                        std::vector<std::string>& out_col_types);

#pragma pack(push, 1)
struct RowHeader{
    uint16_t length;
    uint8_t column_num;
    uint8_t NULLbitmap;
};
#pragma pack(pop)