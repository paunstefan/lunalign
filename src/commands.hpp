#pragma once

#include <functional>
#include <string>
#include <unordered_map>

#include "result.hpp"

struct ArgSpec {
    std::string name;
    bool required;
    std::string default_value; // used if not required
};

struct Command
{
    std::string name;
    std::vector<ArgSpec> args;
    std::function<la_result(std::unordered_map<std::string, std::string>&)> runner;
    std::string help;
};

la_result process_commands(std::string script);