#include <filesystem>
#include <iostream>

#include "decode.hpp"
#include "ser.hpp"

namespace fs = std::filesystem;

void run_decode(std::vector<std::string> args)
{
    fs::path input_file = args[0];
    fs::path output_dir = "fits_out";
    if(input_file.extension() != ".ser"){
        std::cerr << "Error: Only .ser files can be decoded.\n";
        return;
    }

    try
    {
        SerFile::decode_to_dir(input_file, output_dir);
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return;
    }
}