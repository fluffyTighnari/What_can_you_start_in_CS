#ifdef BTREE_USE_MOCK_PAGER
#include "mockpager.hpp"
#else
#include "Storage_engine/pager.hpp"
#endif
#include "Btree.hpp"
#include "StackPath.hpp"
#include <iostream>

uint32_t StackPath::size() const{
    return static_cast<uint32_t>(curtop + 1);
}

void StackPath::push(std::unique_ptr<Btree_Node> page){
    path.push_back(std::move(page));
    curtop++;
    stacksize++;
}

void StackPath::pop(){
    curtop--;
}

Btree_Node& StackPath::top(){
    return *path[curtop];
}

bool StackPath::empty(){
    return curtop == -1;
}

void StackPath::clear(){
    path.clear();
    curtop = -1;
    stacksize = 0;
}

bool StackPath::flush_all(Pager_t& pager){
    for(auto& node_ptr : path){
        if(node_ptr->is_dirty()){
            node_ptr->dirty = false;
            if(!pager.update_page(node_ptr->page_num, reinterpret_cast<char*>(node_ptr.get()))){
                return false;
            }
        }
    }
    clear();
    return true;
}

bool update_Path(Pager_t& pager,StackPath& path){
    if(!path.flush_all(pager)){
        return false;
    }
    return true;
}
