#pragma once
#include <variant>
#include <string>

/*利用模板定义通用Result*/

struct Error{

    enum class Layer{//报错层号
        Parser,
        Executor,
        Pager,
        Btree
    };

    Layer layer;
    std::string message;
    Error(const std::string& input,const Layer& where){
        message = input;
        layer = where;
    }
    
    std::string error_message(){
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
};

template<typename T>
using Result = std::variant<T,Error>;