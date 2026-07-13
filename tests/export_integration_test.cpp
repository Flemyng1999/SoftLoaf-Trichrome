#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QUuid>

#include <opencv2/core.hpp>
#include <tiffio.h>

#include "export_writer.hpp"
#include "softloaf_trichrome/color_management.hpp"

namespace fs = std::filesystem;
using softloaf::trichrome::ImageBuf;
namespace color = softloaf::trichrome::color;
namespace desktop = softloaf::trichrome::desktop;

namespace {

int failures = 0;

void Check(bool condition, const std::string& message) {
    if (!condition) {
        ++failures;
        std::cerr << "FAIL: " << message << '\n';
    }
}

ImageBuf Pattern(int width, int height) {
    ImageBuf image;
    image.data.create(height, width, CV_32FC3);
    for (int y = 0; y < height; ++y) {
        auto* row = image.data.ptr<cv::Vec3f>(y);
        for (int x = 0; x < width; ++x) {
            const float u = static_cast<float>(x) / std::max(1, width - 1);
            const float v = static_cast<float>(y) / std::max(1, height - 1);
            row[x] = {1.15f * u - 0.05f, 0.9f * v + 0.03f,
                      0.75f * (1.0f - u) + 0.15f * v};
        }
    }
    return image;
}

struct TiffInfo {
    uint32_t width = 0;
    uint32_t height = 0;
    uint16_t bits = 0;
    uint16_t samples = 0;
    uint16_t compression = 0;
    uint32_t icc_size = 0;
    std::vector<uint16_t> first_pixel;
};

TiffInfo ReadTiff(const QString& path) {
    TiffInfo info;
#ifdef _WIN32
    const fs::path native(path.toStdWString());
    TIFF* tif = TIFFOpenW(native.c_str(), "r");
#else
    const QByteArray encoded = QFile::encodeName(path);
    TIFF* tif = TIFFOpen(encoded.constData(), "r");
#endif
    if (!tif) return info;
    TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &info.width);
    TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &info.height);
    TIFFGetField(tif, TIFFTAG_BITSPERSAMPLE, &info.bits);
    TIFFGetField(tif, TIFFTAG_SAMPLESPERPIXEL, &info.samples);
    TIFFGetField(tif, TIFFTAG_COMPRESSION, &info.compression);
    void* icc = nullptr;
    TIFFGetField(tif, TIFFTAG_ICCPROFILE, &info.icc_size, &icc);
    if (info.width && info.samples == 3 && (info.bits == 8 || info.bits == 16)) {
        std::vector<unsigned char> row(static_cast<size_t>(TIFFScanlineSize(tif)));
        if (TIFFReadScanline(tif, row.data(), 0, 0) >= 0) {
            info.first_pixel.resize(3);
            if (info.bits == 16) {
                const auto* p = reinterpret_cast<const uint16_t*>(row.data());
                std::copy_n(p, 3, info.first_pixel.begin());
            } else {
                std::copy_n(row.begin(), 3, info.first_pixel.begin());
            }
        }
    }
    TIFFClose(tif);
    return info;
}

void TestColorSpacesAndDepths(const QString& root) {
    const ImageBuf image = Pattern(37, 23);
    for (const color::RgbColorSpace* space : color::kExportColorSpaces) {
        for (const int depth : {8, 16}) {
            const QString path = QDir(root).filePath(
                QString::fromUtf8(space->id.data()) + QString("_%1.tiff").arg(depth));
            QString reason;
            Check(desktop::WriteExport(image, QString::fromUtf8(space->id.data()),
                                       depth, path, &reason),
                  "write " + std::string(space->id) + " " + std::to_string(depth));
            const TiffInfo info = ReadTiff(path);
            Check(info.width == 37 && info.height == 23, "dimensions " + std::string(space->id));
            Check(info.bits == depth && info.samples == 3, "layout " + std::string(space->id));
            Check(info.compression == COMPRESSION_NONE, "compression " + std::string(space->id));
            Check(info.icc_size > 128, "ICC present " + std::string(space->id));
            Check(info.first_pixel.size() == 3, "pixel readable " + std::string(space->id));
            if (info.first_pixel.size() == 3) {
                const color::Mat3 matrix = color::LinearSrgbToColorSpaceMatrix(*space);
                const color::Vec3 encoded = color::ConvertLinearSrgbToEncoded(
                    *space, matrix, {-0.05, 0.03, 0.75});
                const double scale = depth == 16 ? 65535.0 : 255.0;
                for (int c = 0; c < 3; ++c) {
                    const auto expected = static_cast<uint16_t>(std::lround(
                        std::clamp(encoded[c], 0.0, 1.0) * scale));
                    Check(std::abs(static_cast<int>(info.first_pixel[c]) - expected) <= 1,
                          "encoded pixel " + std::string(space->id));
                }
            }
        }
    }
}

void TestUnicodeLongPathAndOverwrite(const QString& root) {
    QString dir = root;
    for (int i = 0; i < 5; ++i)
        dir = QDir(dir).filePath(QStringLiteral("很长的导出目录 with spaces %1").arg(i));
    Check(QDir().mkpath(dir), "create Unicode nested export directory");
    const QString path = QDir(dir).filePath(QStringLiteral("成片 Ω кадр 01.tiff"));
    QString reason;
    Check(desktop::WriteExport(Pattern(31, 19), QStringLiteral("display_p3"), 16,
                               path, &reason), "Unicode long-path export");
    Check(desktop::WriteExport(Pattern(17, 11), QStringLiteral("srgb"), 8,
                               path, &reason), "overwrite existing export");
    const TiffInfo info = ReadTiff(path);
    Check(info.width == 17 && info.height == 11 && info.bits == 8,
          "overwrite truncates and replaces container");
}

