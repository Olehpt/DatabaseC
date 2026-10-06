#pragma once

#include "DataModels.h"
#include "BinaryFile.h"

class Table
{
private:
    std::string name;
    std::vector<Column> columns;
    std::vector<Record> records;
    bool loadContent(BinaryFile& file);

public:
    Table() = default;
    Table(const std::string& name);

    void addColumn(const Column& column);
    void addColumn(const Column& column, const Value& defaultValue);
    void rename(const std::string& newName);
    void renameColumn(const std::string& oldName, const std::string& newName);
    bool valid() const;
    void addRecord(const Record& record);

    void removeColumn(const std::string& name);
    void removeRecord(std::size_t index);
    void updateRecord(std::size_t index, const Record& record);

    const std::string& getName() const;
    const std::vector<Column>& getColumns() const;
    const std::vector<Record>& getRecords() const;

    bool save(BinaryFile& file) const;
    bool load(BinaryFile& file);
};
