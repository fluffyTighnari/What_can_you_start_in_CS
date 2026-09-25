//tokenize.cpp
#include "tokenize.hpp"
#include <sstream>
#include <vector>
#include <string>
#include <iostream>

Result<std::vector<Token>> tokenize(std::string& input){
    std::vector<Token> token{};
    std::stringstream ss(input);
    char c;
    while(ss.get(c)){
        if(isspace(c)){
            continue;
        }
        if(c == '('){
            token.emplace_back(Token::Type::LPAREN,"(",0,0);
        }
        else if(c == ')'){
            token.emplace_back(Token::Type::RPAREN,")",0,0);
        }
        else if(c == ','){
            token.emplace_back(Token::Type::COMMA,",",0,0);
        }
        else if(c == ';'){
            token.emplace_back(Token::Type::END,";",0,0);
        }
        else if(isdigit(c)){
            std::string record;
            bool flag = false;
            record.push_back(c);
            while(ss.get(c) && (isdigit(c) || c == '.')){
                if(c == '.'){
                    flag = true;
                }
                record.push_back(c);
            }
            ss.unget();
            
            if(flag){
                token.emplace_back(Token::Type::FLOAT,"",0,stod(record));
            }
            else{
                token.emplace_back(Token::Type::INT,"",stoi(record),0);
            }
        }
        else if(c == '"'){
            std::string record;
            while(ss.get(c) && c != '"'){
                record.push_back(c);
            }
            token.emplace_back(Token::Type::STRING,record,0,0);
        }
        else if(c == '\''){
            std::string record;
            while(ss.get(c) && c != '\''){
                record.push_back(c);
            }
            token.emplace_back(Token::Type::STRING,record,0,0);
        }
        else if(c == '*'){
            token.emplace_back(Token::Type::IDENT,"*",0,0);
        }
        else if(c == '=' || c == '<' || c == '>'){
            std::string op{c};
            token.emplace_back(Token::Type::OP,op,0,0);
        }
        else if(isalpha(c) || c == '_'){
            std::string word;
            word += toupper(c);
            while(ss.get(c) && (isalnum(c) || c == '_')){
                word += toupper(c);
            }
            ss.unget();
            Token t(Token::Type::COMMA,word,0,0);
            if(word == "INSERT"){
                t.type = Token::Type::INSERT;
            }
            else if(word == "INTO"){
                t.type = Token::Type::INTO;
            }
            else if(word == "VALUES"){
                t.type = Token::Type::VALUES;
            }
            else if(word == "WHERE"){
                t.type = Token::Type::WHERE;
            }
            else if(word == "DELETE"){
                t.type = Token::Type::DELETE;
            }
            else if(word == "CREATE"){
                t.type = Token::Type::CREATE;
            }
            else if(word == "UPDATE"){
                t.type = Token::Type::UPDATE;
            }
            else if(word == "DROP"){
                t.type = Token::Type::DROP;
            }
            else if(word == "SHOW"){
                t.type = Token::Type::SHOW;
            }
            else if(word == "SELECT"){
                t.type = Token::Type::SELECT;
            }
            else if(word == "FROM"){
                t.type = Token::Type::FROM;
            }
            else if(word == "TABLE"){
                t.type = Token::Type::TABLE;
            }
            else if(word == "SET"){
                t.type = Token::Type::SET;
            }
            else{
                t.type = Token::Type::IDENT;
            }
            t.str = word;
            token.push_back(t);
        }
        else{
            std::string temp = "invalid symbol：";
            temp.append(1,c);
            return Error(temp,Error::Layer::Parser);
        }
    }
    return token;
}


#ifdef DEBUG
void Token::print_message(){
    std::cout << type_to_string.at(type) << ' ';
    if(type == Type::INT){
        std::cout << Inum;
    }
    else if(type == Type::FLOAT){
        std::cout << Fnum;
    }
    else if(type == Type::STRING){
        std::cout << str;
    }
    std::cout << '\n';
}
#endif