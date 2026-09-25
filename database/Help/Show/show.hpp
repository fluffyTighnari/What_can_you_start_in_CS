#pragma once

#include <vector>
#include <string>

struct Info{
    bool need_print = false;
    std::vector<std::string> columns;
    std::vector<std::string> types;
    std::vector<std::string> values;
    std::string output;

    void print_message();
};
