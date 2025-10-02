#include "fits.hpp"
#include <fitsio.h>
#include <print>
#include <iostream>

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