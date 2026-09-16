#pragma once
#include <string>
#include <variant>
#include <vector>

enum class DataType
{
    Integer,
    Real,
    Char,
    String
};

struct Column
{
    std::string name;
    DataType type;
};

using Value = std::variant<
    int32_t,
    double,
    char,
    std::string
>;

struct Record
{
    std::vector<Value> values;
    std::string to_string(const Value &v);
};