#include "rate.hpp"
#include "result.hpp"
#include <cstdint>
#include <filesystem>
#include <fitsio.h>
#include <iostream>
#include <opencv2/core.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <optional>
#include <print>
#include <ranges>
#include <string>
#include <vector>

#include "fits.hpp"

namespace fs = std::filesystem;

struct ImageRating
{
    fs::path path;
    float rating;

    bool operator<(const ImageRating &other) const
    {
        return this->rating < other.rating;
    }
};

float rate_image(FitsFile &image);
float calculate_laplacian(int width, int height, std::vector<uint16_t> &image_data);

la_result run_rate(std::unordered_map<std::string, std::string> &args, PipelineContext &ctx)
{
    fs::path input_dir = args["in"];

    float percentage = std::stof(args["percent"]);

    fs::path output_dir = args["out"];

    fs::create_directories(output_dir);

    FrameEvaluation evaluator;

    std::vector<fs::path> fits_files;
    for (auto const &dir_entry : std::filesystem::directory_iterator{input_dir})
    {
        if (dir_entry.path().extension() == ".fits")
            fits_files.push_back(dir_entry.path());
    }

    std::vector<ImageRating> images(fits_files.size());

#ifdef LUNALIGN_USE_OPENMP
#pragma omp parallel for schedule(dynamic)
#endif
    for (int i = 0; i < static_cast<int>(fits_files.size()); ++i)
    {
        auto fits_file = FitsFile(fits_files[i], FitsFile::Mode::ReadOnly);

        auto rating = evaluator.rate_image(fits_file);
        if (rating.has_value())
        {
            images[i] = {fits_files[i], rating.value()};
            std::println("Evaluated image {}: {}", fits_files[i].string(), rating.value());
        }
    }

    std::println("Finished evaluating!\nCount: {}", images.size());

    int images_to_save = static_cast<float>(images.size()) * (percentage / 100.0);
    if (images_to_save == 0)
    {
        images_to_save = 1;
    }

    std::erase_if(images, [](const ImageRating &img) { return img.path.empty(); });

    if (images.empty())
    {
        std::println("No images were successfully rated.");
        return la_result::Error;
    }

    std::println("Copying best rated frames:");

    std::sort(images.begin(), images.end());

    for (auto &image : images | std::views::reverse | std::views::take(images_to_save))
    {
        fs::path new_path = output_dir / image.path.filename();
        std::println("{}: {}", image.path.filename().string(), image.rating);
        fs::copy_file(image.path, new_path, fs::copy_options::overwrite_existing);
    }

    auto &best = images.back();
    ctx["best_frame"] = best.path.filename().string();

    la_result result = la_result::Ok;

    return result;
}

std::optional<float> FrameEvaluation::rate_image(FitsFile &image)
{

    int width = image.naxes[0];
    int height = image.naxes[1];

    std::vector<uint16_t> mono_layer;

    if(image.naxis != 3){
        mono_layer = image.readPix<uint16_t>({1, 1, 1}, width * height);
    }
    else {
        mono_layer = image.readPix<uint16_t>({1, 1, 2}, width * height);
    }

    if (mono_layer.empty())
    {
        return std::nullopt;
    }

    cv::Mat imageMat(height, width, CV_16UC1, mono_layer.data());

    cv::Mat blurredMat;
    cv::Size kernelSize = cv::Size(5, 5);
    double sigmaX = 0;

    cv::GaussianBlur(imageMat, blurredMat, kernelSize, sigmaX);

    cv::Mat laplacianMat;
    cv::Laplacian(blurredMat, laplacianMat, CV_32F);

    cv::Scalar mean, stddev;
    cv::meanStdDev(laplacianMat, mean, stddev);

    return static_cast<float>(stddev[0] * stddev[0]);
}

#if 0
float calculate_laplacian(int width, int height, std::vector<uint16_t> &image_data)
{
    if (height < 2 || width < 2)
    {
        return -1;
    }

    std::vector<float> laplacian_values;
    laplacian_values.reserve(((height - 2) * (width - 2)));

    for (int y = 1; y < height - 1; y++)
    {
        for (int x = 1; x < width - 1; x++)
        {
            auto center = image_data[y * width + x];
            auto top = image_data[(y - 1) * width + x];
            auto bottom = image_data[(y + 1) * width + x];
            auto left = image_data[y * width + (x - 1)];
            auto right = image_data[y * width + (x + 1)];

            float lap_value = -4.0 * center + top + bottom + left + right;
            laplacian_values.push_back(lap_value);
        }
    }

    float mean = std::accumulate(laplacian_values.begin(), laplacian_values.end(), 0.0) /
                 static_cast<float>(laplacian_values.size());

    double sum_sq_diff =
        std::accumulate(laplacian_values.begin(), laplacian_values.end(), 0.0,
                        [mean](double accumulator, double value) { return accumulator + std::pow(value - mean, 2); });

    float variance = sum_sq_diff / static_cast<float>(laplacian_values.size());

    return variance;
}
#endif