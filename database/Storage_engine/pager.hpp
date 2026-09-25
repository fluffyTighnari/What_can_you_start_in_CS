#pragma once

#ifdef BTREE_USE_MOCK_PAGER
#include "B+Tree/mockpager.hpp"
#else

#include <string>
#include <unordered_map>
#include <cstdint>
#include <memory>
#include <unistd.h>
#include <cstdio>
#include <vector>

#ifdef DEBUG
#include "../Help/Error/error.hpp"
#else
#include "Help/Error/error.hpp"
#endif

constexpr uint32_t PAGE_SIZE = 4096U;
constexpr uint32_t MAX_CACHED_PAGES = 64;
constexpr uint32_t MAX_DB_PAGES = 512;
constexpr uint32_t MAGIC = 0x66666666;

typedef struct Page{
    uint32_t page_num;
    char data[PAGE_SIZE];
    bool dirty = false;
    Page* pre = nullptr;
    Page* next = nullptr;
}Page;

typedef struct mainpage{
    uint32_t magic;
    uint32_t page_size;
    uint32_t version;
    uint32_t pages_num;
    uint32_t bitmap[16]; /*max_pages : 16 * 32 = 512*/
    uint32_t system_page; /*system_table 的 root_page*/
    uint8_t  reserved[PAGE_SIZE - 21*sizeof(uint32_t)];
}mainpage;

struct Pager_t{
    public:
    explicit Pager_t(const char* db_name);

    ~Pager_t();

    int get_fd() const;

    bool read_page(uint32_t page_num,char* buffer);

    bool write_page(uint32_t page_num);

    bool update_page(uint32_t page_num,const char* buffer);

    bool delete_page(uint32_t page_num);

    uint32_t alloc_new_page();

    bool is_page_allocated(uint32_t page_num) const;

    bool flush_page();

    bool new_db = false;/*如果是新库，需要在初始化符号表时写入新系统表*/

    std::unique_ptr<mainpage> main_page;

    private:

    void move_to_head(Page* page);

    void evict_if_needed();

    void remove_from_list(Page* page);

    int fd;
    std::unordered_map<uint32_t,Page*> hash;
    Page* head = nullptr;
    Page* tail = nullptr;
    uint32_t current_page = 0;
};

Result<std::unique_ptr<Pager_t>> InitPager(const std::string& db_name);

#endif // BTREE_USE_MOCK_PAGER
