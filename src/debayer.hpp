#pragma once
#include "fits.hpp"
#include "commands.hpp"
#include "result.hpp"
#include <string>
#include <unordered_map>

class Debayer
{
  public:
    Debayer() = default;
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

    std::vector<uint16_t> debayer_fits(FitsFile &file);

  private:
    const std::unordered_map<std::string, sensor_pattern> bayer_mapping = {
        {"RGGB", sensor_pattern::BAYER_FILTER_RGGB},
        {"BGGR", sensor_pattern::BAYER_FILTER_BGGR},
        {"GBRG", sensor_pattern::BAYER_FILTER_GBRG},
        {"GRBG", sensor_pattern::BAYER_FILTER_GRBG},
    };

    void pattern_to_cfarray(sensor_pattern pattern, unsigned int cfarray[2][2]);

    std::vector<uint16_t> debayer_buffer_new_ushort(uint16_t *buf, int *width, int *height, sensor_pattern pattern,
                                                    int bit_depth);

    static uint8_t roundf_to_BYTE(float f);
    static uint16_t roundf_to_WORD(float f);

    static bool progress(double p);
};

la_result run_debayer(std::unordered_map<std::string, std::string> &args, PipelineContext &ctx);
