#pragma once
#include "result.hpp"
#include <opencv2/core.hpp>
#include <string>
#include <unordered_map>

struct RegistrationResult
{
    double dx = 0;               // translation X (pixels)
    double dy = 0;               // translation Y (pixels)
    double rotationAngleDeg = 0; // rotation (degrees, CCW positive)
    double scalingRatio = 1;     // scale factor
};

class FFTRegistration
{
  public:
    FFTRegistration(const cv::Mat &referenceImage, bool enableRotation, bool enableScaling, bool useHighpass);
    RegistrationResult evaluate(const cv::Mat &targetImage) const;
    cv::Mat align(const std::string &image_name, const cv::Mat &targetImage) const;

  private:
    bool enableRotation = false;
    bool enableScaling = false;
    bool useHighpass = true;

    int refW_ = 0, refH_ = 0;
    cv::Mat refPrep_;     // preprocessed reference (for translation)
    cv::Mat refPolarFFT_; // DFT of polar magnitude  (for rotation)
    int polarSize_ = 0;

    // Precomputed remap tables for the 0..180° polar transform
    cv::Mat polarMapX_, polarMapY_;

    static cv::Mat toGray32F(const cv::Mat &src);
    cv::Mat preprocess(const cv::Mat &src) const;

    cv::Mat computeMagnitudeSpectrum(const cv::Mat &img, int size) const;
    cv::Mat toPolar(const cv::Mat &mag, int size) const;
    double detectRotation(const cv::Mat &targetImg) const;

    static cv::Mat makeCPS(const cv::Mat &fftA, const cv::Mat &fftB);
    void buildPolarRemapTables(int size);
};

la_result run_registration(std::unordered_map<std::string, std::string> &args);
