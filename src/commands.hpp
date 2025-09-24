#pragma once

#include <functional>
#include <string>
#include <unordered_map>

#include "result.hpp"

struct Command
{
    std::string name;
    int number_args;
    std::function<la_result(std::unordered_map<std::string, std::string>&)> runner;
    std::string help;
};

la_result process_commands(std::string script);