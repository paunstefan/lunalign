#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cstdint>
#include <filesystem>

#include "ser.hpp"

namespace fs = std::filesystem;

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <input.ser> <output_directory>" << std::endl;
        return 1;
    }

    try {
        fs::path input_file = argv[1];
        fs::path output_dir = argv[2];
        SerFile::decode_to_dir(input_file, output_dir);
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}