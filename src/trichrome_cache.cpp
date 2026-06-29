#include "trichrome_cache.hpp"

#include <chrono>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <system_error>

#include <QDir>
#include <QImageWriter>
#include <QStandardPaths>
#include <QString>

#include "obs_log.hpp"
#include "qt_path_utils.hpp"
#include "softloaf_trichrome/cache_probe.hpp"

namespace softloaf::trichrome::desktop {
namespace {

constexpr const char* kPreviewCacheExtension = ".jpg";
constexpr const char* kPreviewCacheFormat = "JPEG";
constexpr int kPreviewCacheJpegQuality = 92;

void HashString(uint64_t* h, const std::string& value) {
    for (unsigned char c : value) detail::HashByte(h, c);
    detail::HashByte(h, 0xff);
}

void HashUint64(uint64_t* h, uint64_t value) {
    for (int i = 0; i < 8; ++i)
        detail::HashByte(h, static_cast<unsigned char>((value >> (i * 8)) & 0xff));
}

std::string HexDigest(uint64_t h) {
    std::ostringstream out;
    out << std::hex << std::setw(16) << std::setfill('0') << h;
    return out.str();
}

std::filesystem::path CacheRoot() {
    QString root = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    if (root.isEmpty()) root = QDir::tempPath() + "/SoftLoafTrichromeCache";
    return PathFromQString(root) / "preview";
}

std::filesystem::path TempPathFor(const std::filesystem::path& path) {
    const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    return path.parent_path() / (path.stem().string() + ".tmp." +
                                 std::to_string(now) + path.extension().string());
}

}  // namespace

std::string TrichromePreviewCache::keyFor(const TrichromeCacheInput& input) const {
    uint64_t h = detail::kFnvOffset;
    HashString(&h, "softloaf.trichrome.preview_cache.v6");
    HashUint64(&h, static_cast<uint64_t>(input.schema_version));
    HashString(&h, input.sensor_mode);
    HashString(&h, input.role_order);
    HashString(&h, input.decode_mode);
    HashUint64(&h, static_cast<uint64_t>(std::max(0, input.max_edge)));
    HashUint64(&h, static_cast<uint64_t>(input.paths.size()));
    for (const auto& path : input.paths) {
        const FileProbe probe = ProbeFileWithPartialHash(path);
        HashString(&h, QStringFromPath(path).toUtf8().toStdString());
        detail::HashByte(&h, probe.ok ? 1 : 0);
        HashUint64(&h, probe.size);
        HashUint64(&h, static_cast<uint64_t>(probe.mtime));
        HashUint64(&h, probe.partial_hash);
    }
    return HexDigest(h);
}

std::filesystem::path TrichromePreviewCache::pathForKey(const std::string& key) const {
    return CacheRoot() / (key + kPreviewCacheExtension);
}

CacheLookupResult TrichromePreviewCache::lookup(const TrichromeCacheInput& input) const {
    CacheLookupResult result;
    result.key = keyFor(input);
    result.path = pathForKey(result.key);
    if (!std::filesystem::exists(result.path)) {
        result.reason = "missing";
        ObsLog("cache.lookup", {{"category", "trichrome_preview"},
                                {"result", "miss"},
                                {"reason", "missing"},
                                {"key", result.key}});
        return result;
    }
    QImage image(QStringFromPath(result.path));
    if (image.isNull()) {
        result.reason = "read_error";
        ObsLog("cache.lookup", {{"category", "trichrome_preview"},
                                {"result", "miss"},
                                {"reason", "read_error"},
                                {"key", result.key}});
        return result;
    }
    result.hit = true;
    result.reason = "hit";
    result.image = image;
    ObsLog("cache.lookup", {{"category", "trichrome_preview"},
                            {"result", "hit"},
                            {"reason", "ok"},
                            {"key", result.key}});
    return result;
}

bool TrichromePreviewCache::write(const std::string& key, const QImage& image,
                                  std::filesystem::path* path_out,
                                  std::string* reason_out) const {
    if (image.isNull()) {
        if (reason_out) *reason_out = "empty_image";
        return false;
    }
    const std::filesystem::path path = pathForKey(key);
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    if (ec) {
        if (reason_out) *reason_out = "create_dir_failed";
        return false;
    }
    const std::filesystem::path tmp = TempPathFor(path);
    QImageWriter writer(QStringFromPath(tmp), kPreviewCacheFormat);
    writer.setQuality(kPreviewCacheJpegQuality);
    if (!writer.write(image)) {
        if (reason_out) *reason_out = "write_failed";
        ObsLog("cache.write", {{"category", "trichrome_preview"},
                               {"result", "fail"},
                               {"reason", writer.errorString().toStdString()},
                               {"key", key}});
        return false;
    }
    std::filesystem::rename(tmp, path, ec);
    if (ec) {
        std::filesystem::remove(tmp, ec);
        if (reason_out) *reason_out = "rename_failed";
        ObsLog("cache.write", {{"category", "trichrome_preview"},
                               {"result", "fail"},
                               {"reason", "rename_failed"},
                               {"key", key}});
        return false;
    }
    if (path_out) *path_out = path;
    if (reason_out) *reason_out = "ok";
    ObsLog("cache.write", {{"category", "trichrome_preview"},
                           {"result", "success"},
                           {"reason", "ok"},
                           {"format", "jpeg"},
                           {"key", key}});
    return true;
}

}  // namespace softloaf::trichrome::desktop
