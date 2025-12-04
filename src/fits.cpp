#include "fits.hpp"
#include "result.hpp"
#include <cstdio>
#include <fitsio.h>
#include <iostream>
#include <print>
#include <string>
#include <sys/types.h>

FitsFile::FitsFile(const std::string &filename, Mode mode)
{
    status = 0;

    if (mode == Mode::Create)
    {
        fits_create_file(&fptr, filename.c_str(), &status);
    }
    else
    {
        int iomode = (mode == Mode::ReadOnly) ? READONLY : READWRITE;
        fits_open_file(&fptr, filename.c_str(), iomode, &status);
    }

    if (status != 0)
    {
        throwFitsError("Failed to open/create file: " + filename);
    }

    if (mode != Mode::Create)
    {
        naxes.resize(3);
        fits_get_img_param(fptr, 3, &bitpix, &naxis, naxes.data(), &status);
        naxes.resize(naxis);

        if (status != 0)
        {
            throwFitsError("Failed to read image params: " + filename);
        }
    }

    this->name = filename;
}

FitsFile::~FitsFile()
{
    close();
}

void FitsFile::close() noexcept
{
    if (fptr)
    {
        int close_status = 0;
        fits_close_file(fptr, &close_status);

        if (close_status != 0)
        {
            std::println(std::cerr, "Error closing FITS file. Status: {}", close_status);
        }

        fptr = nullptr;
    }
}

FitsFile::FitsFile(FitsFile &&other) noexcept : fptr(other.fptr), status(other.status)
{
    other.fptr = nullptr;
}

FitsFile &FitsFile::operator=(FitsFile &&other) noexcept
{
    if (this != &other)
    {
        close();
        fptr = other.fptr;
        status = other.status;
        other.fptr = nullptr;
    }
    return *this;
}

FitsFile::operator fitsfile *() const
{
    return fptr;
}

void FitsFile::throwFitsError(const std::string &msg)
{
    char err_text[32];
    fits_get_errstatus(status, err_text);
    throw std::runtime_error(msg + " [" + std::string(err_text) + "]");
}

la_result FitsFile::writeComment(const std::string &msg)
{
    fits_write_comment(fptr, msg.c_str(), &status);

    return check_fits_status(status);
}

std::optional<std::string> FitsFile::readKey(const std::string &key)
{
    int bayer_status = 0;

    std::string comment;
    comment.resize(FLEN_COMMENT);
    char pat[32];

    if (fits_read_key(fptr, TSTRING, key.c_str(), pat, comment.data(), &bayer_status))
    {
        std::println("{} keyword: Not found.", key);
        return {};
    }
    std::string bayerpat(pat);
    return bayerpat;
}

la_result FitsFile::writeKey(const std::string &key, const std::string &value)
{
    int bayer_status = 0;

    std::string comment;

    if (fits_write_key(fptr, TSTRING, key.c_str(), (void*)value.c_str(), comment.c_str(), &bayer_status))
    {
        std::println("Could not write keyword.");
        return la_result::Error;
    }
    return la_result::Ok;
}

la_result check_fits_status(int status)
{
    if (status)
    {
        fits_report_error(stderr, status);
        std::println(std::cerr, "CFITSIO error occurred.");
        return la_result::Error;
    }
    return la_result::Ok;
}