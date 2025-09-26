#include <filesystem>
#include <iostream>
#include <print>
#include "debayer.hpp"
#include "result.hpp"
#include <librtprocess.h>

#include <fitsio.h>

namespace fs = std::filesystem;

la_result debayer_fits(fitsfile *file);

la_result run_debayer(std::unordered_map<std::string, std::string>& args)
{
    if (!args.contains("in"))
    {
        std::println("Error: Required argument 'in' for 'decode' not present!");
        return la_result::Error;
    }
    fs::path input_file = args["in"];

    fs::path output_dir = "debayered";
    if (args.contains("out"))
    {
        output_dir = args["out"];
    }
    if (input_file.extension() != ".ser")
    {
        std::println(std::cerr, "Error: Only .ser files can be decoded.");
        return la_result::Error;
    }

    la_result result = debayer_fits();

    return result;
}

la_result debayer_fits(fitsfile *file)
{

}