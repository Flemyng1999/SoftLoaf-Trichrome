#include <cassert>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include <vector>

#include <QCoreApplication>
#include <QColor>
#include <QColorSpace>
#include <QDir>
#include <QImage>
#include <QImageReader>
#include <QImageWriter>
#include <QSet>
#include <QStandardPaths>
#include <QUrl>

#include <opencv2/imgcodecs.hpp>

#include "obs_log.hpp"
#include "desktop_decoder.hpp"
#include "export_naming.hpp"
#include "export_writer.hpp"
#include "qt_path_utils.hpp"
#include "softloaf_trichrome/color_management.hpp"
#include "softloaf_trichrome/composer.hpp"
#include "softloaf_trichrome/model.hpp"
#include "trichrome_cache.hpp"

namespace fs = std::filesystem;
namespace desktop = softloaf::trichrome::desktop;
namespace color = softloaf::trichrome::color;
namespace tri = softloaf::trichrome;

namespace {

fs::path WriteFile(const fs::path& path, const std::string& bytes) {
    fs::create_directories(path.parent_path());
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    f << bytes;
    return path;
}

bool HasField(const desktop::LogRecord& record,
              const std::string& key,
              const std::string& value) {
    for (const auto& field : record.fields) {
        if (field.first == key && field.second == value) return true;
    }
    return false;
}

void TestObsLogSink() {
    std::vector<desktop::LogRecord> records;
    std::mutex mu;
    const desktop::SinkHandle sink = desktop::InstallSink(
        [&](const desktop::LogRecord& record) {
            std::lock_guard<std::mutex> lock(mu);
            records.push_back(record);
        });
    desktop::ObsLog("test.category", {{"event", "hello"}, {"result", "ok"}});
    desktop::RemoveSink(sink);
    assert(records.size() == 1);
    assert(records[0].category == "test.category");
    assert(HasField(records[0], "event", "hello"));
    assert(HasField(records[0], "result", "ok"));
}

void TestPreviewCacheIdentityAndHit() {
    const fs::path root = fs::temp_directory_path() / "softloaf_trichrome_cache_test";
    std::error_code ec;
    fs::remove_all(root, ec);
    const fs::path r = WriteFile(root / "R.raf", "red");
    const fs::path g = WriteFile(root / "G.raf", "green");
    const fs::path b = WriteFile(root / "B.raf", "blue");

    desktop::TrichromePreviewCache cache;
    desktop::TrichromeCacheInput input;
    input.paths = {r, g, b};
    input.sensor_mode = "mono";
    input.role_order = "RGB";
    input.max_edge = 2048;
    const std::string key = cache.keyFor(input);
    assert(!key.empty());

    desktop::TrichromeCacheInput small_input = input;
    small_input.max_edge = 1024;
    assert(cache.keyFor(small_input) != key);

    desktop::TrichromeCacheInput export_input = input;
    export_input.decode_mode = "export";
    assert(cache.keyFor(export_input) != key);

    desktop::TrichromeCacheInput fallback_input = input;
    fallback_input.raw_decode_policy = "raw-boundary-test:fallback-changed";
    assert(cache.keyFor(fallback_input) != key);

    desktop::TrichromeCacheInput provenance_input = input;
    provenance_input.raw_provenance_identity = "raw-provenance-test:changed";
    assert(cache.keyFor(provenance_input) != key);

    const desktop::CacheLookupResult miss = cache.lookup(input);
    assert(!miss.hit);

    QImage image(8, 6, QImage::Format_RGB888);
    image.fill(QColor(25, 50, 75));
    fs::path written;
    std::string reason;
    assert(cache.write(key, image, &written, &reason));
    assert(reason == "ok");
    assert(fs::exists(written));
    assert(written.extension() == ".jpg");

    const desktop::CacheLookupResult hit = cache.lookup(input);
    assert(hit.hit);
    assert(hit.image.size() == image.size());

    WriteFile(r, "changed red");
    const std::string changed_key = cache.keyFor(input);
    assert(changed_key != key);

    fs::remove_all(root, ec);
}

void TestUnicodePathUtilities() {
    const fs::path root = fs::temp_directory_path() / "softloaf 路径 test";
    std::error_code ec;
    fs::remove_all(root, ec);
    fs::create_directories(root, ec);
    const fs::path file = root / "导出 frame 01.tiff";
    const QString qpath = desktop::QStringFromPath(file);
    const fs::path round_trip = desktop::PathFromQString(qpath);
    assert(round_trip == file);

    const QUrl url = QUrl::fromLocalFile(qpath);
    assert(desktop::LocalPathFromUrl(url) == QDir::cleanPath(QDir::fromNativeSeparators(qpath)));
    assert(desktop::WriteFileBytes(file, QByteArray("ok")));
    assert(desktop::ReadFileBytes(file) == QByteArray("ok"));
    fs::remove_all(root, ec);
}

void TestGeneratedIccProfilesRoundTripThroughTiff() {
    for (const color::RgbColorSpace* space : color::kExportColorSpaces) {
        const std::vector<unsigned char> icc = color::CreateIccProfile(*space);
        assert(!icc.empty());
        const QByteArray bytes(reinterpret_cast<const char*>(icc.data()),
                               static_cast<int>(icc.size()));
        const QColorSpace qt_space = QColorSpace::fromIccProfile(bytes);
        assert(qt_space.isValid());
    }

    const fs::path root = fs::temp_directory_path() / "softloaf_trichrome_icc_test";
    std::error_code ec;
    fs::remove_all(root, ec);
    fs::create_directories(root, ec);

    const std::vector<unsigned char> icc = color::CreateIccProfile(color::kAcesCgLinear);
    const QByteArray bytes(reinterpret_cast<const char*>(icc.data()),
                           static_cast<int>(icc.size()));
    const QColorSpace acescg = QColorSpace::fromIccProfile(bytes);
    assert(acescg.isValid());

    QImage image(8, 6, QImage::Format_RGBX64);
    image.fill(QColor(25, 50, 75));
    image.setColorSpace(acescg);
    const QString path = desktop::QStringFromPath(root / "acescg_16bit_icc.tiff");
    QImageWriter writer(path, QByteArrayLiteral("tiff"));
    assert(writer.write(image));

    QImageReader reader(path, QByteArrayLiteral("tiff"));
    const QImage read = reader.read();
    assert(!read.isNull());
    assert(read.depth() >= 48);
    assert(read.colorSpace().isValid());
    assert(!read.colorSpace().iccProfile().isEmpty());

    fs::remove_all(root, ec);
}

void TestRegularTiffDecodeIsNotRawOnly() {
    assert(tri::IsRawLikeExtension(".tiff"));
    assert(tri::IsRawLikeExtension(".tif"));
    assert(tri::IsSupportedStillImageExtension(".tiff"));
    assert(tri::IsSupportedStillImageExtension(".tif"));

    const fs::path root = fs::temp_directory_path() / "softloaf_trichrome_decode_tiff_test";
    std::error_code ec;
    fs::remove_all(root, ec);
    fs::create_directories(root, ec);

    QImage image(12, 10, QImage::Format_RGBX64);
    image.fill(QColor(120, 180, 220));
    const fs::path path = root / "scan.tiff";
    QImageWriter writer(desktop::QStringFromPath(path), QByteArrayLiteral("tiff"));
    assert(writer.write(image));

    assert(!desktop::LooksLikeRaw(path));
    const tri::ImageBuf decoded = desktop::DecodeLinear(path, true, desktop::DecodeMode::kPreview);
    assert(!decoded.data.empty());
    assert(decoded.width() == 12);
    assert(decoded.height() == 10);
    assert(decoded.data.channels() == 1);
    assert(decoded.color_space == "linear_srgb");

    fs::remove_all(root, ec);
}

void TestLinearMonoTiffDecodeAndCompose() {
    const fs::path root = fs::temp_directory_path() / "softloaf_trichrome_mono_tiff_test";
    std::error_code ec;
    fs::remove_all(root, ec);
    fs::create_directories(root, ec);

    const fs::path mono8_path = root / "mono8.tiff";
    const fs::path mono16_path = root / "mono16.tiff";
    const cv::Mat mono8(1, 1, CV_8UC1, cv::Scalar(128));
    const cv::Mat mono16(1, 1, CV_16UC1, cv::Scalar(32768));
    assert(cv::imwrite(mono8_path.string(), mono8));
    assert(cv::imwrite(mono16_path.string(), mono16));

    const tri::ImageBuf decoded8 = desktop::DecodeLinear(
        mono8_path, true, desktop::DecodeMode::kPreview);
    const tri::ImageBuf decoded16 = desktop::DecodeLinear(
        mono16_path, true, desktop::DecodeMode::kPreview);
    const float expected8 = 128.0f / 255.0f;
    const float expected16 = 32768.0f / 65535.0f;
    assert(decoded8.is_mono());
    assert(decoded16.is_mono());
    assert(std::abs(decoded8.data.at<float>(0, 0) - expected8) < 1e-6f);
    assert(std::abs(decoded16.data.at<float>(0, 0) - expected16) < 1e-6f);
    assert(std::abs(decoded8.data.at<float>(0, 0) - 0.21586f) > 0.1f);

    const fs::path red_path = root / "red.tiff";
    const fs::path green_path = root / "green.tiff";
    const fs::path blue_path = root / "blue.tiff";
    const cv::Mat red(1, 1, CV_8UC1, cv::Scalar(64));
    const cv::Mat green(1, 1, CV_8UC1, cv::Scalar(128));
    const cv::Mat blue(1, 1, CV_8UC1, cv::Scalar(192));
    assert(cv::imwrite(red_path.string(), red));
    assert(cv::imwrite(green_path.string(), green));
    assert(cv::imwrite(blue_path.string(), blue));
    const tri::ComposeResult composed = tri::ComposeMonoRgb(
        desktop::DecodeLinear(red_path, true, desktop::DecodeMode::kPreview),
        desktop::DecodeLinear(green_path, true, desktop::DecodeMode::kPreview),
        desktop::DecodeLinear(blue_path, true, desktop::DecodeMode::kPreview));
    assert(composed.ok);
    const cv::Vec3f pixel = composed.rgb.data.at<cv::Vec3f>(0, 0);
    assert(std::abs(pixel[0] - 64.0f / 255.0f) < 1e-6f);
    assert(std::abs(pixel[1] - 128.0f / 255.0f) < 1e-6f);
    assert(std::abs(pixel[2] - 192.0f / 255.0f) < 1e-6f);

    fs::remove_all(root, ec);
}

void TestExportNamingUsesSourceStemAndAvoidsOverwrite() {
    const fs::path root = fs::temp_directory_path() / "softloaf_trichrome_export_name_test";
    std::error_code ec;
    fs::remove_all(root, ec);
    fs::create_directories(root, ec);

    softloaf::trichrome::ProjectTrichromeGroup group;
    group.group_index = 0;
    group.logical_frame_index = 0;
    softloaf::trichrome::ProjectTrichromeSource source;
    const QString source_path = desktop::QStringFromPath(root / "11.raf");
    source.path = source_path.toUtf8().toStdString();
    group.sources.push_back(std::move(source));

    const QString folder = desktop::QStringFromPath(root);
    assert(desktop::ExportBaseNameForGroup(group) == QStringLiteral("11_rgb"));
    assert(desktop::ExportBaseNameForGroup(group, QStringLiteral("_final")) ==
           QStringLiteral("11_final"));
    assert(desktop::ExportBaseNameForGroup(group, QStringLiteral("")) ==
           QStringLiteral("11_rgb"));
    assert(desktop::ExportBaseNameForGroup(group, QStringLiteral("_bad/name")) ==
           QStringLiteral("11_bad_name"));
    assert(desktop::UniqueExportPathForGroup(folder, group, QStringLiteral("tiff")) ==
           QDir(folder).filePath(QStringLiteral("11_rgb.tiff")));

    WriteFile(root / "11_rgb.tiff", "existing export");
    QSet<QString> reserved_paths;
    assert(desktop::UniqueExportPathForGroup(folder, group, QStringLiteral("tiff"),
                                            &reserved_paths) ==
           QDir(folder).filePath(QStringLiteral("11_rgb_2.tiff")));
    assert(desktop::UniqueExportPathForGroup(folder, group, QStringLiteral("tiff"),
                                            &reserved_paths) ==
           QDir(folder).filePath(QStringLiteral("11_rgb_3.tiff")));

    softloaf::trichrome::ProjectTrichromeGroup fallback_group;
    fallback_group.group_index = 4;
    fallback_group.logical_frame_index = -1;
    assert(desktop::ExportBaseNameForGroup(fallback_group) ==
           QStringLiteral("trichrome_frame_0005_rgb"));

    fs::remove_all(root, ec);
}

void TestSafeExportWriterHandlesUnicodeAndFailures() {
    const fs::path root = fs::temp_directory_path() / "softloaf 导出稳定性 test";
    std::error_code ec;
    fs::remove_all(root, ec);
    fs::create_directories(root, ec);

    QImage image(12, 10, QImage::Format_RGBX64);
    image.fill(QColor(60, 120, 180));
    QString reason;
    const QString output = desktop::QStringFromPath(root / "中文成片.tiff");
    assert(desktop::WriteExportImageSafely(
        image, output, QByteArrayLiteral("tiff"), &reason));
    assert(reason == QStringLiteral("ok"));
    assert(!QImageReader(output, QByteArrayLiteral("tiff")).read().isNull());

    const QString missing_parent =
        desktop::QStringFromPath(root / "disconnected" / "frame.tiff");
    assert(!desktop::WriteExportImageSafely(
        image, missing_parent, QByteArrayLiteral("tiff"), &reason));
    assert(!reason.isEmpty());
    assert(!QFileInfo::exists(missing_parent));

    fs::remove_all(root, ec);
}

}  // namespace

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName("SoftLoafTrichromeTest");
    QCoreApplication::setOrganizationName("SoftLoaf");
    qputenv("SOFTLOAF_TRICHROME_CACHE_ROOT",
            desktop::QStringFromPath(fs::temp_directory_path() /
                                      "softloaf_trichrome_test_cache_root").toUtf8());

    TestObsLogSink();
    TestPreviewCacheIdentityAndHit();
    TestUnicodePathUtilities();
    TestGeneratedIccProfilesRoundTripThroughTiff();
    TestRegularTiffDecodeIsNotRawOnly();
    TestLinearMonoTiffDecodeAndCompose();
    TestExportNamingUsesSourceStemAndAvoidsOverwrite();
    TestSafeExportWriterHandlesUnicodeAndFailures();
    return 0;
}
