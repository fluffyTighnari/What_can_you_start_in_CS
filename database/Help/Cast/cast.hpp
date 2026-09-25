#pragma once

#include <cstddef>
#include <cstring>

template <typename T>
inline T* page_cast(char* page_data) {
    static_assert(std::is_standard_layout_v<T>,"page_cast:nonstandard struct is not supported");
    static_assert(sizeof(T) <= 4096,"page_cast:the size of struct is bigger than a page");
    return reinterpret_cast<T*>(page_data);
}

template <typename T>
inline const T* page_cast(const char* page_data) {
    static_assert(std::is_standard_layout_v<T>,"page_cast:page_cast:nonstandard struct is not supported");
    static_assert(sizeof(T) <= 4096,"page_cast:the size of struct is bigger than a page");
    return reinterpret_cast<const T*>(page_data);
}