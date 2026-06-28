#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include <QImage>

#include "softloaf_trichrome/model.hpp"

namespace softloaf::trichrome::desktop {

struct TrichromeCacheInput {
    std::vector<std::filesystem::path> paths;
    std::string sensor_mode;
    std::string role_order;
    int schema_version = 1;
};

struct CacheLookupResult {
    bool hit = false;
    std::string key;
    std::filesystem::path path;
    std::string reason = "miss";
    QImage image;
};

class TrichromePreviewCache {
 public:
    [[nodiscard]] CacheLookupResult lookup(const TrichromeCacheInput& input) const;
    [[nodiscard]] bool write(const std::string& key, const QImage& image,
                             std::filesystem::path* path_out,
                             std::string* reason_out) const;
    [[nodiscard]] std::string keyFor(const TrichromeCacheInput& input) const;
    [[nodiscard]] std::filesystem::path pathForKey(const std::string& key) const;
};

}  // namespace softloaf::trichrome::desktop
