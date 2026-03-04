#include "registration.hpp"
#include "rate.hpp"
#include "result.hpp"
#include <cstdint>
#include <filesystem>
#include <fitsio.h>
#include <opencv2/core.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <print>
#include <string.h>
#include <string>
#include <vector>

#include "fits.hpp"

namespace fs = std::filesystem;

la_result run_registration(std::unordered_map<std::string, std::string> &args)
{
    fs::path input_dir = args["in"];
    std::string reference_filename = args["reference"];
    fs::path output_dir = args["out"];

    fs::path reference_file = input_dir / reference_filename;

    fs::create_directories(output_dir);

    std::vector<fs::path> fits_files;
    for (auto const &dir_entry : std::filesystem::directory_iterator{input_dir})
    {
        if (dir_entry.path().extension() == ".fits")
            fits_files.push_back(dir_entry.path());
    }

    auto fits_ref = FitsFile(reference_file, FitsFile::Mode::ReadOnly);

    auto image_mat = fits_ref.readToCvMat<uint16_t>();

    FFTRegistration register_runner = FFTRegistration(image_mat,false,false,true);

    for (int i = 0; i < static_cast<int>(fits_files.size()); ++i)
    {
        const auto &path = fits_files[i];

        auto fits_file = FitsFile(path, FitsFile::Mode::ReadOnly);
        auto image_mat = fits_file.readToCvMat<uint16_t>();
        auto aligned = register_runner.align(image_mat);

        fs::path output_filename = output_dir / ("registered_" + path.filename().string());
        std::println("Registered file: {}", path.filename().string());
        std::string create_path = "!" + output_filename.string();
        auto out_file = FitsFile(create_path, FitsFile::Mode::Create);
        out_file.writeCvMat<uint16_t>(aligned);
    }

    return la_result::Ok;
}

cv::Mat FFTRegistration::toGray32F(const cv::Mat &src)
{
    cv::Mat gray;
    if (src.channels() > 1)
        cv::cvtColor(src, gray, cv::COLOR_BGR2GRAY);
    else
        gray = src;

    cv::Mat f;
    switch (gray.depth())
    {
    case CV_32F:
        return gray.clone();
    case CV_64F:
        gray.convertTo(f, CV_32F);
        return f;
    case CV_16U:
        gray.convertTo(f, CV_32F, 1.0 / 65535.0);
        return f;
    case CV_8U:
        gray.convertTo(f, CV_32F, 1.0 / 255.0);
        return f;
    default:
        gray.convertTo(f, CV_32F);
        cv::normalize(f, f, 0, 1, cv::NORM_MINMAX);
        return f;
    }
}

cv::Mat FFTRegistration::preprocess(const cv::Mat &src) const
{
    cv::Mat img = toGray32F(src);

    // Fill pure-black pixels with the median.
    // Prevents false edges at rotated-image borders.
    {
        std::vector<float> vals;
        vals.reserve(img.total());
        const float *p = img.ptr<float>();
        for (size_t i = 0; i < img.total(); ++i)
            if (p[i] > 0.f)
                vals.push_back(p[i]);
        if (!vals.empty())
        {
            std::nth_element(vals.begin(), vals.begin() + (long)(vals.size() / 2), vals.end());
            float med = vals[vals.size() / 2];
            float *q = img.ptr<float>();
            for (size_t i = 0; i < img.total(); ++i)
                if (q[i] == 0.f)
                    q[i] = med;
        }
    }

    cv::Mat result;

    if (useHighpass)
    {
        // Highpass = image − heavily-blurred copy.
        // Preserves more structure than a gradient on smooth lunar surfaces.
        cv::Mat blurred;
        cv::GaussianBlur(img, blurred, cv::Size(0, 0), 15);
        result = img - blurred;
    }
    else
    {
        // Classic gradient magnitude (Scharr ≈ Kroon quality in 3×3)
        cv::GaussianBlur(img, img, cv::Size(5, 5), 0);
        cv::Mat gx, gy;
        cv::Scharr(img, gx, CV_32F, 1, 0);
        cv::Scharr(img, gy, CV_32F, 0, 1);
        cv::magnitude(gx, gy, result);
    }

    // Normalise and suppress the bottom 25 %
    cv::normalize(result, result, 0, 1, cv::NORM_MINMAX);
    result = cv::max(result - 0.25f, 0.f);
    cv::normalize(result, result, 0, 1, cv::NORM_MINMAX);

    return result;
}

