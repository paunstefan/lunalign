#include "ser.hpp"
#include "fits.hpp"

#include "fitsio.h"
#include "result.hpp"
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <print>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

int32_t read_le_i32(const std::vector<uint8_t> &buffer, size_t offset)
{
    return static_cast<int32_t>(
        static_cast<uint32_t>(buffer[offset]) | (static_cast<uint32_t>(buffer[offset + 1]) << 8) |
        (static_cast<uint32_t>(buffer[offset + 2]) << 16) | (static_cast<uint32_t>(buffer[offset + 3]) << 24));
}

la_result SerFile::decode_to_dir(const fs::path &input_path, const fs::path &output_dir)
{
    std::ifstream file(input_path, std::ios::binary);
    if (!file)
    {
        std::println("Failed to open input file: {}", input_path.string());
        return la_result::Error;
    }

    std::vector<uint8_t> header_buffer(178);
    file.read(reinterpret_cast<char *>(header_buffer.data()), header_buffer.size());

    SerHeader header;
    header.name = input_path.string();
    header.color = read_le_i32(header_buffer, 18);
    header.endianess = read_le_i32(header_buffer, 22);
    header.width = read_le_i32(header_buffer, 26);
    header.height = read_le_i32(header_buffer, 30);
    header.pixel_depth = read_le_i32(header_buffer, 34);
    header.frame_count = read_le_i32(header_buffer, 38);

    std::println("SER Header Parsed:");
    std::println("  - Dimensions: {}x{}", header.width, header.height);
    std::println("  - Pixel Depth: {} bits", header.pixel_depth);
    std::println("  - Frame Count: {}", header.frame_count);

    size_t pixels_per_frame = header.width * header.height;
    size_t bytes_per_pixel = header.pixel_depth / 8;
    size_t frame_size_bytes = pixels_per_frame * bytes_per_pixel;

    int fits_image_type;
    switch (header.pixel_depth)
    {
    case 8:
        fits_image_type = BYTE_IMG;
        break;
    case 16:
        fits_image_type = USHORT_IMG;
        break;
    case 32:
        fits_image_type = ULONG_IMG;
        break;
    default: {
        std::println("Unsupported pixel depth: {}", header.pixel_depth);
        return la_result::Error;
    }
    }

    la_result res = la_result::Error;

    std::vector<uint8_t> frame_buffer(frame_size_bytes);
    for (int32_t i = 0; i < header.frame_count; ++i)
    {
        std::println("Processing frame {}/{}", i + 1, header.frame_count);

        file.read(reinterpret_cast<char *>(frame_buffer.data()), frame_size_bytes);
        if (file.gcount() != frame_size_bytes)
        {
            std::println("Failed to read full frame data for frame ", i);
            return la_result::Error;
        }

        fs::path output_filename = output_dir / std::format("decoded_{:04d}.fits", i);

        std::vector<long> naxes = {header.width, header.height};

        std::string create_path = "!" + output_filename.string();
        auto fits_file = FitsFile(create_path, FitsFile::Mode::Create);

        switch (fits_image_type)
        {
        case 8: {
            res = fits_file.writeImage(frame_buffer, 2, naxes, 1, pixels_per_frame);
            break;
        }
        case 16: {
            std::vector<uint16_t> image_data(pixels_per_frame);
            for (size_t p = 0; p < pixels_per_frame; ++p)
            {
                image_data[p] =
                    static_cast<uint16_t>(frame_buffer[p * 2]) | (static_cast<uint16_t>(frame_buffer[p * 2 + 1]) << 8);
            }
            res = fits_file.writeImage(image_data, 2, naxes, 1, pixels_per_frame);
            break;
        }
        case 32: {
            std::vector<uint32_t> image_data(pixels_per_frame);
            for (size_t p = 0; p < pixels_per_frame; ++p)
            {
                image_data[p] = static_cast<uint32_t>(frame_buffer[p * 4]) |
                                (static_cast<uint32_t>(frame_buffer[p * 4 + 1]) << 8) |
                                (static_cast<uint32_t>(frame_buffer[p * 4 + 2]) << 16) |
                                (static_cast<uint32_t>(frame_buffer[p * 4 + 3]) << 24);
            }
            res = fits_file.writeImage(image_data, 2, naxes, 1, pixels_per_frame);
            break;
        }
        }

        std::string bayer_pattern;
        switch (header.color)
        {
        case 8:
            bayer_pattern = "RGGB";
            break;
        case 9:
            bayer_pattern = "GRBG";
            break;
        case 10:
            bayer_pattern = "GBRG";
            break;
        case 11:
            bayer_pattern = "BGGR";
            break;
        default:
            break;
        }

        if (!bayer_pattern.empty())
        {
            res = fits_file.writeKey("BAYERPAT", bayer_pattern);
        }
    }

    std::println("\nConversion complete! {} FITS files written to '{}'.", header.frame_count, output_dir.string());
    return res;
}
