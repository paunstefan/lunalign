#pragma once
#include "fits.hpp"
#include "result.hpp"
#include <opencv2/core.hpp>
#include <string>
#include <unordered_map>
#include <vector>

enum class StackMethod
{
    Mean,
    Median,
    SigmaClip
};

class FrameStacker
{
  public:
    FrameStacker(StackMethod method, float sigma, bool useWeights);

    /// Add a frame with an optional quality weight (default 1.0).
    /// Returns false if the frame dimensions don't match previous frames.
    bool addFrame(const cv::Mat &frame, float weight = 1.0f);

    /// Produce the final stacked image (CV_32F).
    cv::Mat stack() const;

  private:
    StackMethod method_;
    float sigma_;
    bool useWeights_;

    std::vector<cv::Mat> frames_; // stored as CV_32F (per channel)
    std::vector<float> weights_;

    cv::Mat stackMean() const;
    cv::Mat stackMedian() const;
    cv::Mat stackSigmaClip() const;
};

la_result run_stack(std::unordered_map<std::string, std::string> &args);