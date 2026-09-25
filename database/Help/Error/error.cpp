#include "error.hpp"
#include <string>

std::string Error::error_message(){
    std::string where_error{};
    switch(layer){
        case Layer::Parser:
        where_error = "解析层";
        break;

        case Layer::Executor:
        where_error = "执行层";
        break;

        case Layer::Pager:
        where_error = "存储层";
        break;

        case Layer::Btree:
        where_error = "B+树层";
    }
    return std::string(where_error + "发生错误：" + message);
}