cv::Mat FFTRegistration::computeMagnitudeSpectrum(const cv::Mat &img, int size) const
{
    // Pad into optimal-size square
    cv::Mat padded = cv::Mat::zeros(size, size, CV_32F);
    int ox = (size - img.cols) / 2;
    int oy = (size - img.rows) / 2;
    img.copyTo(padded(cv::Rect(ox, oy, img.cols, img.rows)));

    // 2-D Hann window to reduce spectral leakage
    cv::Mat hann;
    cv::createHanningWindow(hann, padded.size(), CV_32F);
    padded = padded.mul(hann);

    // Forward DFT
    cv::Mat planes[] = {padded, cv::Mat::zeros(size, size, CV_32F)};
    cv::Mat cpx;
    cv::merge(planes, 2, cpx);
    cv::dft(cpx, cpx);

    // |DFT|
    cv::split(cpx, planes);
    cv::Mat mag;
    cv::magnitude(planes[0], planes[1], mag);

    // Shift quadrants so zero-frequency is centred
    int cx = mag.cols / 2, cy = mag.rows / 2;
    cv::Mat q0(mag, cv::Rect(0, 0, cx, cy));
    cv::Mat q1(mag, cv::Rect(cx, 0, cx, cy));
    cv::Mat q2(mag, cv::Rect(0, cy, cx, cy));
    cv::Mat q3(mag, cv::Rect(cx, cy, cx, cy));
    cv::Mat tmp;
    q0.copyTo(tmp);
    q3.copyTo(q0);
    tmp.copyTo(q3);
    q1.copyTo(tmp);
    q2.copyTo(q1);
    tmp.copyTo(q2);

    cv::normalize(mag, mag, 0, 1, cv::NORM_MINMAX);
    return mag;
}

cv::Mat FFTRegistration::toPolar(const cv::Mat &mag, int size) const
{
    cv::Point2f centre(size / 2.f, size / 2.f);
    double maxRadius = size / 2.0;

    int flags =
        cv::INTER_LINEAR | cv::WARP_FILL_OUTLIERS | (enableScaling ? cv::WARP_POLAR_LOG : cv::WARP_POLAR_LINEAR);

    cv::Mat polar;
    cv::warpPolar(mag, polar, cv::Size(size, size), centre, maxRadius, flags);

    // Keep only top half (0..180°) – magnitude spectrum is symmetric
    polar = polar(cv::Rect(0, 0, size, size / 2)).clone();
    cv::resize(polar, polar, cv::Size(size, size), 0, 0, cv::INTER_LINEAR);
    cv::normalize(polar, polar, 0, 1, cv::NORM_MINMAX);
    return polar;
}

cv::Mat FFTRegistration::makeCPS(const cv::Mat &fftA, const cv::Mat &fftB)
{
    cv::Mat pa[2], pb[2];
    cv::split(fftA, pa);
    cv::split(fftB, pb);

    // Numerator = A · conj(B)
    cv::Mat re = pa[0].mul(pb[0]) + pa[1].mul(pb[1]);
    cv::Mat im = pa[1].mul(pb[0]) - pa[0].mul(pb[1]);

    cv::Mat den;
    cv::magnitude(re, im, den);
    den = cv::max(den, 1e-10f);
    re /= den;
    im /= den;

    cv::Mat cps;
    cv::Mat ch[] = {re, im};
    cv::merge(ch, 2, cps);
    return cps;
}

double FFTRegistration::detectRotation(const cv::Mat &targetImg) const
{
    cv::Mat tgtPrep = preprocess(targetImg);
    cv::Mat tgtMag = computeMagnitudeSpectrum(tgtPrep, polarSize_);
    cv::Mat tgtPol = toPolar(tgtMag, polarSize_);

    // DFT of target polar image
    cv::Mat planes[] = {tgtPol, cv::Mat::zeros(tgtPol.size(), CV_32F)};
    cv::Mat c1;
    cv::merge(planes, 2, c1);
    cv::dft(c1, c1);

    // Cross-power spectrum → inverse DFT → peak
    cv::Mat cps = makeCPS(refPolarFFT_, c1);
    cv::Mat inv;
    cv::idft(cps, inv, cv::DFT_SCALE);
    cv::split(inv, planes);
    cv::Mat R;
    cv::magnitude(planes[0], planes[1], R);
    cv::normalize(R, R, 0, 1, cv::NORM_MINMAX);

    // Parabolic sub-pixel along Y (angle) axis
    cv::Point maxLoc;
    cv::minMaxLoc(R, nullptr, nullptr, nullptr, &maxLoc);
    int py = maxLoc.y;
    int h = R.rows;

    float yp = R.at<float>((py - 1 + h) % h, maxLoc.x);
    float y0 = R.at<float>(py, maxLoc.x);
    float yn = R.at<float>((py + 1) % h, maxLoc.x);

    double subY = py;
    double d = (double)yp - 2.0 * y0 + (double)yn;
    if (std::abs(d) > 1e-12)
        subY += 0.5 * ((double)yp - (double)yn) / d;

    // Y → angle (0..180° mapped over full height)
    double angle = 180.0 * subY / h;
    if (angle > 90.0)
        angle = 180.0 - angle;
    else
        angle = -angle;

    return angle;
}

