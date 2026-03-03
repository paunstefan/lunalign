/*
 * This file contains code originally from Siril, an astronomy image processor.
 * * Original Copyright (C) 2005-2011 Francois Meyer (dulle at free.fr)
 * Original Copyright (C) 2012-2021 team free-astro (see Siril AUTHORS file)
 * * Modified by Stefan Paun in September 2025
 * Copyright (C) 2025-2026 Stefan Paun
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "debayer.hpp"
#include "fits.hpp"
#include "result.hpp"
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <librtprocess.h>
#include <print>
#include <vector>

#include <fitsio.h>
#include <omp.h>

namespace fs = std::filesystem;

la_result run_debayer(std::unordered_map<std::string, std::string> &args)
{
    fs::path input_dir = args["in"];

    fs::path output_dir = args["out"];

    fs::create_directories(output_dir);

    auto result = la_result::Ok;
    Debayer debayer;

    std::vector<fs::path> fits_files;
    for (auto const &dir_entry : std::filesystem::directory_iterator{input_dir})
    {
        if (dir_entry.path().extension() == ".fits")
            fits_files.push_back(dir_entry.path());
    }

#pragma omp parallel for schedule(dynamic)
    for (int i = 0; i < static_cast<int>(fits_files.size()); ++i)
    {
        const auto &path = fits_files[i];

        auto fits_file = FitsFile(path, FitsFile::Mode::ReadOnly);
        std::vector<long> naxes_out = {fits_file.naxes[0], fits_file.naxes[1], 3};
        long nelem_out = naxes_out[0] * naxes_out[1] * 3;

        std::vector<uint16_t> out_buffer = debayer.debayer_fits(fits_file);

        if (!out_buffer.empty())
        {
            fs::path output_filename = output_dir / ("debayered_" + path.filename().string());
            std::println("Debayered file: {}", path.filename().string());
            std::string create_path = "!" + output_filename.string();
            auto out_file = FitsFile(create_path, FitsFile::Mode::Create);
            out_file.writePix(out_buffer, 3, naxes_out, {1, 1, 1}, nelem_out);
            out_file.writeComment("CTYPE3 = 'RGB' / Color space");
            out_file.writeComment("CPLANE1 = 'RED' / Color plane 1");
            out_file.writeComment("CPLANE2 = 'GREEN' / Color plane 2");
            out_file.writeComment("CPLANE3 = 'BLUE' / Color plane 3");
        }
    }

    return result;
}

std::vector<uint16_t> Debayer::debayer_fits(FitsFile &file)
{
    // printf("FITS File Information:\n");
    // printf("------------------------\n");
    // printf("BITPIX (data type): %d\n", bitpix);
    // printf("NAXIS (dimensions): %d\n", naxis);

    // for (int i = 0; i < naxis; i++)
    // {
    //     printf("NAXIS%d (dimension %d): %ld\n", i + 1, i + 1, naxes[i]);
    // }

    auto key_read = file.readKey("BAYERPAT");

    if (!key_read)
    {
        std::println("Error: Could not read bayer pattern from FITS file {}!", file.name);
        return {};
    }

    std::string bayer_pattern = *key_read;

    auto it = bayer_mapping.find(bayer_pattern);
    if (it == bayer_mapping.end())
    {
        std::println("{}", bayer_pattern);
        std::println("Error: Bayer pattern invalid for {}!", file.name);
        return {};
    }

    auto sensor_pat = it->second;

    long nelements = file.naxes[0] * file.naxes[1];
    if (file.naxis > 2)
    {
        nelements *= file.naxes[2];
    }

    std::vector<uint16_t> image_data = file.readPix<uint16_t>({1, 1, 1}, nelements);

    if (image_data.empty())
    {
        std::println(std::cerr, "Warning: debayer failed for {}, skipping.", file.name);
        return {};
    }

    int width = static_cast<int>(file.naxes[0]);
    int height = static_cast<int>(file.naxes[1]);
    auto out_buffer = debayer_buffer_new_ushort(image_data.data(), &width, &height, sensor_pat, file.bitpix);

    if (out_buffer.empty())
    {
        std::println("Debayer failed");
        return {};
    }

    return out_buffer;
}

void Debayer::pattern_to_cfarray(sensor_pattern pattern, unsigned int cfarray[2][2])
{
    switch (pattern)
    {
    case sensor_pattern::BAYER_FILTER_RGGB:
        cfarray[0][0] = 0;
        cfarray[0][1] = 1;
        cfarray[1][0] = 1;
        cfarray[1][1] = 2;
        break;
    case sensor_pattern::BAYER_FILTER_BGGR:
        cfarray[0][0] = 2;
        cfarray[0][1] = 1;
        cfarray[1][0] = 1;
        cfarray[1][1] = 0;
        break;
    case sensor_pattern::BAYER_FILTER_GBRG:
        cfarray[0][0] = 1;
        cfarray[0][1] = 2;
        cfarray[1][0] = 0;
        cfarray[1][1] = 1;
        break;
    case sensor_pattern::BAYER_FILTER_GRBG:
        cfarray[0][0] = 1;
        cfarray[0][1] = 0;
        cfarray[1][0] = 2;
        cfarray[1][1] = 1;
        break;
    default:
        break;
    }
}

bool Debayer::progress(double p)
{
    // p is [0, 1] progress of the debayer process
    return true;
}

/**
 * Round float value to a BYTE
 * @param f value to round
 * @return a truncated and rounded BYTE
 */
