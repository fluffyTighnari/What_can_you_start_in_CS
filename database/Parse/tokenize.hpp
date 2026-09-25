//tokenize.hpp
#pragma once

#include "Help/Error/error.hpp"
#include <vector>
#include <string>
#include <unordered_map>

/*利用手动状态机构建词法解析器*/

typedef struct Token{
    enum class Type{
        INSERT,INTO,VALUES,WHERE,SELECT,DELETE,CREATE,UPDATE,DROP,SHOW,TABLE,FROM,SET,
        IDENT,/*column_name、table_name*/
        INT,
        FLOAT,
        STRING,
        LPAREN,
        RPAREN,
        COMMA,
        OP,
        END
    };
    Type type;
    std::string str;
    int Inum;
    float Fnum;

    Token(Type a,const std::string& b,int c,float d): type(a),str(b),Inum(c),Fnum(d){}

    #ifdef DEBUG
    void print_message();
    #endif
    
}Token;

Result<std::vector<Token>> tokenize(std::string& input);

#ifdef DEBUG
const std::unordered_map<Token::Type, std::string> type_to_string = {
    {Token::Type::INSERT,   "INSERT"},
    {Token::Type::INTO,     "INTO"},
    {Token::Type::VALUES,   "VALUES"},
    {Token::Type::WHERE,    "WHERE"},
    {Token::Type::SELECT,   "SELECT"},
    {Token::Type::DELETE,   "DELETE"},
    {Token::Type::CREATE,   "CREATE"},
    {Token::Type::UPDATE,   "UPDATE"},
    {Token::Type::DROP,     "DROP"},
    {Token::Type::SHOW,     "SHOW"},
    {Token::Type::TABLE,    "TABLE"},
    {Token::Type::FROM,     "FROM"},
    {Token::Type::SET,      "SET"},
    {Token::Type::IDENT,    "IDENT"},
    {Token::Type::INT,      "INT"},
    {Token::Type::FLOAT,    "FLOAT"},
    {Token::Type::STRING,   "STRING"},
    {Token::Type::LPAREN,   "LPAREN"},
    {Token::Type::RPAREN,   "RPAREN"},
    {Token::Type::COMMA,    "COMMA"},
    {Token::Type::OP,       "OP"},
    {Token::Type::END,      "END"}
};
#endif