#include "pager.hpp"
#include <memory>
#include <cstring>
#include <cstdio>

#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#ifndef _S_IREAD
#define _S_IREAD 0x0100
#endif
#ifndef _S_IWRITE
#define _S_IWRITE 0x0080
#endif
#else
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#endif

Pager_t::Pager_t(const char* db_name){
#ifdef _WIN32
    fd = _open(db_name, _O_RDWR | _O_CREAT | _O_BINARY, _S_IREAD | _S_IWRITE);
#else
    fd = open(db_name, O_RDWR | O_CREAT, 0644);
#endif
    main_page = std::make_unique<mainpage>();
}

Pager_t::~Pager_t(){
    flush_page();
    if(fd >= 0){
#ifdef _WIN32
        _close(fd);
#else
        close(fd);
#endif
    }
    for(auto& pair : hash){
        delete pair.second;
    }
    hash.clear();
}

bool Pager_t::flush_page(){
    for(auto& [page_num,page] : hash){
        if(page->dirty){
            if(!write_page(page_num)){
                return false;
            }
        }
    }
#ifdef _WIN32
    int ret = _lseek(fd, 0, SEEK_SET);
    if(ret == -1) return false;
    int n = _write(fd, main_page.get(), PAGE_SIZE);
    if(n != PAGE_SIZE) return false;
#else
    ssize_t ret = pwrite(fd, main_page.get(), PAGE_SIZE, 0);
    if(ret != PAGE_SIZE) return false;
#endif
    return true;
}

void Pager_t::move_to_head(Page* page) {
    if(page == head){
        return;
    }
    remove_from_list(page);
    page->next = head;
    page->pre = nullptr;
    if(head){
        head->pre = page;
    }
    head = page;
    if(!tail){
        tail = page;
    }
}

void Pager_t::remove_from_list(Page* page) {
    if(page->pre){
        page->pre->next = page->next;
    }
    else{
        head = page->next;
    }
    if(page->next){
        page->next->pre = page->pre;
    }
    else{
        tail = page->pre;
    }
    page->pre = page->next = nullptr;
}

void Pager_t::evict_if_needed() {
    while (hash.size() > MAX_CACHED_PAGES && tail) {
        Page* temp = tail;
        if(temp->dirty){
            write_page(temp->page_num);
        }
        remove_from_list(temp);
        hash.erase(temp->page_num);
        delete temp;
    }
}

bool Pager_t::is_page_allocated(uint32_t page_num) const {
    if (page_num == 0) return true;
    if (page_num >= MAX_DB_PAGES) return false;
    uint32_t idx = page_num / 32;
    uint32_t bit = page_num % 32;
    return (main_page->bitmap[idx] >> bit) & 1;
}

Result<std::unique_ptr<Pager_t>> InitPager(const std::string& db_name){
    auto target_pager = std::make_unique<Pager_t>(db_name.c_str());
    if(target_pager->get_fd() < 0){
        std::string temp = "failed to open the file:";
        temp.append(db_name);
        return Error(temp,Error::Layer::Pager);
    }

#ifdef _WIN32
    struct _stati64 st{};
    _fstati64(target_pager->get_fd(), &st);
    __int64 file_size = st.st_size;
#else
    struct stat st{};
    fstat(target_pager->get_fd(),&st);
    off_t file_size = st.st_size;
#endif

    std::string error;
    if(file_size == 0){
        target_pager->main_page->magic = 0x66666666;
        target_pager->main_page->page_size = PAGE_SIZE;
        target_pager->main_page->version = 1;
        target_pager->main_page->pages_num = 0;
        target_pager->main_page->system_page = 0;
        target_pager->new_db = true;

#ifdef _WIN32
        _lseek(target_pager->get_fd(), 0, SEEK_SET);
        int res = _write(target_pager->get_fd(), (void*)target_pager->main_page.get(), PAGE_SIZE);
        if(res < 0){
            return Error("failed to write the file",Error::Layer::Pager);
        }
#else
        ssize_t res = pwrite(target_pager->get_fd(),(void*)target_pager->main_page.get(),PAGE_SIZE,0);
        if(res < 0){
            return Error("failed to write the file",Error::Layer::Pager);
        }
#endif
    }
    else{
#ifdef _WIN32
        _lseek(target_pager->get_fd(), 0, SEEK_SET);
        int n = _read(target_pager->get_fd(), (void*)target_pager->main_page.get(), PAGE_SIZE);
        if(n < 0){
            return Error("failed to read the file",Error::Layer::Pager);
        }
#else
        ssize_t n = pread(target_pager->get_fd(),(void*)target_pager->main_page.get(),PAGE_SIZE,0);
        if(n < 0){
            return Error("failed to write the file",Error::Layer::Pager);
        }
#endif
        if(target_pager->main_page->magic != MAGIC){
            return Error("invalid magic",Error::Layer::Pager);
        }
    }
    return target_pager;
}

