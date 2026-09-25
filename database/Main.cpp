//Main.cpp
/*基本原则：每个表只允许以int类型为主键（为了方便）
where谓词只支持单个等值条件（为了简化）
支持select/create/insert/delete/update
*/
#include <iostream>
#include <string>
#include <cstdio>
#include <memory>
#include <variant>
#include "Parse/parse.hpp"
#include "Storage_engine/storage.hpp"
#include "Execute/executor.hpp"
#include "Storage_engine/pager.hpp"
#include "Parse/sense.hpp"

#ifdef _WIN32
#include <windows.h>
#endif

using std::cout;
using std::string;
using std::unique_ptr;
using std::move;

static void setup_console_encoding(){
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif
}

int main(){
    setup_console_encoding();
    auto pager_result = InitPager("sql.db");
    if(std::holds_alternative<Error>(pager_result)){
        auto& err = std::get<Error>(pager_result);
        std::cerr << err.error_message() << std::endl;
        return -1;
    }
    auto pager = std::move(std::get<unique_ptr<Pager_t>>(pager_result));

    auto symbol_table = InitSymbolTable(*pager);

    auto storage_result = InitStorage(std::move(pager), std::move(symbol_table));
    if(std::holds_alternative<Error>(storage_result)){
        auto& err = std::get<Error>(storage_result);
        std::cerr << err.error_message() << std::endl;
        return -1;
    }
    auto storage = std::move(std::get<unique_ptr<Storage>>(storage_result));

    printf("Database started successfully.\nSupported commands:\n"
           "  create table table_name (col1 type1, col2 type2...);\n"
           "    types: int, float, text, varchar (first column is primary key)\n"
           "  drop table table_name;\n"
           "  show tables;\n"
           "  insert into table_name (col1,col2,...) values(val1,val2,...);\n"
           "  insert into table_name values(val1,val2,...);\n"
           "  select * from table_name [where col=val];\n"
           "  select col1,col2 from table_name [where col=val];\n"
           "  delete from table_name [where col=val];\n"
           "  update table_name set col=val where col=val;\n"
           "Note: predicate only supports single equality on primary key.\n\n");

    while(true){
        cout << "SQLite >>";
        string input;
        if(!getline(std::cin, input)){
            break;
        }
        if(input.empty()){
            continue;
        }

        auto command_result = parse_command(input);
        if(std::holds_alternative<Error>(command_result)){
            auto& err = std::get<Error>(command_result);
            std::cout << err.error_message() << std::endl;
            continue;
        }
        auto command = std::move(std::get<unique_ptr<Command>>(command_result));

        auto meaning_result = parse_meaning(command.get(), *storage);
        if(std::holds_alternative<Error>(meaning_result)){
            auto& err = std::get<Error>(meaning_result);
            std::cout << err.error_message() << std::endl;
            continue;
        }

        auto exec_result = execute_command(command.get(), storage.get());
        if(std::holds_alternative<Error>(exec_result)){
            auto& err = std::get<Error>(exec_result);
            std::cout << err.error_message() << std::endl;
            continue;
        }
        auto info = std::move(std::get<unique_ptr<Info>>(exec_result));

        if(info->need_print){
            info->print_message();
        }
        if(!info->output.empty()){
            std::cout << info->output << std::endl;
        }
    }
    return 0;
}
