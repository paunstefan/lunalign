#pragma once

#include "result.hpp"
#include <cstdint>
#include <filesystem>
#include <string>

struct SerHeader
{
    std::string name;
    int32_t color;
    int32_t endianess;
    int32_t width;
    int32_t height;
    int32_t pixel_depth;
    int32_t frame_count;
};

class SerFile
{
  public:
    static la_result decode_to_dir(const std::filesystem::path &input_path, const std::filesystem::path &output_dir);
};