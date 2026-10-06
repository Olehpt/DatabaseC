#include "Table.h"

#include <cstdint>
#include <cmath>
#include <set>
#include <stdexcept>

namespace {
bool matches(const Value& value, DataType type)
{
    return static_cast<std::size_t>(type) == value.index() &&
        (!std::holds_alternative<double>(value) || std::isfinite(std::get<double>(value))) &&
        (!std::holds_alternative<Complex>(value) ||
            (std::isfinite(std::get<Complex>(value).real()) && std::isfinite(std::get<Complex>(value).imag()))) &&
        (!std::holds_alternative<std::string>(value) || std::get<std::string>(value).size() <= 16 * 1024 * 1024);
}
}
bool Table::valid() const
{
    if (name.empty() || name.size() > 1024 || name.find('\0') != std::string::npos || columns.size() > 10000 || records.size() > 1000000 ||
        (!columns.empty() && records.size() > 1000000 / columns.size())) return false;
    std::set<std::string> names;
    for (const auto& c : columns)
        if (c.name.empty() || c.name.size() > 1024 || c.name.find('\0') != std::string::npos ||
            static_cast<unsigned>(c.type) > static_cast<unsigned>(DataType::Complex) || !names.insert(c.name).second) return false;
    for (const auto& r : records) {
        if (r.values.size() != columns.size()) return false;
        for (std::size_t i = 0; i < columns.size(); ++i) if (!matches(r.values[i], columns[i].type)) return false;
    }
    return true;
}
void Table::rename(const std::string& newName)
{
    if (newName.empty() || newName.size() > 1024 || newName.find('\0') != std::string::npos) throw std::invalid_argument("Invalid table name");
    name = newName;
}
void Table::renameColumn(const std::string& oldName, const std::string& newName)
{
    if (newName.empty() || newName.size() > 1024 || newName.find('\0') != std::string::npos) throw std::invalid_argument("Invalid column name");
    for (const auto& c : columns) if (c.name == newName && c.name != oldName) throw std::invalid_argument("Duplicate column");
    for (auto& c : columns) if (c.name == oldName) { c.name = newName; return; }
    throw std::out_of_range("Column not found");
}

Table::Table(const std::string& name)
    : name(name)
{
}

void Table::addColumn(const Column& column)
{
    Value value;
    switch (column.type) {
    case DataType::Integer: value = std::int32_t{0}; break;
    case DataType::Real: value = 0.0; break;
    case DataType::Char: value = '\0'; break;
    case DataType::String: value = std::string{}; break;
    case DataType::Complex: value = Complex{0.0, 0.0}; break;
    default: throw std::invalid_argument("Invalid column type");
    }
    addColumn(column, value);
}
void Table::addColumn(const Column& column, const Value& defaultValue)
{
    if (column.name.empty() || column.name.size() > 1024 || column.name.find('\0') != std::string::npos ||
        !matches(defaultValue, column.type) || columns.size() >= 10000 || records.size() > 1000000 / (columns.size() + 1))
        throw std::invalid_argument("Invalid column");
    for (const auto& c : columns) if (c.name == column.name) throw std::invalid_argument("Duplicate column");
    columns.push_back(column);
    for (auto& record : records) record.values.push_back(defaultValue);
}

void Table::addRecord(const Record& record)
{
    if (record.values.size() != columns.size() || records.size() >= 1000000 ||
        (!columns.empty() && records.size() + 1 > 1000000 / columns.size())) throw std::invalid_argument("Invalid record size");
    for (std::size_t i = 0; i < columns.size(); ++i)
        if (!matches(record.values[i], columns[i].type)) throw std::invalid_argument("Invalid record type");
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
    if (!valid()) return false;
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

            case DataType::Complex:
                if (!file.writeDouble(std::get<Complex>(value).real()) ||
                    !file.writeDouble(std::get<Complex>(value).imag())) return false;
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
    Table candidate;
    if (!candidate.loadContent(file)) return false;
    *this = std::move(candidate);
    return true;
}

bool Table::loadContent(BinaryFile& file)
{
    if (!file.readString(name))
        return false;

    std::uint32_t columnCount;

    if (!file.readUInt32(columnCount))
        return false;
    if (columnCount > 10000 || columnCount > file.remaining() / 8) return false;

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

        if (type > static_cast<std::uint32_t>(DataType::Complex)) return false;
        column.type = static_cast<DataType>(type);

        columns.push_back(column);
    }

    std::uint32_t recordCount;

    if (!file.readUInt32(recordCount))
        return false;
    if (recordCount > 1000000 || (columnCount && recordCount > 1000000 / columnCount)) return false;
    std::uint64_t minimumRowSize = 0;
    for (const auto& c : columns)
        minimumRowSize += c.type == DataType::Char ? 1 :
            (c.type == DataType::Complex ? 16 : (c.type == DataType::Real ? 8 : 4));
    if (minimumRowSize && recordCount > file.remaining() / minimumRowSize) return false;

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

            case DataType::Complex:
            {
                double real, imag;
                if (!file.readDouble(real) || !file.readDouble(imag)) return false;
                record.values.push_back(Complex{real, imag});
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

    return valid();
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
    if (index >= records.size()) throw std::out_of_range("Row not found");
    if (record.values.size() != columns.size()) throw std::invalid_argument("Invalid record size");
    for (std::size_t i = 0; i < columns.size(); ++i)
        if (!matches(record.values[i], columns[i].type)) throw std::invalid_argument("Invalid record type");

    records[index] = record;
}
