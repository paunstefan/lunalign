#include "rate.hpp"
#include "result.hpp"
#include <cstdint>
#include <filesystem>
#include <fitsio.h>
#include <iostream>
#include <numeric>
#include <opencv2/core.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <optional>
#include <print>
#include <ranges>
#include <set>
#include <string.h>
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

la_result run_rate(std::unordered_map<std::string, std::string> &args)
{
    if (!args.contains("in"))
    {
        std::println("Error: Required argument 'in' for 'rate' not present!");
        return la_result::Error;
    }
    fs::path input_dir = args["in"];

    if (!args.contains("percent"))
    {
        std::println("Error: Required argument 'percent' for 'rate' not present!");
        return la_result::Error;
    }
    float percentage = std::stof(args["percent"]);

    fs::path output_dir = "process/rating_out";
    if (args.contains("out"))
    {
        output_dir = args["out"];
    }

    fs::create_directories(output_dir);

    std::set<ImageRating> images;
    FrameEvaluation evaluator;

    for (auto const &dir_entry : std::filesystem::directory_iterator{input_dir})
    {
        if (dir_entry.path().extension() == ".fits")
        {
            auto fits_file = FitsFile(dir_entry.path(), FitsFile::Mode::ReadOnly);

            auto rating = evaluator.rate_image(fits_file);
            if (rating.has_value())
            {
                images.insert({dir_entry.path(), rating.value()});
            }
        }
    }
    std::println("Count: {}", images.size());

    int images_to_save = static_cast<float>(images.size()) * (percentage / 100.0);
    if (images_to_save == 0)
    {
        images_to_save = 1;
    }

    for (auto &image : images | std::views::reverse)
    {
        std::println("{}: {}", image.path.string(), image.rating);
    }

    for (auto &image : images | std::views::reverse | std::views::take(images_to_save))
    {
        fs::path new_path = output_dir / image.path.filename();
        fs::copy_file(image.path, new_path, fs::copy_options::overwrite_existing);
    }

    la_result result = la_result::Ok;

    return result;
}

std::optional<float> FrameEvaluation::rate_image(FitsFile &image)
{
    if (image.naxis != 3)
    {
        std::println(std::cerr, "Number of axes != 3");
        return std::nullopt;
    }

    int width = image.naxes[0];
    int height = image.naxes[1];

    std::vector<uint16_t> green_layer = image.readPix<uint16_t>({1, 1, 2}, width * height);

    if (green_layer.empty())
    {
        return std::nullopt;
    }

    cv::Mat imageMat(height, width, CV_16UC1, green_layer.data());

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