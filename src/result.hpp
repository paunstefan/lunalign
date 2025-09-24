#pragma once

#include <format>
#include <string_view>

enum class la_result
{
    Ok = 0,
    Error = -1
};

inline constexpr std::string_view to_string(la_result c)
{
    switch (c)
    {
    case la_result::Ok:
        return "Ok";
    case la_result::Error:
        return "Error";
    default:
        return "Unknown";
    }
}

template <> struct std::formatter<la_result> : std::formatter<std::string_view>
{
    auto format(la_result c, std::format_context &ctx) const
    {
        return std::formatter<std::string_view>::format(to_string(c), ctx);
    }
};