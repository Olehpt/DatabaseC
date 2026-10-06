#include "DataModels.h"
#include <string>
#include <variant>
#include <type_traits>
#include <cmath>

std::string Record::to_string(const Value &v) {
    return std::visit(
        [](const auto& arg) -> std::string
    {
        using T = std::decay_t<decltype(arg)>;

        if constexpr (std::is_same_v<T, std::string>)
        {
            return arg;
        }
        else if constexpr (std::is_same_v<T, char>)
        {
            return std::string(1, arg);
        }
        else if constexpr (std::is_same_v<T, Complex>)
        {
            return std::to_string(arg.real()) + (std::signbit(arg.imag()) ? "" : "+") +
                std::to_string(arg.imag()) + "i";
        }
        else
        {
            return std::to_string(arg);
        }
    },
        v
    );
}
