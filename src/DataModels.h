#pragma once
#include <string>
#include <variant>
#include <vector>
#include <cstdint>
#include <complex>


enum class DataType
{
    Integer,
    Real,
    Char,
    String,
    Complex
};

struct Column
{
    std::string name;
    DataType type;
};

using Complex = std::complex<double>;

using Value = std::variant<
    int32_t,
    double,
    char,
    std::string,
    Complex
>;

struct Record
{
    std::vector<Value> values;
    std::string to_string(const Value &v);
};
