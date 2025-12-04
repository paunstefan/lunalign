#include "debayer.hpp"
#include "fits.hpp"
#include "result.hpp"
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <librtprocess.h>
#include <print>

#include <fitsio.h>
#include <vector>

namespace fs = std::filesystem;

enum class sensor_pattern
{
    BAYER_FILTER_RGGB,
    BAYER_FILTER_BGGR,
    BAYER_FILTER_GBRG,
    BAYER_FILTER_GRBG,
    XTRANS_FILTER_1,
    XTRANS_FILTER_2,
    XTRANS_FILTER_3,
    XTRANS_FILTER_4,
    BAYER_FILTER_NONE = -1 // case where pattern is undefined or untested
};

static const std::unordered_map<std::string, sensor_pattern> bayer_mapping = {
    {"RGGB", sensor_pattern::BAYER_FILTER_RGGB},
    {"BGGR", sensor_pattern::BAYER_FILTER_BGGR},
    {"GBRG", sensor_pattern::BAYER_FILTER_GBRG},
    {"GRBG", sensor_pattern::BAYER_FILTER_GRBG},
};

std::vector<uint16_t> debayer_fits(FitsFile &file);
void pattern_to_cfarray(sensor_pattern pattern, unsigned int cfarray[2][2]);
std::vector<uint16_t> debayer_buffer_new_ushort(uint16_t *buf, int *width, int *height, sensor_pattern pattern,
                                                int bit_depth);

la_result run_debayer(std::unordered_map<std::string, std::string> &args)
{
    if (!args.contains("in"))
    {
        std::println("Error: Required argument 'in' for 'decode' not present!");
        return la_result::Error;
    }
    fs::path input_dir = args["in"];

    fs::path output_dir = "process/debayered";
    if (args.contains("out"))
    {
        output_dir = args["out"];
    }

    fs::create_directories(output_dir);

    auto result = la_result::Ok;
    int file_count = 0;

    for (auto const &dir_entry : std::filesystem::directory_iterator{input_dir})
    {
        if (dir_entry.path().extension() == ".fits")
        {
            auto fits_file = FitsFile(dir_entry.path(), FitsFile::Mode::ReadOnly);

            std::vector<long> naxes_out = {fits_file.naxes[0], fits_file.naxes[1], 3};
            long nelem_out = naxes_out[0] * naxes_out[1] * 3;

            std::vector<uint16_t> out_buffer = debayer_fits(fits_file);

            if (!out_buffer.empty())
            {
                fs::path output_filename = output_dir / std::format("debayered_{}.fits", file_count);
                file_count++;
                std::string create_path = "!" + output_filename.string();

                auto out_file = FitsFile(create_path, FitsFile::Mode::Create);

                out_file.writePix(out_buffer, 3, naxes_out, {1, 1, 1}, nelem_out);

                out_file.writeComment("CTYPE3 = 'RGB' / Color space");
                out_file.writeComment("CPLANE1 = 'RED' / Color plane 1");
                out_file.writeComment("CPLANE2 = 'GREEN' / Color plane 2");
                out_file.writeComment("CPLANE3 = 'BLUE' / Color plane 3");

            }
        }
    }

    return result;
}

std::vector<uint16_t> debayer_fits(FitsFile &file)
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
        return {};
    }

    auto out_buffer = debayer_buffer_new_ushort(image_data.data(), (int *)&(file.naxes[0]), (int *)&(file.naxes[1]),
                                                sensor_pat, file.bitpix);

    if (out_buffer.empty())
    {
        std::println("Debayer failed");
        return {};
    }

    return out_buffer;
}

void pattern_to_cfarray(sensor_pattern pattern, unsigned int cfarray[2][2])
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

static bool progress(double p)
{
    // p is [0, 1] progress of the debayer process
    return true;
}

/**
 * Round float value to a BYTE
 * @param f value to round
 * @return a truncated and rounded BYTE
 */
uint8_t roundf_to_BYTE(float f)
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
uint16_t roundf_to_WORD(float f)
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
std::vector<uint16_t> debayer_buffer_new_ushort(uint16_t *buf, int *width, int *height, sensor_pattern pattern,
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
    // TODO: vectorize!
    for (j = 0; j < nbpixels; j++)
        rawdata[0][j] = (float)buf[j];

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