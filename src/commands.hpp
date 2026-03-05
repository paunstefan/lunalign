#pragma once

#include <functional>
#include <string>
#include <unordered_map>

#include "result.hpp"

using PipelineContext = std::unordered_map<std::string, std::string>;

struct ArgSpec {
    std::string name;
    bool required;
    std::string default_value; // used if not required
};

struct Command
{
    std::string name;
    std::vector<ArgSpec> args;
    std::function<la_result(std::unordered_map<std::string, std::string>&, PipelineContext&)> runner;
    std::string help;
};

la_result process_commands(std::string script);