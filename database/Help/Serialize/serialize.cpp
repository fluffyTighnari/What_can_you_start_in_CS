#include "serialize.hpp"
#include "Parse/parse.hpp"
#include "Help/Show/show.hpp"
#include "Parse/sense.hpp"
#include <string>
#include <cstring>


void serialize(std::string& input, InsertNode& insert, const Table* table){
    RowHeader header;
    header.column_num = table->get_column_num();
    header.NULLbitmap = 0;
    header.length = 0;

    if(!insert.given_column){//insert into table_name values(...,...,...);
        for(uint32_t i = 0;i < header.column_num;i++){
            auto col = table->get_column(i);
            if(col->type == Type::INT){
                header.length += 4;
                input.append((const char*)&insert.values[i].Inum,sizeof(int));
            }
            else if(col->type == Type::FLOAT){
                header.length += 4;
                input.append((const char*)&insert.values[i].Fnum,sizeof(float));
            }
            else if(col->type == Type::TEXT || col->type == Type::VARCHAR){
                size_t size = insert.values[i].str.size();
                header.length += size + 4;
                input.append((const char*)&size,sizeof(int));
                input.append(insert.values[i].str);
            }
        }
    }
    else{//insert into table_name (...,...) values(...,...);
        int j = 0;
        for(uint32_t i = 0;i < header.column_num;i++){
            auto col = table->get_column(i);
            int flag = -1;
            if(j < insert.columns.size() && insert.columns[j].column_name == col->column_name){
                flag = j;
                j++;
            }
            if(flag == -1){
                header.NULLbitmap |= (1 << i);
            }
            else{
                if(col->type == Type::INT){
                    header.length += 4;
                    input.append((const char*)&insert.values[flag].Inum,sizeof(int));
                }
                else if(col->type == Type::FLOAT){
                    header.length += 4;
                    input.append((const char*)&insert.values[flag].Fnum,sizeof(float));
                }
                else if(col->type == Type::TEXT || col->type == Type::VARCHAR){
                    size_t size = insert.values[flag].str.size();
                    header.length += size + 4;
                    input.append((const char*)&size,sizeof(int));
                    input.append(insert.values[flag].str);
                }
            }
        }
    }
    input.insert(0,std::string((const char*)&header,sizeof(header)));
}


void deserialize(const Table* table,std::vector<uint32_t>& need,std::string& input, Info* output){
    //[RowHeader][int:...][size][string:...][RowHeader][...];
    if(output == nullptr) {
        return;
    }
    const char* p = input.data();
    const char* end = input.data() + input.size();
    bool first_row = true;
    while(p < end){
        RowHeader header = *(const RowHeader*)p;
        p += sizeof(RowHeader);

        uint32_t curindex = 0;
        for(uint32_t i = 0;i < header.column_num;i++){
            auto col = table->get_column(i);
            if(curindex >= need.size() || need[curindex] != i){
                if(col->type == Type::TEXT || col->type == Type::VARCHAR){
                    int size = *(const int*)p;
                    p += sizeof(int)*(1 + size);
                }
                else{
                    p += sizeof(int);
                }
            }
            else{
                if((header.NULLbitmap & (1 << i)) != 0){
                    output->values.push_back("NULL");
                }
                else{
                    if(col->type == Type::INT){
                        output->values.push_back(std::to_string(*(const int*)p));
                        p += sizeof(int);
                    }
                    else if(col->type == Type::FLOAT){
                        output->values.push_back(std::to_string(*(const float*)p));
                        p += sizeof(float);
                    }
                    else if(col->type == Type::TEXT || col->type == Type::VARCHAR){
                        int size = *(const int*)p;
                        if(size < 0 || size > (int)(end - p)) {
                            break;
                        }
                        p += sizeof(int);
                        std::string val;
                        while(size--){
                           val.append(1,*p++);
                        }
                        output->values.push_back(val);
                    }
                }
                if(first_row){
                    output->columns.push_back(col->column_name);
                    std::string type;
                    switch(col->type){
                        case Type::INT:
                        type = "INT";
                        break;
                        case Type::FLOAT:
                        type = "FLOAT";
                        break;
                        case Type::TEXT:
                        type = "TEXT";
                        break;
                        case Type::VARCHAR:
                        type = "VARCHAR";
                        break;
                    }
                    output->types.push_back(type);
                }
                curindex++;
            }
        }
        first_row = false;
    }
}

