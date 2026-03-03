#pragma once
#include "fits.hpp"
#include "result.hpp"
#include <string>
#include <unordered_map>

class FrameEvaluation
{
  public:
    FrameEvaluation() = default;
    std::optional<float> rate_image(FitsFile &image);

  private:
    
};

la_result run_rate(std::unordered_map<std::string, std::string> &args);
