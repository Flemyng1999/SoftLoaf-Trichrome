#include "export_writer.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <vector>

#include <QFile>

#include <tiffio.h>

#include "softloaf_trichrome/color_management.hpp"

namespace softloaf::trichrome::desktop {

bool WriteExport(const ImageBuf& image, const QString& output_color_space,
                 int bit_depth, const QString& path, QString* reason) {
    if (image.empty() || image.data.type() != CV_32FC3) {
        if (reason) *reason = QStringLiteral("Expected non-empty linear RGB image");
        return false;
    }
    const color::RgbColorSpace* space =
        color::LookupColorSpace(output_color_space.toStdString());
    if (!space) {
        if (reason) *reason = QStringLiteral("Unknown output color space");
        return false;
    }

#ifdef _WIN32
    const std::filesystem::path native_path(path.toStdWString());
    TIFF* tif = TIFFOpenW(native_path.c_str(), "w");
#else
    const QByteArray encoded_path = QFile::encodeName(path);
    TIFF* tif = TIFFOpen(encoded_path.constData(), "w");
#endif
    if (!tif) {
        if (reason) *reason = QStringLiteral("TIFFOpen failed");
        return false;
    }

    const uint16_t bits = bit_depth >= 16 ? 16 : 8;
    bool ok = TIFFSetField(tif, TIFFTAG_IMAGEWIDTH, static_cast<uint32_t>(image.width())) &&
              TIFFSetField(tif, TIFFTAG_IMAGELENGTH, static_cast<uint32_t>(image.height())) &&
              TIFFSetField(tif, TIFFTAG_SAMPLESPERPIXEL, 3) &&
              TIFFSetField(tif, TIFFTAG_BITSPERSAMPLE, bits) &&
              TIFFSetField(tif, TIFFTAG_ORIENTATION, ORIENTATION_TOPLEFT) &&
              TIFFSetField(tif, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG) &&
              TIFFSetField(tif, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_RGB) &&
              TIFFSetField(tif, TIFFTAG_COMPRESSION, COMPRESSION_NONE);
    const std::vector<unsigned char> icc = color::CreateIccProfile(*space);
    if (ok && !icc.empty()) {
        ok = TIFFSetField(tif, TIFFTAG_ICCPROFILE,
                          static_cast<uint32_t>(icc.size()), icc.data());
    }
    if (ok) TIFFSetField(tif, TIFFTAG_ROWSPERSTRIP, TIFFDefaultStripSize(tif, 0));

    const color::Mat3 matrix = color::LinearSrgbToColorSpaceMatrix(*space);
    if (bits == 16) {
        std::vector<uint16_t> row(static_cast<size_t>(image.width()) * 3);
        for (int y = 0; y < image.height() && ok; ++y) {
            const auto* src = image.data.ptr<cv::Vec3f>(y);
            for (int x = 0; x < image.width(); ++x) {
                const color::Vec3 rgb = color::ConvertLinearSrgbToEncoded(
                    *space, matrix, {src[x][0], src[x][1], src[x][2]});
                for (int c = 0; c < 3; ++c) row[3 * x + c] = static_cast<uint16_t>(
                    std::lround(std::clamp(rgb[c], 0.0, 1.0) * 65535.0));
            }
            ok = TIFFWriteScanline(tif, row.data(), static_cast<uint32_t>(y), 0) >= 0;
        }
    } else {
        std::vector<uint8_t> row(static_cast<size_t>(image.width()) * 3);
        for (int y = 0; y < image.height() && ok; ++y) {
            const auto* src = image.data.ptr<cv::Vec3f>(y);
            for (int x = 0; x < image.width(); ++x) {
                const color::Vec3 rgb = color::ConvertLinearSrgbToEncoded(
                    *space, matrix, {src[x][0], src[x][1], src[x][2]});
                for (int c = 0; c < 3; ++c) row[3 * x + c] = static_cast<uint8_t>(
                    std::lround(std::clamp(rgb[c], 0.0, 1.0) * 255.0));
            }
            ok = TIFFWriteScanline(tif, row.data(), static_cast<uint32_t>(y), 0) >= 0;
        }
    }
    TIFFClose(tif);
    if (!ok) {
        QFile::remove(path);
        if (reason) *reason = QStringLiteral("TIFF scanline write failed");
        return false;
    }
    if (reason) *reason = QStringLiteral("ok");
    return true;
}

}  // namespace softloaf::trichrome::desktop