bool serialize_row_from_strings(const Table* table, const std::vector<std::string>& values, std::string& output){
    if(!table || values.size() != table->get_column_num()){
        return false;
    }
    RowHeader header;
    header.column_num = table->get_column_num();
    header.NULLbitmap = 0;
    header.length = 0;

    std::string body;
    for(uint32_t i = 0; i < header.column_num; i++){
        auto col = table->get_column(i);
        if(!col){
            return false;
        }
        if(col->type == Type::INT){
            int val = std::stoi(values[i]);
            header.length += 4;
            body.append((const char*)&val, sizeof(int));
        }
        else if(col->type == Type::FLOAT){
            float val = std::stof(values[i]);
            header.length += 4;
            body.append((const char*)&val, sizeof(float));
        }
        else if(col->type == Type::TEXT || col->type == Type::VARCHAR){
            int size = (int)values[i].size();
            header.length += size + 4;
            body.append((const char*)&size, sizeof(int));
            body.append(values[i]);
        }
    }
    output.assign((const char*)&header, sizeof(header));
    output.append(body);
    return true;
}

void serialize_system(const std::string& table_name, CreateNode& create, std::string& input, uint32_t newpage){
    //[RowHeader][int:name_len][string:table_name][int:page_num][int:column_num][int:size1][string:columns][int:size2][string:types]
    RowHeader header;
    header.column_num = create.columns.size();
    header.NULLbitmap = 0;
    header.length = 0;

    int name_len = (int)table_name.size();
    input.append((const char*)&name_len, sizeof(int));
    header.length += 4;
    input.append(table_name);
    header.length += name_len;

    input.append((const char*)&newpage,sizeof(int));
    header.length += 4;

    int size = header.column_num;
    input.append((const char*)&size,sizeof(int));
    header.length += 4;

    std::string column;
    for(auto& a : create.columns){
        column.append(a.column_name);
        column.append(1,',');
    }
    column.pop_back();
    int size1 = column.size();
    input.append((const char*)&size1,sizeof(int));
    input.append(column);
    header.length += size1 + 4;
    
    std::string type;
    for(auto& a : create.columns){
        type.append(a.type);
        type.append(1,',');
    }
    type.pop_back();
    int size2 = type.size();
    input.append((const char*)&size2,sizeof(int));
    input.append(type);
    header.length += size2 + 4;

    input.insert(0,std::string((const char*)&header,sizeof(header)));
}

bool deserialize_system(const std::string& input, std::string& out_table_name, uint32_t& out_root_page,
                        std::vector<std::string>& out_col_names,
                        std::vector<std::string>& out_col_types){
    if(input.size() < sizeof(RowHeader) + 12) return false;
    const char* p = input.data();
    const char* end = input.data() + input.size();

    RowHeader header = *(const RowHeader*)p;
    p += sizeof(RowHeader);
    if(p + 4 > end) return false;

    int name_len = *(const int*)p;
    p += sizeof(int);
    if(p + name_len > end) return false;
    out_table_name.assign(p, name_len);
    p += name_len;

    if(p + 8 > end) return false;

    out_root_page = *(const int*)p;
    p += sizeof(int);

    int col_num = *(const int*)p;
    p += sizeof(int);

    if(p + 4 > end) return false;
    int names_len = *(const int*)p;
    p += sizeof(int);
    if(p + names_len > end) return false;
    std::string names_str(p, names_len);
    p += names_len;

    if(p + 4 > end) return false;
    int types_len = *(const int*)p;
    p += sizeof(int);
    if(p + types_len > end) return false;
    std::string types_str(p, types_len);
    p += types_len;

    // 按逗号分割列名
    out_col_names.clear();
    size_t start = 0;
    for(size_t i = 0; i <= names_str.size(); i++){
        if(i == names_str.size() || names_str[i] == ','){
            out_col_names.push_back(names_str.substr(start, i - start));
            start = i + 1;
        }
    }

    // 按逗号分割类型
    out_col_types.clear();
    start = 0;
    for(size_t i = 0; i <= types_str.size(); i++){
        if(i == types_str.size() || types_str[i] == ','){
            out_col_types.push_back(types_str.substr(start, i - start));
            start = i + 1;
        }
    }

    if((int)out_col_names.size() != col_num || (int)out_col_types.size() != col_num){
        return false;
    }
    return true;
}