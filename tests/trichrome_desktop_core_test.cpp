#include <cassert>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include <vector>

#include <QCoreApplication>
#include <QColor>
#include <QImage>
#include <QStandardPaths>

#include "obs_log.hpp"
#include "project_store.hpp"
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
    const std::string key = cache.keyFor(input);
    assert(!key.empty());

    const desktop::CacheLookupResult miss = cache.lookup(input);
    assert(!miss.hit);

    QImage image(8, 6, QImage::Format_RGB888);
    image.fill(QColor(25, 50, 75));
    fs::path written;
    std::string reason;
    assert(cache.write(key, image, &written, &reason));
    assert(reason == "ok");
    assert(fs::exists(written));

    const desktop::CacheLookupResult hit = cache.lookup(input);
    assert(hit.hit);
    assert(hit.image.size() == image.size());

    WriteFile(r, "changed red");
    const std::string changed_key = cache.keyFor(input);
    assert(changed_key != key);

    fs::remove_all(root, ec);
}

void TestProjectStoreRoundTrip() {
    const fs::path root = fs::temp_directory_path() / "softloaf_trichrome_project_test";
    std::error_code ec;
    fs::remove_all(root, ec);
    const fs::path project_path = root / "project" / "session.sltrichrome";
    const fs::path r = WriteFile(root / "project" / "roll" / "R.raf", "red");
    const fs::path g = WriteFile(root / "project" / "roll" / "G.raf", "green");
    const fs::path b = WriteFile(root / "project" / "roll" / "B.raf", "blue");

    desktop::ProjectDocument saved;
    saved.sensor_mode = "bayer";
    saved.role_order = "BGR";
    saved.sort_mode = "selection";
    saved.active_group = 0;
    saved.files = {{r, 2}, {g, 1}, {b, 0}};

    std::string err;
    assert(desktop::SaveProjectDocument(project_path, saved, &err));
    assert(err == "ok");
    assert(fs::exists(project_path));

    desktop::ProjectDocument loaded;
    assert(desktop::LoadProjectDocument(project_path, &loaded, &err));
    assert(err == "ok");
    assert(loaded.schema_version == 1);
    assert(loaded.sensor_mode == "bayer");
    assert(loaded.role_order == "BGR");
    assert(loaded.sort_mode == "selection");
    assert(loaded.active_group == 0);
    assert(loaded.files.size() == 3);
    assert(loaded.files[0].selection_index == 2);
    assert(loaded.files[1].selection_index == 1);
    assert(loaded.files[2].selection_index == 0);
    assert(fs::weakly_canonical(loaded.files[0].path, ec) ==
           fs::weakly_canonical(r, ec));
    assert(fs::weakly_canonical(loaded.files[1].path, ec) ==
           fs::weakly_canonical(g, ec));
    assert(fs::weakly_canonical(loaded.files[2].path, ec) ==
           fs::weakly_canonical(b, ec));

    fs::remove_all(root, ec);
}

}  // namespace

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName("SoftLoafTrichromeTest");
    QCoreApplication::setOrganizationName("SoftLoaf");

    TestObsLogSink();
    TestPreviewCacheIdentityAndHit();
    TestProjectStoreRoundTrip();
    return 0;
}
