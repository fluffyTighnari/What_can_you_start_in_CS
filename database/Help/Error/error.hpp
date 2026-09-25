//error.hpp
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
    
    std::string error_message();
};

template<typename T>
using Result = std::variant<T,Error>;