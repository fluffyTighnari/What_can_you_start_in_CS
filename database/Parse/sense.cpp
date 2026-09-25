//sense.cpp
#include "sense.hpp"
#include "parse.hpp"
#include "Storage_engine/storage.hpp"
#include "Storage_engine/pager.hpp"
#include "Storage_engine/B+Tree/Btree.hpp"
#include "Storage_engine/B+Tree/StackPath.hpp"
#include "Help/Serialize/serialize.hpp"
#include <optional>
#include <vector>
#include <memory>
#include <iostream>
#include <sstream>

static std::vector<std::string> split_string(const std::string& s, char delim){
    std::vector<std::string> result;
    size_t start = 0;
    for(size_t i = 0; i <= s.size(); i++){
        if(i == s.size() || s[i] == delim){
            result.push_back(s.substr(start, i - start));
            start = i + 1;
        }
    }
    return result;
}

std::unique_ptr<SymbolTable> InitSymbolTable(Pager_t& pager){
    auto table = std::make_unique<SymbolTable>();
    if(pager.new_db){
        return table;
    }

    uint32_t sys_root = pager.main_page->system_page;
    if(sys_root == 0){
        return table;
    }

    // 扫描系统 B+ 树，加载所有表元数据
    auto scan_result = Btree_scan(sys_root, pager);
    if(std::holds_alternative<Error>(scan_result)){
        return table;
    }
    std::string& data = std::get<std::string>(scan_result);
    if(data.empty()){
        return table;
    }

    // 逐行反序列化
    const char* p = data.data();
    const char* end = data.data() + data.size();
    while(p < end){
        if(p + sizeof(RowHeader) > end) break;
        RowHeader header = *(const RowHeader*)p;
        if(header.length == 0 || p + sizeof(RowHeader) + header.length > end) break;

        std::string row_data(p, sizeof(RowHeader) + header.length);
        std::string table_name;
        uint32_t root_page;
        std::vector<std::string> col_names, col_types;
        if(deserialize_system(row_data, table_name, root_page, col_names, col_types)){
            std::vector<ColumnDef> cols;
            for(size_t i = 0; i < col_names.size(); i++){
                ColumnDef cd;
                cd.column_name = col_names[i];
                cd.type = col_types[i];
                cols.push_back(cd);
            }
            table->add_table_with_root(table_name, cols, root_page);
        }
        p += sizeof(RowHeader) + header.length;
    }

    return table;
}

const Table* SymbolTable::get_table_message(TableNode& table_name) const{
    auto it = tables.find(table_name.table_name);
    if(it == tables.end()){
        return nullptr;
    }
    else{
        return &it->second;
    }
}

Table* SymbolTable::get_table_message(TableNode& table_name){
    auto it = tables.find(table_name.table_name);
    if(it == tables.end()){
        return nullptr;
    }
    else{
        return &it->second;
    }
}

bool SymbolTable::add_table(TableNode& table_name,std::vector<ColumnDef>& column){
    if(tables.count(table_name.table_name)){
        return false;
    }
    int size = column.size();
    Table newtable;

    for(int i = 0;i < size;i++){
        if(!newtable.add_column(column[i])){
            return false;
        }
    }
    tables[table_name.table_name] = std::move(newtable);
    return true;
}

bool SymbolTable::add_table_with_root(const std::string& table_name, std::vector<ColumnDef>& columns, uint32_t root_page){
    if(tables.count(table_name)){
        return false;
    }
    Table newtable;
    for(size_t i = 0; i < columns.size(); i++){
        if(!newtable.add_column(columns[i])){
            return false;
        }
    }
    tables[table_name] = std::move(newtable);
    root_pages[table_name] = root_page;
    table_num++;
    return true;
}

bool Table::add_column(ColumnDef& column){
    if(index.count(column.column_name)){
        return false;
    }
    if(column.type != "INT" && column.type != "FLOAT" && column.type != "TEXT" && column.type != "VARCHAR"){
        return false;
    }
    columns.emplace_back(column.column_name,column.type,true);
    index[column.column_name] = column_num;
    column_num++;

    return true;
}

bool Table::column_exist(ColumnRef& column) const{
    if(index.count(column.column_name)){
        return true;
    }
    return false;
}

bool Table::type_valid(ColumnRef& column,ValueNode& value) const{
    auto it = index.find(column.column_name);
    if(it == index.end()){
        return false;
    }
    const Column& target_col = columns[it->second];
    if(value.type == ValueNode::Sort::String){
        if(target_col.type == Type::TEXT || target_col.type == Type::VARCHAR){
            return true;
        }
        return false;
    }
    else if(value.type == ValueNode::Sort::Int){
        if(target_col.type == Type::INT){
            return true;
        }
        return false;
    }
    else if(value.type == ValueNode::Sort::Float){
        if(target_col.type == Type::FLOAT){
            return true;
        }
        return false;
    }
    return false;
}

bool Table::type_valid(int idx,ValueNode& value) const{
    if(static_cast<uint32_t>(idx) >= column_num){
        return false;
    }
    const Column& target_col = columns[idx];
    if(value.type == ValueNode::Sort::String){
        if(target_col.type == Type::TEXT || target_col.type == Type::VARCHAR){
            return true;
        }
        return false;
    }
    else if(value.type == ValueNode::Sort::Int){
        if(target_col.type == Type::INT){
            return true;
        }
        return false;
    }
    else if(value.type == ValueNode::Sort::Float){
        if(target_col.type == Type::FLOAT){
            return true;
        }
        return false;
    }
    return false;
}

