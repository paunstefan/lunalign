#include <filesystem>
#include <iostream>
#include <print>

#include "decode.hpp"
#include "result.hpp"
#include "ser.hpp"

namespace fs = std::filesystem;

la_result run_decode(std::unordered_map<std::string, std::string>& args)
{
    if (!args.contains("in"))
    {
        std::println("Error: Required argument 'in' for 'decode' not present!");
        return la_result::Error;
    }
    fs::path input_file = args["in"];

    fs::path output_dir = "fits_out";
    if (args.contains("out"))
    {
        output_dir = args["out"];
    }
    if (input_file.extension() != ".ser")
    {
        std::println(std::cerr, "Error: Only .ser files can be decoded.");
        return la_result::Error;
    }

    la_result result = SerFile::decode_to_dir(input_file, output_dir);

    return result;
}