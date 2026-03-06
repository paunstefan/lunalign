#include "stack.hpp"
#include "fits.hpp"
#include "rate.hpp"
#include "result.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <print>
#include <string>
#include <vector>

#ifdef LUNALIGN_USE_OPENMP
#include <omp.h>
#endif

namespace fs = std::filesystem;

la_result run_stack(std::unordered_map<std::string, std::string> &args, PipelineContext &ctx)
{
    fs::path input_dir = args["in"];
    fs::path output_path = args["out"];
    float sigma = std::stof(args["sigma"]);
    bool use_weights = std::stoi(args["weighted"]) != 0;

    std::string method_str = args["method"];
    StackMethod method = StackMethod::SigmaClip;
    if (method_str == "mean")
        method = StackMethod::Mean;
    else if (method_str == "median")
        method = StackMethod::Median;

    // Collect FITS files
    std::vector<fs::path> fits_files;
    for (const auto &entry : fs::directory_iterator{input_dir})
    {
        if (entry.path().extension() == ".fits")
            fits_files.push_back(entry.path());
    }

    if (fits_files.empty())
    {
        std::println("Error: No FITS files found in '{}'.", input_dir.string());
        return la_result::Error;
    }

    std::sort(fits_files.begin(), fits_files.end());

    std::println("Stacking {} frames (method={}, sigma={:.2f}, weighted={})...", fits_files.size(), method_str, sigma,
                 use_weights);

    FrameStacker stacker(method, sigma, use_weights);

    for (int i = 0; i < static_cast<int>(fits_files.size()); ++i)
    {
        auto fits_file = FitsFile(fits_files[i], FitsFile::Mode::ReadOnly);
        auto mat = fits_file.readToCvMat<uint16_t>();

        float weight = 1.0f;
        if (use_weights)
        {
            FrameEvaluation evaluator;
            auto rating = evaluator.rate_image(fits_file);
            if (rating.has_value())
            {
                weight = rating.value();
            }
        }

        if (!stacker.addFrame(mat, weight))
        {
            std::println("Warning: '{}' has mismatched dimensions, skipping.", fits_files[i].filename().string());
            continue;
        }
        std::println("  loaded {}  (weight={:.1f})", fits_files[i].filename().string(), weight);
    }

    cv::Mat result = stacker.stack();

    if (result.empty())
    {
        std::println("Error: Stacking produced an empty result.");
        return la_result::Error;
    }

    // Result is already CV_32F from the stacker — write directly as float FITS
    std::string create_path = "!" + output_path.string();
    auto out_file = FitsFile(create_path, FitsFile::Mode::Create);
    out_file.writeCvMat<float>(result);

    std::println("Stacked image written to '{}'.", output_path.string());
    return la_result::Ok;
}

FrameStacker::FrameStacker(StackMethod method, float sigma, bool useWeights)
    : method_{method}, sigma_{sigma}, useWeights_{useWeights}
{
}

bool FrameStacker::addFrame(const cv::Mat &frame, float weight)
{
    cv::Mat f32;
    if (frame.depth() != CV_32F)
    {
        frame.convertTo(f32, CV_32F);
    }
    else
    {
        f32 = frame.clone();
    }

    // Validate dimensions match the first frame
    if (!frames_.empty())
    {
        const auto &ref = frames_[0];
        if (f32.rows != ref.rows || f32.cols != ref.cols || f32.channels() != ref.channels())
            return false;
    }

    frames_.push_back(std::move(f32));
    weights_.push_back(weight);
    return true;
}

cv::Mat FrameStacker::stack() const
{
    if (frames_.empty())
    {
        return {};
    }

    switch (method_)
    {
    case StackMethod::Mean:
        return stackMean();
    case StackMethod::Median:
        return stackMedian();
    case StackMethod::SigmaClip:
        return stackSigmaClip();
    }
    return {};
}

