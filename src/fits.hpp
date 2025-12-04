#pragma once
#include "fitsio.h"
#include "result.hpp"
#include <string>
#include <type_traits>
#include <vector>

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
        int datatype, bitpix;

        if constexpr (std::is_same_v<T, double>)
        {
            datatype = TDOUBLE;
            bitpix = DOUBLE_IMG;
        }
        else if constexpr (std::is_same_v<T, float>)
        {
            datatype = TFLOAT;
            bitpix = FLOAT_IMG;
        }
        else if constexpr (std::is_same_v<T, uint32_t>)
        {
            datatype = TULONG;
            bitpix = ULONG_IMG;
        }
        else if constexpr (std::is_same_v<T, uint16_t>)
        {
            datatype = TUSHORT;
            bitpix = USHORT_IMG;
        }
        else if constexpr (std::is_same_v<T, uint8_t>)
        {
            datatype = TBYTE;
            bitpix = BYTE_IMG;
        }
        else
        {
            static_assert(always_false<T>, "Unsupported type for FITS write");
        }

        return {datatype, bitpix};
    }

    template <typename T>
    la_result writeImage(const std::vector<T> &data, int naxis, std::vector<long> naxes, long long firstelem,
                         long long nelems)
    {
        const auto [datatype, bitpix] = getFitsTypes(data);

        this->bitpix = bitpix;
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
        const auto [datatype, bitpix] = getFitsTypes(data);

        this->bitpix = bitpix;
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
        const auto [datatype, bitpix] = getFitsTypes(result);

        fits_read_pix(fptr, datatype, firstpix.data(), nelems, NULL, result.data(), NULL, &status);

        if (status)
        {
            fits_report_error(stderr, status);
            return {};
        }

        return result;
    }

    std::optional<std::string> readKey(const std::string &key);
    la_result writeKey(const std::string &key, const std::string &value);
};

la_result check_fits_status(int status);
