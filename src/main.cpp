#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "commands.hpp"

const char *helpstring = "Usage: lunalign <commands>/<script_file>";

struct Config {
    std::string input_file;

};

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        std::cerr << helpstring << "\n";
        return 1;
    }

    std::string arg = argv[1];

    if(arg == "-h" || arg == "--help"){
        std::cout << helpstring << std::endl;
        return 0;
    }

    process_commands(arg);

    return 0;
}