void TestFailures(const QString& root) {
    QString reason;
    ImageBuf empty;
    Check(!desktop::WriteExport(empty, QStringLiteral("srgb"), 16,
                                QDir(root).filePath("empty.tiff"), &reason),
          "empty input rejected");
    Check(!desktop::WriteExport(Pattern(2, 2), QStringLiteral("not_a_space"), 16,
                                QDir(root).filePath("bad-space.tiff"), &reason),
          "unknown color space rejected");
    const QString missing = QDir(root).filePath("missing/parent/frame.tiff");
    Check(!desktop::WriteExport(Pattern(2, 2), QStringLiteral("srgb"), 16,
                                missing, &reason), "missing parent rejected");
    Check(!QFileInfo::exists(missing), "failed export leaves no output");
}

void TestConcurrentBatch(const QString& root) {
    const QString dir = QDir(root).filePath(QStringLiteral("批量 batch"));
    QDir().mkpath(dir);
    const ImageBuf image = Pattern(1024, 768);
    std::atomic<int> successes{0};
    std::vector<std::thread> workers;
    for (int i = 0; i < 8; ++i) {
        workers.emplace_back([&, i] {
            QString reason;
            const QString path = QDir(dir).filePath(QString("frame_%1.tiff").arg(i));
            if (desktop::WriteExport(image, i % 2 ? QStringLiteral("acescg_linear")
                                                  : QStringLiteral("srgb"),
                                     i % 2 ? 16 : 8, path, &reason))
                ++successes;
        });
    }
    for (auto& worker : workers) worker.join();
    Check(successes == 8, "eight concurrent exports complete");
    for (int i = 0; i < 8; ++i) {
        const TiffInfo info = ReadTiff(QDir(dir).filePath(QString("frame_%1.tiff").arg(i)));
        Check(info.width == 1024 && info.height == 768, "concurrent output readable");
    }
}

void TestRealisticFullResolution(const QString& root) {
    const ImageBuf image = Pattern(4096, 3072);
    const QString path = QDir(root).filePath(QStringLiteral("12MP_16bit_AP0.tiff"));
    QString reason;
    Check(desktop::WriteExport(image, QStringLiteral("aces_ap0_linear"), 16,
                               path, &reason), "12MP 16-bit export");
    const TiffInfo info = ReadTiff(path);
    Check(info.width == 4096 && info.height == 3072 && info.bits == 16,
          "12MP output structure");
    Check(QFileInfo(path).size() > 4096LL * 3072LL * 6LL,
          "12MP output has expected payload");
}

void TestFilesystemOperations(const QString& root, const std::string& label) {
    const QString original = QDir(root).filePath(QStringLiteral("写入后重命名.tiff"));
    const QString renamed = QDir(root).filePath(QStringLiteral("重命名后的成片.tiff"));
    QString reason;
    Check(desktop::WriteExport(Pattern(257, 193), QStringLiteral("rec_2020_linear"),
                               16, original, &reason), label + ": initial write");
    Check(QFile::rename(original, renamed), label + ": rename on target filesystem");
    const TiffInfo info = ReadTiff(renamed);
    Check(info.width == 257 && info.height == 193, label + ": renamed file readable");
    Check(QFile::remove(renamed), label + ": delete on target filesystem");
    Check(!QFileInfo::exists(renamed), label + ": deletion visible immediately");
}

void TestRealTarget(const char* environment_name, const std::string& label) {
    const QString configured = qEnvironmentVariable(environment_name).trimmed();
    if (configured.isEmpty()) {
        std::cout << "SKIP: " << label << " target (set " << environment_name << ")\n";
        return;
    }
    const QDir target(configured);
    Check(target.exists(), label + ": configured root exists");
    if (!target.exists()) return;

    const QString leaf = QStringLiteral("SoftLoaf导出测试-%1")
        .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    const QString isolated = target.filePath(leaf);
    Check(QDir().mkpath(isolated), label + ": create isolated Chinese test directory");
    if (!QDir(isolated).exists()) return;

    TestUnicodeLongPathAndOverwrite(isolated);
    TestConcurrentBatch(isolated);
    TestFilesystemOperations(isolated, label);

    const ImageBuf image = Pattern(2048, 1536);
    const QString large = QDir(isolated).filePath(QStringLiteral("外部介质_3MP_16bit.tiff"));
    QString reason;
    Check(desktop::WriteExport(image, QStringLiteral("aces_ap0_linear"), 16,
                               large, &reason), label + ": multi-megabyte 16-bit write");
    Check(ReadTiff(large).width == 2048, label + ": large file read-after-write");

    Check(QDir(isolated).removeRecursively(), label + ": recursive cleanup");
    Check(!QFileInfo::exists(isolated), label + ": cleanup visible immediately");
}

}  // namespace

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    QTemporaryDir temp(QDir::tempPath() + QStringLiteral("/SoftLoaf 导出集成-XXXXXX"));
    Check(temp.isValid(), "temporary export root");
    if (!temp.isValid()) return 1;
    TestColorSpacesAndDepths(temp.path());
    TestUnicodeLongPathAndOverwrite(temp.path());
    TestFailures(temp.path());
    TestConcurrentBatch(temp.path());
    TestRealisticFullResolution(temp.path());
    TestFilesystemOperations(temp.path(), "local");
    TestRealTarget("SOFTLOAF_EXPORT_EXTERNAL_TARGET", "external-volume");
    TestRealTarget("SOFTLOAF_EXPORT_NAS_TARGET", "NAS/SMB");
    if (failures) std::cerr << failures << " export integration assertion(s) failed\n";
    else std::cout << "export integration tests passed\n";
    return failures ? 1 : 0;
}
