#pragma once
#include <vector>
#include <memory>
#include <cstdint>
#include "Help/Error/error.hpp"

struct Btree_Node;
struct Pager_t;

class StackPath{
    private:
    std::vector<std::unique_ptr<Btree_Node>> path;
    int curtop = -1;
    uint32_t stacksize = 0;

    public:
    uint32_t size() const;

    void push(std::unique_ptr<Btree_Node> page);

    void pop();

    Btree_Node& top();

    bool empty();

    void clear();

    bool flush_all(Pager_t& pager);
};

bool update_Path(Pager_t& pager,StackPath& path);
