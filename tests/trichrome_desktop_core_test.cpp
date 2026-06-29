#include <cassert>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include <vector>

#include <QCoreApplication>
#include <QColor>
#include <QDir>
#include <QImage>
#include <QStandardPaths>
#include <QUrl>

#include "obs_log.hpp"
#include "qt_path_utils.hpp"
#include "trichrome_cache.hpp"

namespace fs = std::filesystem;
namespace desktop = softloaf::trichrome::desktop;

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

}  // namespace

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName("SoftLoafTrichromeTest");
    QCoreApplication::setOrganizationName("SoftLoaf");

    TestObsLogSink();
    TestPreviewCacheIdentityAndHit();
    TestUnicodePathUtilities();
    return 0;
}
