#pragma once

#include "DataModels.h"
#include "BinaryFile.h"

class Table
{
private:
    std::string name;
    std::vector<Column> columns;
    std::vector<Record> records;

public:
    Table() = default;
    Table(const std::string& name);

    void addColumn(const Column& column);
    void addRecord(const Record& record);

    const std::string& getName() const;
    const std::vector<Column>& getColumns() const;
    const std::vector<Record>& getRecords() const;

    bool save(BinaryFile& file) const;
    bool load(BinaryFile& file);
};