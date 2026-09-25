#pragma once
#include <vector>
#include <memory>
#include "Help/Error/error.hpp"

struct SymbolTable;
struct Pager_t;

class Storage{
    private:
    std::unique_ptr<Pager_t> pager;
    std::unique_ptr<SymbolTable> symbol_table;

    public:
    Storage(std::unique_ptr<Pager_t> pager_t,std::unique_ptr<SymbolTable> symbol_table_t);

    const Pager_t& get_Pager() const;

    Pager_t& get_Pager();

    const SymbolTable& get_symboltable() const;

    SymbolTable& get_symboltable();

    const SymbolTable* acquire_const_SymbolTable() const;

    SymbolTable* acquire_SymbolTable();

    std::unique_ptr<Pager_t> release_pager();
};

Result<std::unique_ptr<Storage>> InitStorage(std::unique_ptr<Pager_t> pager_t,std::unique_ptr<SymbolTable> symbol_table_t);