cv::Mat FrameStacker::stackMean() const
{
    int n = static_cast<int>(frames_.size());
    cv::Mat acc = cv::Mat::zeros(frames_[0].size(), frames_[0].type());
    float total_weight = 0.f;

    for (int i = 0; i < n; ++i)
    {
        float w = useWeights_ ? weights_[i] : 1.0f;
        acc += frames_[i] * w;
        total_weight += w;
    }

    acc /= total_weight;
    return acc;
}

cv::Mat FrameStacker::stackMedian() const
{
    int n = static_cast<int>(frames_.size());
    int rows = frames_[0].rows;
    int cols = frames_[0].cols;
    int channels = frames_[0].channels();

    cv::Mat result(rows, cols, frames_[0].type());

#ifdef LUNALIGN_USE_OPENMP
#pragma omp parallel
#endif
    {
        std::vector<float> buf(n);

#ifdef LUNALIGN_USE_OPENMP
#pragma omp for schedule(dynamic)
#endif
        for (int r = 0; r < rows; ++r)
        {
            for (int c = 0; c < cols * channels; ++c)
            {
                for (int f = 0; f < n; ++f)
                    buf[f] = frames_[f].ptr<float>(r)[c];

                std::nth_element(buf.begin(), buf.begin() + n / 2, buf.end());
                result.ptr<float>(r)[c] = buf[n / 2];
            }
        }
    }
    return result;
}

cv::Mat FrameStacker::stackSigmaClip() const
{
    int n = static_cast<int>(frames_.size());
    int rows = frames_[0].rows;
    int cols = frames_[0].cols;
    int channels = frames_[0].channels();

    cv::Mat result(rows, cols, frames_[0].type());

    constexpr int kClipPasses = 2;

#ifdef LUNALIGN_USE_OPENMP
#pragma omp parallel
#endif
    {
        std::vector<float> vals(n);
        std::vector<float> w(n);
        std::vector<bool> keep(n);

#ifdef LUNALIGN_USE_OPENMP
#pragma omp for schedule(dynamic)
#endif
        for (int r = 0; r < rows; ++r)
        {
            for (int c = 0; c < cols * channels; ++c)
            {
                for (int f = 0; f < n; ++f)
                {
                    vals[f] = frames_[f].ptr<float>(r)[c];
                    w[f] = useWeights_ ? weights_[f] : 1.0f;
                }

                std::fill(keep.begin(), keep.end(), true);
                int kept = n;

                for (int pass = 0; pass < kClipPasses; ++pass)
                {
                    if (kept < 3)
                    {
                        break;
                    }

                    // Weighted mean
                    float sum_w = 0.f, sum_wv = 0.f;
                    for (int f = 0; f < n; ++f)
                    {
                        if (!keep[f])
                        {
                            continue;
                        }
                        sum_wv += w[f] * vals[f];
                        sum_w += w[f];
                    }
                    if (sum_w == 0.f)
                    {
                        break;
                    }
                    float mean = sum_wv / sum_w;

                    // Standard deviation (unweighted for robust clipping)
                    float sum_sq = 0.f;
                    for (int f = 0; f < n; ++f)
                    {
                        if (!keep[f])
                        {
                            continue;
                        }
                        float diff = vals[f] - mean;
                        sum_sq += diff * diff;
                    }
                    float stddev = std::sqrt(sum_sq / static_cast<float>(kept));

                    if (stddev < 1e-10f)
                    {
                        break; // all values effectively identical
                    }

                    float lo = mean - sigma_ * stddev;
                    float hi = mean + sigma_ * stddev;
                    for (int f = 0; f < n; ++f)
                    {
                        if (keep[f] && (vals[f] < lo || vals[f] > hi))
                        {
                            keep[f] = false;
                            --kept;
                        }
                    }
                }

                // Final weighted mean of surviving pixels
                float sum_w = 0.f, sum_wv = 0.f;
                for (int f = 0; f < n; ++f)
                {
                    if (!keep[f])
                    {
                        continue;
                    }
                    sum_wv += w[f] * vals[f];
                    sum_w += w[f];
                }
                result.ptr<float>(r)[c] = (sum_w > 0.f) ? sum_wv / sum_w : 0.f;
            }
        }
    }
    return result;
}