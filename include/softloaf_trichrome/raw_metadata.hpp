#pragma once

#include <algorithm>
#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "softloaf_trichrome/model.hpp"

namespace softloaf::trichrome {

struct RawImageMetadata {
    bool ok = false;
    std::optional<double> shutter_seconds;
    std::optional<int> iso;
    std::optional<double> aperture;
    std::string make;
    std::string model;
    std::optional<int> width;
    std::optional<int> height;
    std::optional<int> raw_width;
    std::optional<int> raw_height;
    std::optional<int> bit_depth;
    std::optional<std::string> raw_type;
    std::optional<double> black_level;
    std::optional<double> white_level;
    std::optional<int64_t> capture_timestamp;
};

inline RawImageMetadata ProbeRawImageMetadata(const std::filesystem::path&) {
    return {};
}

inline std::string DetectDominantRawSuffix(const std::filesystem::path& folder) {
    std::unordered_map<std::string, int> counts;
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(folder, ec)) {
        if (!entry.is_regular_file(ec)) continue;
        const std::string ext = LowerExtension(entry.path());
        if (IsImportableRawPath(entry.path())) ++counts[ext];
    }
    std::string best;
    int best_count = 0;
    for (const auto& [ext, count] : counts) {
        if (count > best_count || (count == best_count && ext < best)) {
            best = ext;
            best_count = count;
        }
    }
    return best;
}

inline std::vector<std::filesystem::path> CollectRollFrames(
    const std::filesystem::path& folder,
    const std::string& suffix) {
    std::vector<std::filesystem::path> files;
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(folder, ec)) {
        if (!entry.is_regular_file(ec)) continue;
        if (LowerExtension(entry.path()) == suffix && IsImportableRawPath(entry.path()))
            files.push_back(entry.path());
    }
    std::sort(files.begin(), files.end());
    return files;
}

}  // namespace softloaf::trichrome
