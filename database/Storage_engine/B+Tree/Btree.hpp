#pragma once

#ifdef BTREE_USE_MOCK_PAGER
#include "mockpager.hpp"
#else
#include "Storage_engine/pager.hpp"
#endif
#include "Help/Error/error.hpp"

#include <string>
#include <vector>
#include <cstdint>
#include <optional>
#include <cstring>
#include <memory>
#define MAX_KEYS 16
#define LOGNEST_MESSAGE 4096U
#define HEAD_SIZE 1202U
struct Info;
struct StackPath;
struct Pager_t;

#pragma pack(push, 1)
struct Key{
    enum class Type : uint32_t{
        KEY_INT,
        KEY_FLOAT,
        KEY_VARCHAR
    };

    Type type;

    int val_int;
    float val_float;
    uint32_t length;
    char str[48];

    Key(const char* s){
        length = strlen(s);
        if (length > sizeof(str) - 1){
            length = sizeof(str) - 1;
        }
        strncpy(str, s, length);
        str[length] = '\0';

        type = Type::KEY_VARCHAR;
    }

    Key(int val){
        val_int = val;
        type = Type::KEY_INT;
    }

    Key(float val){
        val_float = val;
        type = Type::KEY_FLOAT;
    }

    Key() = default;

    bool operator<(const Key& outer) const{
        if(type == Type::KEY_INT){
            return val_int < outer.val_int;
        }
        else if(type == Type::KEY_FLOAT){
            return val_float < outer.val_float;
        }
        else{
            return strcmp(str,outer.str) < 0;
        }
    }

    bool operator>(const Key& outer) const{
        if(type == Type::KEY_INT){
            return val_int > outer.val_int;
        }
        else if(type == Type::KEY_FLOAT){
            return val_float > outer.val_float;
        }
        else{
            return strcmp(str,outer.str) > 0;
        }
    }

    bool operator<=(const Key& outer) const{
        if(type == Type::KEY_INT){
            return val_int <= outer.val_int;
        }
        else if(type == Type::KEY_FLOAT){
            return val_float <= outer.val_float;
        }
        else{
            return strcmp(str,outer.str) <= 0;
        }
    }

    bool operator>=(const Key& outer) const{
        if(type == Type::KEY_INT){
            return val_int >= outer.val_int;
        }
        else if(type == Type::KEY_FLOAT){
            return val_float >= outer.val_float;
        }
        else{
            return strcmp(str,outer.str) >= 0;
        }
    }

    bool operator==(const Key& outer) const{
        if(type == Type::KEY_INT){
            return val_int == outer.val_int;
        }
        else if(type == Type::KEY_FLOAT){
            return val_float == outer.val_float;
        }
        else{
            return strcmp(str,outer.str) == 0;
        }
    }

    bool operator!=(const Key& outer) const{
        if(type == Type::KEY_INT){
            return val_int != outer.val_int;
        }
        else if(type == Type::KEY_FLOAT){
            return val_float != outer.val_float;
        }
        else{
            return strcmp(str,outer.str) != 0;
        }
    }

};//size:64

#pragma pack(1)
struct Btree_Node{

    enum class Type : uint32_t{
        MID_NODE = 0,
        LEAF_NODE = 1
    };

    bool dirty;
    bool root;
    Type type;
    uint32_t key_number = 0;
    uint32_t father = 0;
    uint32_t page_num;
    uint32_t pre_leaf = 0;
    uint32_t next_leaf = 0;

    uint32_t curval = 0;
    uint32_t son_pages[MAX_KEYS + 1];
    uint32_t offsets[MAX_KEYS];
    uint8_t reserves[16];
    Key keys[MAX_KEYS];
    char values[PAGE_SIZE - HEAD_SIZE];

    bool is_dirty() const{
        return dirty;
    }

    bool is_leaf() const{
        return type == Type::LEAF_NODE;
    }

    bool is_root() const{
        return root;
    }

    uint32_t Find_Key_Index(const Key& key) const;

    uint32_t Find_Next_Page(const Key& key) const;/*MID调用*/

    bool Find_Value(const Key& key,char* buffer, uint32_t& out_size) const;/*LEAF调用*/

    uint32_t Find_Value(char* buffer, uint32_t buf_offset) const;/*LEAF调用, returns bytes written*/

    bool Insert_Key(const Key& key,uint32_t son_page);/*MID调用*/

    bool Insert_Value(const Key& key,const char* buffer, uint32_t val_size);/*LEAF调用*/

    bool Update_Value(const Key& key,const char* buffer, uint32_t val_size);/*LEAF调用*/

    bool Delete_Key(const Key& key,uint32_t son_page);/*MID调用*/

    bool Update_Key(const Key& prekey,const Key& newkey);

    bool Delete_Value(const Key& key);/*LEAF调用*/

    bool clear();/*ROOT调用*/

    /*根据结点类型进行分裂/合并的分类讨论*/

    bool Page_Split(Btree_Node& new_page,uint32_t new_page_num,Key& risekey);

    bool Borrow_Left(Btree_Node& left);

    bool Borrow_Right(Btree_Node& right);

    bool Merge_Left(Btree_Node& left,Key& delkey);

    bool Merge_Right(Btree_Node& right,Key& delkey);

    #ifdef DEBUG

    void print(FILE* fp) const;

    void print() const { print(stdout); }

    bool validate() const;

    #endif

};//size:4096

#pragma pack(pop)

static_assert(sizeof(Btree_Node) == PAGE_SIZE, "Btree_Node size must equal PAGE_SIZE");

/*B+tree存储结构：
[RowHeader][int:...][size][string:...][RowHeader][...][...]
|offset[0]                            |offset[1]

System_page存储结构：
[RowHeader][int:page_num][int:column_num][int:size1][string:column][int:size2][string:column_type]*/

Result<std::string> Btree_find(uint32_t page_num,Key& key,Pager_t& pager);

Result<std::string> Btree_scan(uint32_t page_num,Pager_t& pager);

Result<bool> Btree_insert(uint32_t page_num,Key& key,std::string& value,Pager_t& pager,StackPath& path,std::vector<std::unique_ptr<Btree_Node>>& new_pages,uint32_t& out_root_page);

Result<bool> Btree_update(uint32_t page_num,Key& key,std::string& value,Pager_t& pager,Btree_Node& update_page);

Result<bool> Btree_delete(uint32_t page_num,Key& key,Pager_t& pager,StackPath& path,std::vector<uint32_t>& del_pages,std::vector<std::unique_ptr<Btree_Node>>& update_pages,uint32_t& out_root_page);

Result<bool> Btree_delete_range(uint32_t page_num,Pager_t& pager,std::vector<uint32_t>& del_pages,uint32_t& out_root_page);

bool Split_chain(Btree_Node& curpage,Pager_t& pager,StackPath& path,std::vector<std::unique_ptr<Btree_Node>>& new_pages);

bool Merge_chain(Btree_Node& curpage,Pager_t& pager,StackPath& path,std::vector<uint32_t>& del_pages,std::vector<std::unique_ptr<Btree_Node>>& update_pages,uint32_t& out_root_page);

bool create_new_root(uint32_t page1,uint32_t page2,Key& risekey,Btree_Node& new_root,uint32_t new_root_page);