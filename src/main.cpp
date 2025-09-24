#include <filesystem>
#include <fstream>
#include <print>
#include <string>
#include <utility>

#include "commands.hpp"
#include "result.hpp"

const char *helpstring = "Usage: lunalign <commands>/<script_file>";

int main(int argc, const char *argv[])
{
    if (argc != 2)
    {
        std::println("{}", helpstring);
        return 1;
    }

    std::string arg = argv[1];

    if (arg == "-h" || arg == "--help")
    {
        std::println("{}", helpstring);
        return 0;
    }

    la_result result = la_result::Error;
    if (std::filesystem::is_regular_file(arg))
    {
        std::ifstream file(arg);
        const auto file_size = std::filesystem::file_size(arg);
        if (!file.is_open())
        {
            std::println("Error: Could not open file: {}", arg);
            return -1;
        }
        std::string content(file_size, '\0');
        file.read(content.data(), file_size);
        result = process_commands(content);
    }
    else
    {
        result = process_commands(arg);
    }

    return std::to_underlying(result);
}