int Pager_t::get_fd() const{
    return fd;
}

bool Pager_t::read_page(uint32_t page_num,char* buffer){
    if(!is_page_allocated(page_num)){
        return false;
    }
    auto it = hash.find(page_num);
    if(it != hash.end()){
        Page* page = it->second;
        move_to_head(page);
        memcpy(buffer, page->data, PAGE_SIZE);
        return true;
    }
    else{
        Page* page = new Page;
#ifdef _WIN32
        _lseek(fd, page_num * PAGE_SIZE, SEEK_SET);
        int size = _read(fd, page->data, PAGE_SIZE);
        if(size == -1){
            delete page;
            return false;
        }
#else
        ssize_t size = pread(fd,page->data,PAGE_SIZE,page_num * PAGE_SIZE);
        if(size == -1){
            perror("pread");
            delete page;
            return false;
        }
#endif
        page->page_num = page_num;
        hash[page_num] = page;

        evict_if_needed();

        page->pre = nullptr;
        page->next = head;
        if(head != nullptr){
            head->pre = page;
        }
        head = page;
        if(tail == nullptr){
            tail = page;
        }
        memcpy(buffer,page->data,PAGE_SIZE);
    }
    return true;
}

bool Pager_t::write_page(uint32_t page_num){
    if(!hash.count(page_num)){
        return true;
    }
    Page* page = hash[page_num];
    if(!page->dirty){
        return true;
    }
#ifdef _WIN32
    _lseek(fd, page_num * PAGE_SIZE, SEEK_SET);
    int n = _write(fd, page->data, PAGE_SIZE);
    if(n != PAGE_SIZE){
        return false;
    }
#else
    ssize_t n = pwrite(fd,page->data,PAGE_SIZE,page_num * PAGE_SIZE);
    if(n != PAGE_SIZE){
        return false;
    }
#endif
    page->dirty = false;
    return true;
}

bool Pager_t::update_page(uint32_t page_num,const char* buffer){
    if(!is_page_allocated(page_num)){
        return false;
    }
    if(hash.count(page_num)){
        Page* page = hash[page_num];
        memcpy(page->data,buffer,PAGE_SIZE);
        move_to_head(page);
        page->dirty = true;
    }
    else{
        Page* page = new Page();
        page->page_num = page_num;
        memcpy(page->data, buffer, PAGE_SIZE);
        page->dirty = true;
        hash[page_num] = page;

        page->pre = nullptr;
        page->next = head;
        if (head != nullptr){
            head->pre = page;
        }
        head = page;
        if (tail == nullptr){
            tail = page;
        }
        evict_if_needed();
    }
    return true;
}

bool Pager_t::delete_page(uint32_t page_num){
    if(!is_page_allocated(page_num)){
        return false;
    }

    if(hash.count(page_num)){
        Page* page = hash[page_num];
        remove_from_list(page);
        delete page;
        hash.erase(page_num);
    }
    uint32_t idx = page_num / 32;
    uint32_t bit = page_num % 32;
    main_page->bitmap[idx] &= ~(1 << bit);
    main_page->pages_num--;
    return true;
}

uint32_t Pager_t::alloc_new_page(){
    for(int i = 0; i < 16; i++){
        uint32_t temp = main_page->bitmap[i];
        for(int j = 0; j < 32; j++){
            if(((temp >> j) & 1) == 0){
                uint32_t page_num = i*32 + j;
                if (page_num == 0) continue;
                char zero[PAGE_SIZE] = {0};
#ifdef _WIN32
                _lseek(fd, page_num * PAGE_SIZE, SEEK_SET);
                if(_write(fd, zero, PAGE_SIZE) != PAGE_SIZE){
                    return 0;
                }
#else
                if(pwrite(fd,zero,PAGE_SIZE,page_num * PAGE_SIZE) != PAGE_SIZE){
                    return 0;
                }
#endif
                main_page->bitmap[i] |= (1 << j);
                main_page->pages_num++;
                return page_num;
            }
        }
    }
    return 0;
}
