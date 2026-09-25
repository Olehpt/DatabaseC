#include "Table.h"

#include <cstdint>

Table::Table(const std::string& name)
    : name(name)
{
}

void Table::addColumn(const Column& column)
{
    columns.push_back(column);
}

void Table::addRecord(const Record& record)
{
    records.push_back(record);
}

const std::string& Table::getName() const
{
    return name;
}

const std::vector<Column>& Table::getColumns() const
{
    return columns;
}

const std::vector<Record>& Table::getRecords() const
{
    return records;
}

bool Table::save(BinaryFile& file) const
{
    if (!file.writeString(name))
        return false;

    if (!file.writeUInt32(
        static_cast<std::uint32_t>(columns.size())))
        return false;

    for (const Column& column : columns)
    {
        if (!file.writeString(column.name))
            return false;

        if (!file.writeUInt32(
            static_cast<std::uint32_t>(column.type)))
            return false;
    }

    if (!file.writeUInt32(
        static_cast<std::uint32_t>(records.size())))
        return false;

    for (const Record& record : records)
    {
        if (record.values.size() != columns.size())
            return false;

        for (std::size_t i = 0; i < columns.size(); ++i)
        {
            const Value& value = record.values[i];

            switch (columns[i].type)
            {
            case DataType::Integer:
                if (!file.writeInt32(
                    std::get<std::int32_t>(value)))
                    return false;
                break;

            case DataType::Real:
                if (!file.writeDouble(
                    std::get<double>(value)))
                    return false;
                break;

            case DataType::Char:
            {
                char c = std::get<char>(value);

                if (!file.writeChar(c))
                    return false;

                break;
            }

            case DataType::String:
                if (!file.writeString(
                    std::get<std::string>(value)))
                    return false;
                break;
            }
        }
    }

    return true;
}

bool Table::load(BinaryFile& file)
{
    if (!file.readString(name))
        return false;

    std::uint32_t columnCount;

    if (!file.readUInt32(columnCount))
        return false;

    columns.clear();
    records.clear();

    for (std::uint32_t i = 0; i < columnCount; ++i)
    {
        Column column;

        if (!file.readString(column.name))
            return false;

        std::uint32_t type;

        if (!file.readUInt32(type))
            return false;

        column.type = static_cast<DataType>(type);

        columns.push_back(column);
    }

    std::uint32_t recordCount;

    if (!file.readUInt32(recordCount))
        return false;

    for (std::uint32_t i = 0; i < recordCount; ++i)
    {
        Record record;

        for (const Column& column : columns)
        {
            switch (column.type)
            {
            case DataType::Integer:
            {
                std::int32_t value;

                if (!file.readInt32(value))
                    return false;

                record.values.push_back(value);
                break;
            }

            case DataType::Real:
            {
                double value;

                if (!file.readDouble(value))
                    return false;

                record.values.push_back(value);
                break;
            }

            case DataType::Char:
            {
                char value;

                if (!file.readChar(value))
                    return false;

                record.values.push_back(value);
                break;
            }

            case DataType::String:
            {
                std::string value;

                if (!file.readString(value))
                    return false;

                record.values.push_back(value);
                break;
            }
            }
        }

        records.push_back(record);
    }

    return true;
}

void Table::removeColumn(const std::string& name)
{
    for (std::size_t i = 0; i < columns.size(); ++i)
    {
        if (columns[i].name == name)
        {
            columns.erase(columns.begin() + i);

            for (Record& record : records)
            {
                if (i < record.values.size())
                    record.values.erase(record.values.begin() + i);
            }

            return;
        }
    }
}

void Table::removeRecord(std::size_t index)
{
    if (index >= records.size())
        return;

    records.erase(records.begin() + index);
}

void Table::updateRecord(
    std::size_t index,
    const Record& record)
{
    if (index >= records.size())
        return;

    if (record.values.size() != columns.size())
        return;

    records[index] = record;
}