//sense.hpp
#pragma once

#include "Help/Error/error.hpp"
#include <unordered_map>
#include <string>
#include <vector>
#include <cstdint>
#include <optional>
#include <memory>
#include <sstream>
/*利用符号表进行语义检查*/
struct Pager_t;
struct token;

struct TableNode;
struct ColumnRef;
struct ColumnDef;
struct PredicateNode;
struct ValueNode;

struct CreateNode;
struct SelectNode;
struct DeleteNode;
struct UpdateNode;
struct InsertNode;
struct DropNode;
struct ShowTablesNode;
struct Command;
class Storage;

enum class Type{
    INT,
    FLOAT,
    TEXT,
    VARCHAR
};

class Column{
    public:
    std::string column_name;
    Type type;
    bool can_NULL; /*暂时默认全支持NULL*/

    Column(std::string& a,std::string& b,bool c):column_name(a),can_NULL(c){
        if(b == "INT"){
            type = Type::INT;
        }
        else if(b == "FLOAT"){
            type = Type::FLOAT;
        }
        else if(b == "TEXT"){
            type = Type::TEXT;
        }
        else if(b == "VARCHAR"){
            type = Type::VARCHAR;
        }
    }
};

class Table{
    public: 
    bool add_column(ColumnDef& column); /*暂时没有用*/

    bool column_exist(ColumnRef& column_name) const;

    bool type_valid(ColumnRef& column_name,ValueNode& value) const;

    bool type_valid(int index,ValueNode& value) const;

    uint32_t get_column_num() const{
        return column_num;
    }

    const Column* get_column(uint32_t index) const{
        return index < column_num ? &columns[index] : nullptr;
    }
    
    private:
    uint32_t column_num = 0;
    std::vector<Column> columns; /*默认主键：下标0*/
    std::unordered_map<std::string,uint32_t> index;
};

class SymbolTable{
    public:
    const Table* get_table_message(TableNode& table_name) const;

    Table* get_table_message(TableNode& table_name);

    bool add_table(TableNode& table_name,std::vector<ColumnDef>& column);

    bool add_table_with_root(const std::string& table_name, std::vector<ColumnDef>& columns, uint32_t root_page);

    uint32_t get_table_num() const{
        return table_num;
    }

    uint32_t get_root_page(const std::string& table_name) const;

    void set_root_page(const std::string& table_name, uint32_t page);

    const std::unordered_map<std::string,uint32_t>& get_root_pages() const{
        return root_pages;
    }

    bool remove_table(const std::string& table_name);

    private:
    uint32_t table_num = 0;
    std::unordered_map<std::string,Table> tables;
    std::unordered_map<std::string,uint32_t> root_pages;
};

std::unique_ptr<SymbolTable> InitSymbolTable(Pager_t& pager);

Result<bool> parse_meaning(Command* command, const Storage& storage_t);

bool parse_create_cmd(CreateNode& create,const SymbolTable& symbol_table,std::string& error);

bool parse_delete_cmd(DeleteNode& del,const SymbolTable& symbol_table,std::string& error);

bool parse_select_cmd(SelectNode& select,const SymbolTable& symbol_table,std::string& error);

bool parse_insert_cmd(InsertNode& insert,const SymbolTable& symbol_table,std::string& error);

bool parse_update_cmd(UpdateNode& update,const SymbolTable& symbol_table,std::string& error);

bool parse_drop_cmd(DropNode& drop,const SymbolTable& symbol_table,std::string& error);

bool parse_expr(const Table* table,PredicateNode& expr);