Result<bool> parse_meaning(Command* command, const Storage& storage_t){
    const SymbolTable* symbol_table = storage_t.acquire_const_SymbolTable();
    bool flag = true;
    std::string error;
    std::visit([&](auto&& n) {
        using T = std::decay_t<decltype(n)>;

        if constexpr (std::is_same_v<T, CreateNode>) {
            flag = parse_create_cmd(n,*symbol_table,error);
        }
        else if constexpr (std::is_same_v<T, DeleteNode>) {
            flag = parse_delete_cmd(n,*symbol_table,error);
        }
        else if constexpr (std::is_same_v<T, SelectNode>) {
            flag = parse_select_cmd(n,*symbol_table,error);
        }
        else if constexpr (std::is_same_v<T, InsertNode>) {
            flag = parse_insert_cmd(n,*symbol_table,error);
        }
        else if constexpr (std::is_same_v<T, UpdateNode>) {
            flag = parse_update_cmd(n,*symbol_table,error);
        }
        else if constexpr (std::is_same_v<T, DropNode>) {
            flag = parse_drop_cmd(n,*symbol_table,error);
        }
        else if constexpr (std::is_same_v<T, ShowTablesNode>) {
            flag = true;
        }
        else {
            flag = false;
            error = "unknown command type";
        }
    }, *command->Main_command);

    if(!flag){
        return Error(error, Error::Layer::Parser);
    }
    return true;
}

bool parse_create_cmd(CreateNode& create,const SymbolTable& symbol_table,std::string& error){
    TableNode tn = create.table_name;
    const auto* p = symbol_table.get_table_message(tn);
    if(p){
        error = "table already exists";
        return false;
    }
    return true;
}

bool parse_delete_cmd(DeleteNode& del,const SymbolTable& symbol_table,std::string& error){
    TableNode tn = del.table_name;
    const auto* p = symbol_table.get_table_message(tn);
    if(!p){
        error = "table does not exist";
        return false;
    }
    if(del.given_expr){
        if(!parse_expr(p,del.expr)){
            error = "failed to parse expr";
            return false;
        }
    }
    return true;
}

bool parse_select_cmd(SelectNode& select,const SymbolTable& symbol_table,std::string& error){
    TableNode tn = select.table_name;
    const auto* p = symbol_table.get_table_message(tn);
    if(!p){
        error = "table does not exist";
        return false;
    }
    if(!select.select_all){
        int size = select.columns.size();
        for(int i = 0;i < size;i++){
            if(!p->column_exist(select.columns[i])){
                error = "column does not exist";
                return false;
            }
        }
    }

    if(select.given_expr){
        if(!parse_expr(p,select.expr)){
            error = "failed to parse expr";
            return false;
        }
    }
    return true;
}

bool parse_insert_cmd(InsertNode& insert,const SymbolTable& symbol_table,std::string& error){
    TableNode tn = insert.table_name;
    const auto* p = symbol_table.get_table_message(tn);
    if(!p){
        error = "table does not exist";
        return false;
    }
    if(!insert.given_column){
        uint32_t column_num = p->get_column_num();
        if(insert.values.size() != column_num){
            error = "column count mismatch";
            return false;
        }
        for(uint32_t i = 0;i < column_num;i++){
            if(insert.values[i].type == ValueNode::Sort::null){
                continue;
            }
            if(!p->type_valid(static_cast<int>(i),insert.values[i])){
                error = "type is invalid";
                return false;
            }
        }
    }
    else{
        size_t size = insert.columns.size();
        for(size_t i = 0;i < size;i++){
            if(insert.values[i].type == ValueNode::Sort::null){
                continue;
            }
            if(!p->type_valid(insert.columns[i],insert.values[i])){
                error = "type is invalid";
                return false;
            }
        }
    }
    return true;
}

bool parse_update_cmd(UpdateNode& update,const SymbolTable& symbol_table,std::string& error){
    TableNode tn = update.table_name;
    const auto* p = symbol_table.get_table_message(tn);
    if(!p){
        error = "table does not exist";
        return false;
    }
    if(!parse_expr(p,update.set)){
        error = "failed to parse set expr";
        return false;
    }

    if(update.given_expr){
        if(!parse_expr(p,update.expr)){
            error = "failed to parse where expr";
            return false;
        }
    }
    return true;
}

bool parse_drop_cmd(DropNode& drop,const SymbolTable& symbol_table,std::string& error){
    TableNode tn = drop.table_name;
    const auto* p = symbol_table.get_table_message(tn);
    if(!p){
        error = "table does not exist";
        return false;
    }
    return true;
}

bool parse_expr(const Table* table,PredicateNode& expr){
    if(expr.op != '='){
        return false;
    }
    if(!table->type_valid(expr.leftexpr,expr.rightexpr)){
        return false;
    }
    return true;
}

uint32_t SymbolTable::get_root_page(const std::string& table_name) const{
    auto it = root_pages.find(table_name);
    return (it != root_pages.end()) ? it->second : 0;
}

void SymbolTable::set_root_page(const std::string& table_name, uint32_t page){
    root_pages[table_name] = page;
}

bool SymbolTable::remove_table(const std::string& table_name){
    size_t removed = tables.erase(table_name);
    if(removed == 0) return false;
    root_pages.erase(table_name);
    table_num--;
    return true;
}
