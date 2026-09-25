#pragma once
#include <unordered_map>
#include <cstdint>
#include <memory>
#include <cstring>
#include <string>

#define PAGE_SIZE 4096U
constexpr uint32_t MAX_DB_PAGES = 512;
constexpr uint32_t MAGIC = 0x66666666;

struct mainpage {
    uint32_t magic;
    uint32_t page_size;
    uint32_t version;
    uint32_t pages_num;
    uint32_t bitmap[16];
    uint32_t system_page;
    uint8_t  reserved[PAGE_SIZE - 21 * sizeof(uint32_t)];
};

class Pager_t {
public:
    std::unordered_map<uint32_t, std::unique_ptr<char[]>> pages;
    uint32_t next_page = 1;
    bool new_db = true;
    std::unique_ptr<mainpage> main_page;

    Pager_t();

    int get_fd() const;

    bool read_page(uint32_t page_num, char* buffer);

    bool write_page(uint32_t page_num, const char* buffer);

    bool update_page(uint32_t page_num, const char* buffer);

    bool delete_page(uint32_t page_num);

    uint32_t alloc_new_page();

    bool is_page_allocated(uint32_t page_num) const;

    bool flush_page();

    void dump_all();
};
