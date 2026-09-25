//parse.hpp
#pragma once

#include "Help/Error/error.hpp"
#include "tokenize.hpp"
#include <variant>
#include <vector>
#include <string>
#include <memory>

/*利用AST存储std::variant 结合 std::visit
多结构体构建抽象语法树*/

/*待更新：彻底解耦语法和语义，在语义分析层进行语义绑定：
Identifier/Literal*/

struct TableNode {
    std::string table_name;
};

struct ColumnRef {
    std::string column_name;
};

struct ColumnDef {
    std::string column_name;
    std::string type;
};

struct ValueNode {
    enum class Sort {
        String,
        Int,
        Float,
        null
    };

    Sort type;
    int Inum;
    float Fnum;
    std::string str;

    ValueNode(int a) : Inum(a), type(Sort::Int) {}
    ValueNode(float a) : Fnum(a), type(Sort::Float) {}
    ValueNode() : type(Sort::null) {}
    ValueNode(const std::string& a) : str(a), type(Sort::String) {}

    #ifdef DEBUG
    void print_value();
    #endif

};

/*
using ExprNode = std::variant<
    TableNode,
    ColumnRef,
    PredicateNode,
    ValueNode
>;
*/

struct PredicateNode {
    char op;
    ColumnRef leftexpr;
    ValueNode rightexpr;
};

struct CreateNode {
    TableNode table_name;
    std::vector<ColumnDef> columns;
};

struct SelectNode {
    TableNode table_name;
    bool select_all = true;
    bool given_expr = false;
    std::vector<ColumnRef> columns;
    PredicateNode expr;
};

struct DeleteNode {
    TableNode table_name;
    bool given_expr = false;
    PredicateNode expr;
};

struct UpdateNode {
    TableNode table_name;
    PredicateNode set;
    bool given_expr = false;
    PredicateNode expr;
};

struct InsertNode {
    TableNode table_name;
    bool given_column = false;
    std::vector<ColumnRef> columns;
    std::vector<ValueNode> values;
};

struct DropNode {
    TableNode table_name;
};

struct ShowTablesNode {
};

using ASTNode = std::variant<
    CreateNode,
    SelectNode,
    DeleteNode,
    UpdateNode,
    InsertNode,
    DropNode,
    ShowTablesNode,
    ColumnDef,
    ColumnRef,
    TableNode,
    PredicateNode,
    ValueNode
>;

struct Command {
    std::unique_ptr<ASTNode> Main_command;

    #ifdef DEBUG
    void print_message();
    #endif
};


Result<std::unique_ptr<Command>> parse_command(std::string& input);

static bool parse_select(std::vector<Token>& tokens,SelectNode& sel,std::string& error);

static bool parse_create(std::vector<Token>& tokens,CreateNode& cre,std::string& error);

static bool parse_delete(std::vector<Token>& tokens,DeleteNode& del,std::string& error);

static bool parse_insert(std::vector<Token>& tokens,InsertNode& ins,std::string& error);

static bool parse_update(std::vector<Token>& tokens,UpdateNode& upd,std::string& error);

static bool parse_drop(std::vector<Token>& tokens,DropNode& drop,std::string& error);

static bool parse_show_tables(std::vector<Token>& tokens,ShowTablesNode& show,std::string& error);

static bool parse_ColumnRef(std::vector<Token>& tokens,std::vector<ColumnRef>& input,int& cur,std::string& error);

static bool parse_ColumnDef(std::vector<Token>& tokens,std::vector<ColumnDef>& input,int& cur,std::string& error);

static bool parse_from(std::vector<Token>& tokens,TableNode& input,int& cur,std::string& error);

static bool parse_where(std::vector<Token>& tokens,PredicateNode& input,int& cur,std::string& error);

static bool parse_value(std::vector<Token>& tokens,std::vector<ValueNode>& input,int& cur,std::string& error);

static Token eat(std::vector<Token>& tokens, int& cur);

static bool match(Token& t, Token::Type type);