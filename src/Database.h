#pragma once

#include <vector>
#include <string>

#include "Table.h"

class Database
{
private:
    std::vector<Table> tables;

public:
    void addTable(const Table& table);

    Table* getTable(const std::string& name);

    const std::vector<Table>& getTables() const;

    void removeTable(const std::string& name);

    bool save(const std::string& filename) const;
    bool load(const std::string& filename);
};