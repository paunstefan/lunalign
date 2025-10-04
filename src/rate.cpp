#include "rate.hpp"
#include "result.hpp"
#include <cstdint>
#include <filesystem>
#include <fitsio.h>
#include <iostream>
#include <numeric>
#include <print>
#include <ranges>
#include <set>
#include <string.h>
#include <string>
#include <vector>
#include <opencv2/opencv.hpp>

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

float rate_image(fitsfile *image);
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

    for (auto const &dir_entry : std::filesystem::directory_iterator{input_dir})
    {
        if (dir_entry.path().extension() == ".fits")
        {
            fitsfile *in_fptr = nullptr;
            int status = 0;
            if (fits_open_file(&in_fptr, dir_entry.path().c_str(), READONLY, &status))
            {
                check_fits_status(status);
                return la_result::Error;
            }

            float rating = rate_image(in_fptr);
            images.insert({dir_entry.path(), rating});

            fits_close_file(in_fptr, &status);
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

    int copied = 0;
    for (auto &image : images | std::views::reverse)
    {
        fs::path new_path = output_dir / image.path.filename();
        fs::copy_file(image.path, new_path, fs::copy_options::overwrite_existing);
        copied++;
        if (copied >= images_to_save)
        {
            break;
        }
    }

    la_result result = la_result::Ok;

    return result;
}

float rate_image(fitsfile *image)
{

    int bitpix, naxis;
    long naxes[3] = {1, 1, 1};
    // long fpixel[3] = {1, 1, 1};
    long fpixel[3] = {1, 1, 2}; // Starting pixel coordinates for reading green
    long nelements;
    int status = 0;

    if (fits_get_img_param(image, 3, &bitpix, &naxis, naxes, &status))
    {
        check_fits_status(status);
        return -1;
    }

    if (naxis != 3)
    {
        std::println(std::cerr, "Number of axes != 3");
        return -1;
    }

    int width = naxes[0];
    int height = naxes[1];

    nelements = naxes[0] * naxes[1] * naxes[2];

    // std::vector<uint16_t> image_data(nelements);

    // if (fits_read_pix(image, TUSHORT, fpixel, nelements, NULL, image_data.data(), NULL, &status))
    // {
    //     check_fits_status(status);
    //     return -1;
    // }

    std::vector<uint16_t> green_layer(naxes[0] * naxes[1]);

    if (fits_read_pix(image, TUSHORT, fpixel, naxes[0] * naxes[1], NULL, green_layer.data(), NULL, &status))
    {
        check_fits_status(status);
        return -1;
    }

    cv::Mat imageMat(height, width, CV_16UC1, green_layer.data());

    cv::Mat blurredMat;
    cv::Size kernelSize = cv::Size(5, 5); 
    double sigmaX = 0; 
    
    cv::GaussianBlur(imageMat, blurredMat, kernelSize, sigmaX);

    uint16_t* p_start = (uint16_t*)blurredMat.datastart;
    
    uint16_t* p_end = (uint16_t*)blurredMat.dataend;
    
    std::vector<uint16_t> blurredVector(p_start, p_end);

    auto score = calculate_laplacian(naxes[0], naxes[1], blurredVector);

    return score;
}

float calculate_laplacian(int width, int height, std::vector<uint16_t> &image_data)
{
    if (height < 2 || width < 2)
    {
        return -1;
    }

    std::vector<float> laplacian_values((height - 2) * (width - 2));

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