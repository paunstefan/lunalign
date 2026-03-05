#pragma once
#include "fitsio.h"
#include "result.hpp"
#include <string>
#include <type_traits>
#include <vector>

#include <opencv2/core.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

template <class> inline constexpr bool always_false = false;

class FitsFile
{
    fitsfile *fptr = nullptr;
    int status = 0;

  public:
    std::string name;
    int bitpix = 0;
    int naxis = 0;
    std::vector<long> naxes;

    enum class Mode
    {
        ReadOnly,
        ReadWrite,
        Create // Creates new (overwrites if filename starts with '!')
    };

    FitsFile(const std::string &filename, Mode mode);

    ~FitsFile();
    FitsFile(FitsFile &&other) noexcept;
    FitsFile &operator=(FitsFile &&other) noexcept;

    FitsFile(const FitsFile &) = delete;
    FitsFile &operator=(const FitsFile &) = delete;

    operator fitsfile *() const;

    la_result check_status();

    void throwFitsError(const std::string &msg);

    la_result writeComment(const std::string &msg);

    void close() noexcept;

    template <typename T> std::tuple<int, int> getFitsTypes(const std::vector<T> &data)
    {
        int datatype, bitpx;

        if constexpr (std::is_same_v<T, double>)
        {
            datatype = TDOUBLE;
            bitpx = DOUBLE_IMG;
        }
        else if constexpr (std::is_same_v<T, float>)
        {
            datatype = TFLOAT;
            bitpx = FLOAT_IMG;
        }
        else if constexpr (std::is_same_v<T, uint32_t>)
        {
            datatype = TULONG;
            bitpx = ULONG_IMG;
        }
        else if constexpr (std::is_same_v<T, uint16_t>)
        {
            datatype = TUSHORT;
            bitpx = USHORT_IMG;
        }
        else if constexpr (std::is_same_v<T, uint8_t>)
        {
            datatype = TBYTE;
            bitpx = BYTE_IMG;
        }
        else
        {
            static_assert(always_false<T>, "Unsupported type for FITS write");
        }

        return {datatype, bitpx};
    }

    static int getCvType(const int fitsType)
    {
        int datatype;

        switch (fitsType)
        {
        case TFLOAT:
            return CV_32FC1;
        case TUSHORT:
            return CV_16UC1;
        default:
            return -1;
        }
    }

    template <typename T>
    la_result writeImage(const std::vector<T> &data, int naxis, std::vector<long> naxes, long long firstelem,
                         long long nelems)
    {
        const auto [datatype, bitpx] = getFitsTypes(data);

        this->bitpix = bitpx;
        this->naxis = naxis;
        this->naxes = naxes;

        fits_create_img(fptr, bitpix, naxis, naxes.data(), &status);
        if (status)
        {
            fits_report_error(stderr, status);
            return la_result::Error;
        }

        fits_write_img(fptr, datatype, firstelem, nelems, (void *)data.data(), &status);

        if (status)
        {
            fits_report_error(stderr, status);
            return la_result::Error;
        }
        return la_result::Ok;
    }

    template <typename T>
    la_result writePix(const std::vector<T> &data, int naxis, std::vector<long> naxes, std::vector<long> firstpix,
                       long long nelems)
    {
        const auto [datatype, bitpx] = getFitsTypes(data);

        this->bitpix = bitpx;
        this->naxis = naxis;
        this->naxes = naxes;

        fits_create_img(fptr, bitpix, naxis, naxes.data(), &status);
        if (status)
        {
            fits_report_error(stderr, status);
            return la_result::Error;
        }

        fits_write_pix(fptr, datatype, firstpix.data(), nelems, (void *)data.data(), &status);

        if (status)
        {
            fits_report_error(stderr, status);
            return la_result::Error;
        }
        return la_result::Ok;
    }

    template <typename T> std::vector<T> readPix(std::vector<long> firstpix, long long nelems)
    {
        std::vector<T> result(nelems);
        const auto [datatype, bitpx] = getFitsTypes(result);

        fits_read_pix(fptr, datatype, firstpix.data(), nelems, NULL, result.data(), NULL, &status);

        if (status)
        {
            fits_report_error(stderr, status);
            return {};
        }

        return result;
    }

    template <typename T> void writeCvMat(cv::Mat &mat)
    {
        int channels = mat.channels();
        int rows = mat.rows;
        int cols = mat.cols;

        if (channels == 1)
        {
            // Ensure contiguous memory
            if (!mat.isContinuous())
                mat = mat.clone();

            std::vector<T> data(mat.begin<T>(), mat.end<T>());

            this->writePix<T>(data,
                              2,                        // naxis
                              {(long)cols, (long)rows}, // naxes: NAXIS1=cols, NAXIS2=rows
                              {1, 1},                   // firstpix
                              (long long)cols * rows);
        }
        else
        {
            // Split channels back into FITS band-sequential layout
            std::vector<cv::Mat> planes;
            cv::split(mat, planes);

            long plane_size = (long)rows * cols;
            std::vector<T> data(plane_size * channels);

            for (int c = 0; c < channels; c++)
            {
                if (!planes[c].isContinuous())
                    planes[c] = planes[c].clone();
                std::memcpy(data.data() + c * plane_size, planes[c].data, plane_size * sizeof(T));
            }
            this->writePix<T>(data,
                              3,                                        // naxis
                              {(long)cols, (long)rows, (long)channels}, // NAXIS1, NAXIS2, NAXIS3
                              {1, 1, 1}, (long long)plane_size * channels);

            this->writeComment("CTYPE3 = 'RGB' / Color space");
            this->writeComment("CPLANE1 = 'RED' / Color plane 1");
            this->writeComment("CPLANE2 = 'GREEN' / Color plane 2");
            this->writeComment("CPLANE3 = 'BLUE' / Color plane 3");
        }
    }

    template <typename T> cv::Mat readToCvMat()
    {
        long nelements = this->naxes[0] * this->naxes[1];
        if (this->naxis > 2)
        {
            nelements *= this->naxes[2];
        }
        std::vector<T> image_data(nelements);
        const auto [datatype, bitpx] = getFitsTypes(image_data);
        const auto cv_type = getCvType(datatype);

        std::array<long, 3> firstpix = {1, 1, 1};
        fits_read_pix(fptr, datatype, firstpix.data(), nelements, NULL, image_data.data(), NULL, &status);

        if (status)
        {
            fits_report_error(stderr, status);
            return {};
        }

        cv::Mat mat;

        if (this->naxis > 2 && this->naxes[2] > 1)
        {
            int rows = this->naxes[1];
            int cols = this->naxes[0];
            int channels = this->naxes[2];
            long plane_size = rows * cols;

            std::vector<cv::Mat> planes;
            for (int c = 0; c < channels; c++)
            {
                // Each plane is contiguous in memory for FITS (band-sequential)
                cv::Mat plane(rows, cols, cv_type, image_data.data() + c * plane_size);
                planes.push_back(plane.clone());
            }
            cv::merge(planes, mat);
        }
        else
        {
            mat = cv::Mat(this->naxes[1], this->naxes[0], cv_type, image_data.data()).clone();
        }
        // cv::flip(mat, mat, 0);

        return mat;
    }

    std::optional<std::string> readKey(const std::string &key);
    la_result writeKey(const std::string &key, const std::string &value);
};

la_result check_fits_status(int status);