FFTRegistration::FFTRegistration(const cv::Mat &referenceImage, bool enableRotation, bool enableScaling,
                                 bool useHighpass)
    : enableRotation{enableRotation}, enableScaling{enableScaling}, useHighpass{useHighpass}
{
    refW_ = referenceImage.cols;
    refH_ = referenceImage.rows;
    refPrep_ = preprocess(referenceImage);

    if (enableRotation)
    {
        polarSize_ = cv::getOptimalDFTSize(std::max(refW_, refH_));
        cv::Mat mag = computeMagnitudeSpectrum(refPrep_, polarSize_);
        cv::Mat pol = toPolar(mag, polarSize_);

        cv::Mat planes[] = {pol, cv::Mat::zeros(pol.size(), CV_32F)};
        cv::merge(planes, 2, refPolarFFT_);
        cv::dft(refPolarFFT_, refPolarFFT_);
    }
}

RegistrationResult FFTRegistration::evaluate(const cv::Mat &targetImage) const
{
    RegistrationResult res;

    // 1. Rotation
    if (enableRotation)
        res.rotationAngleDeg = detectRotation(targetImage);

    // 2. Translation – rotate the *original* target first if needed,
    //    so the translation measurement is clean.
    cv::Mat tgtForTrans;
    if (enableRotation && std::abs(res.rotationAngleDeg) > 0.01)
    {
        cv::Point2f ctr(targetImage.cols / 2.f, targetImage.rows / 2.f);
        cv::Mat M = cv::getRotationMatrix2D(ctr, res.rotationAngleDeg, 1.0);
        cv::Mat rotated;
        cv::warpAffine(targetImage, rotated, M, targetImage.size(), cv::INTER_LINEAR, cv::BORDER_REPLICATE);
        tgtForTrans = preprocess(rotated);
    }
    else
    {
        tgtForTrans = preprocess(targetImage);
    }

    // Pad to matching optimal DFT size
    int optW = cv::getOptimalDFTSize(std::max(refPrep_.cols, tgtForTrans.cols));
    int optH = cv::getOptimalDFTSize(std::max(refPrep_.rows, tgtForTrans.rows));

    cv::Mat refPad, tgtPad;
    cv::copyMakeBorder(refPrep_, refPad, 0, optH - refPrep_.rows, 0, optW - refPrep_.cols, cv::BORDER_CONSTANT);
    cv::copyMakeBorder(tgtForTrans, tgtPad, 0, optH - tgtForTrans.rows, 0, optW - tgtForTrans.cols,
                       cv::BORDER_CONSTANT);

    // cv::phaseCorrelate handles Hann windowing + sub-pixel internally
    cv::Mat hann;
    cv::createHanningWindow(hann, refPad.size(), CV_32F);
    cv::Point2d shift = cv::phaseCorrelate(refPad, tgtPad, hann);

    res.dx = shift.x;
    res.dy = shift.y;
    return res;
}

cv::Mat FFTRegistration::align(const cv::Mat &targetImage) const
{
    RegistrationResult res = evaluate(targetImage);

    std::printf("[FFTReg] dx=%+.2f  dy=%+.2f  rot=%+.3f°  scale=%.4f\n", res.dx, res.dy, res.rotationAngleDeg,
                res.scalingRatio);

    // Combined affine: rotate about centre, then translate
    cv::Point2f ctr(targetImage.cols / 2.f, targetImage.rows / 2.f);
    cv::Mat M = cv::getRotationMatrix2D(ctr, res.rotationAngleDeg, 1.0);
    M.at<double>(0, 2) -= res.dx;
    M.at<double>(1, 2) -= res.dy;

    cv::Mat aligned;
    cv::warpAffine(targetImage, aligned, M, cv::Size(refW_, refH_), cv::INTER_LANCZOS4, cv::BORDER_CONSTANT,
                   cv::Scalar(0));
    return aligned;
}
