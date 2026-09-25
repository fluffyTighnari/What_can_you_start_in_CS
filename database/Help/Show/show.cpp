#include "show.hpp"
#include <iomanip>
#include <iostream>
#include <vector>
#include <string>

void Info::print_message(){
    size_t col_count = columns.size();
    if (col_count == 0 || col_count != types.size()) {
        std::cout << "Empty result." << std::endl;
        return;
    }

    size_t row_count = values.size() / col_count;

    std::vector<size_t> widths(col_count);
    for (size_t i = 0; i < col_count; ++i) {
        widths[i] = std::max(columns[i].size(), types[i].size()) + 2;
    }
    for (size_t r = 0; r < row_count; ++r) {
        for (size_t i = 0; i < col_count; ++i) {
            size_t val_len = values[r * col_count + i].size();
            if (val_len + 2 > widths[i]) {
                widths[i] = val_len + 2;
            }
        }
    }

    auto print_line = [&]() {
        for (size_t w : widths)
            std::cout << "+" << std::string(w, '-');
        std::cout << "+" << std::endl;
    };

    print_line();

    for (size_t i = 0; i < col_count; ++i)
        std::cout << "| " << std::left << std::setw(widths[i]-1) << columns[i];
    std::cout << "|" << std::endl;

    print_line();

    for (size_t i = 0; i < col_count; ++i)
        std::cout << "| " << std::left << std::setw(widths[i]-1) << types[i];
    std::cout << "|" << std::endl;

    print_line();

    for (size_t r = 0; r < row_count; ++r) {
        for (size_t i = 0; i < col_count; ++i)
            std::cout << "| " << std::left << std::setw(widths[i]-1) << values[r * col_count + i];
        std::cout << "|" << std::endl;
    }

    print_line();

    std::cout << row_count << " row(s) in set." << std::endl;
}