uint8_t Debayer::roundf_to_BYTE(float f)
{
    if (f < 0.5f)
        return 0;
    if (f >= UCHAR_MAX - 0.5f)
        return UCHAR_MAX;
    return (uint8_t)(f + 0.5f);
}

/**
 * Round float value to a WORD
 * @param f value to round
 * @return a truncated and rounded WORD
 */
uint16_t Debayer::roundf_to_WORD(float f)
{
    uint16_t retval;
    if (f < 0.5f)
    {
        retval = 0;
    }
    else if (f >= USHRT_MAX - 0.5f)
    {
        retval = USHRT_MAX;
    }
    else
    {
        retval = (uint16_t)(f + 0.5f);
    }
    return retval;
}

/**
    Function copied from the Siril project.
 */
std::vector<uint16_t> Debayer::debayer_buffer_new_ushort(uint16_t *buf, int *width, int *height, sensor_pattern pattern,
                                                         int bit_depth)
{
    unsigned int cfarray[2][2];
    int i, rx = *width, ry = *height;
    long j, nbpixels = rx * ry;
    long n = nbpixels * 3;
    // 1. convert input data to float (memory size: 2 times original)
    float **rawdata = (float **)malloc(ry * sizeof(float *));
    if (!rawdata)
    {
        return {};
    }
    rawdata[0] = (float *)malloc(nbpixels * sizeof(float));
    if (!rawdata[0])
    {
        free(rawdata);
        return {};
    }

    std::transform(buf, buf + nbpixels, rawdata[0], [](uint16_t v) { return static_cast<float>(v); });

    for (i = 1; i < ry; i++)
        rawdata[i] = rawdata[i - 1] + rx;

    // 2. allocate the demosaiced image buffer (memory size: 6 times original)
    float *newdata = (float *)malloc(n * sizeof(float));
    if (!newdata)
    {
        free(rawdata);
        return {};
    }

    float **red = (float **)malloc(ry * sizeof(float *));
    red[0] = newdata;
    for (i = 1; i < ry; i++)
        red[i] = red[i - 1] + rx;

    float **green = (float **)malloc(ry * sizeof(float *));
    green[0] = red[0] + nbpixels;
    for (i = 1; i < ry; i++)
        green[i] = green[i - 1] + rx;

    float **blue = (float **)malloc(ry * sizeof(float *));
    blue[0] = green[0] + nbpixels;
    for (i = 1; i < ry; i++)
        blue[i] = blue[i - 1] + rx;

    // 3. process
    rpError retval;
    pattern_to_cfarray(pattern, cfarray);
    retval =
        amaze_demosaic(rx, ry, 0, 0, rx, ry, rawdata, red, green, blue, cfarray, progress, 1.0, 4, 65535.0, 65535.0);

    free(rawdata[0]); // memory size: 2 times original freed
    free(rawdata);

    // 4. get the result in WORD (memory size: 3 times original)
    // UPDATED TO VECTOR
    std::vector<uint16_t> newfitdata(n);

    for (j = 0; j < n; j++)
    {
        /* here bit_depth can really be bit_depth (with SER files
         * OR bitpix (with FITS file) so we need to pay attention!!!!
         * But BYTE_IMG has the value of 8. So it should be fine. */
        if (bit_depth == BYTE_IMG)
        {
            newfitdata[j] = roundf_to_BYTE(newdata[j]);
        }
        else
        {
            newfitdata[j] = roundf_to_WORD(newdata[j]);
        }
        /* these rounding are required because librtprocess
         * often returns data out of expected range */
    }

    free(newdata);
    free(blue);
    free(green);
    free(red);
    if (retval == RP_NO_ERROR)
    {
        return newfitdata;
    }
    return {};
}