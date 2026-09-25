#include "Database.h"

void Database::addTable(const Table& table)
{
    tables.push_back(table);
}

Table* Database::getTable(const std::string& name)
{
    for (Table& table : tables)
    {
        if (table.getName() == name)
            return &table;
    }

    return nullptr;
}

const std::vector<Table>& Database::getTables() const
{
    return tables;
}

bool Database::save(const std::string& filename) const
{
    BinaryFile file;

    if (!file.open(
        filename,
        std::ios::out | std::ios::trunc))
    {
        return false;
    }

    if (!file.writeUInt32(
        static_cast<std::uint32_t>(tables.size())))
    {
        return false;
    }

    for (const Table& table : tables)
    {
        if (!table.save(file))
            return false;
    }

    file.close();

    return true;
}

bool Database::load(const std::string& filename)
{
    BinaryFile file;

    if (!file.open(filename, std::ios::in))
        return false;

    std::uint32_t tableCount;

    if (!file.readUInt32(tableCount))
        return false;

    tables.clear();

    for (std::uint32_t i = 0; i < tableCount; ++i)
    {
        Table table;

        if (!table.load(file))
            return false;

        tables.push_back(table);
    }

    file.close();

    return true;
}

void Database::removeTable(const std::string& name)
{
    for (std::size_t i = 0; i < tables.size(); ++i)
    {
        if (tables[i].getName() == name)
        {
            tables.erase(tables.begin() + i);
            return;
        }
    }
}