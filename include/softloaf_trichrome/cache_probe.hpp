#pragma once

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>

namespace softloaf::trichrome {

struct FileProbe {
    bool ok = false;
    uint64_t size = 0;
    int64_t mtime = 0;
    uint64_t partial_hash = 0;
};

namespace detail {
inline constexpr uint64_t kFnvOffset = 1469598103934665603ULL;
inline constexpr uint64_t kFnvPrime = 1099511628211ULL;

inline void HashByte(uint64_t* h, unsigned char byte) {
    *h ^= byte;
    *h *= kFnvPrime;
}

inline void HashBytes(uint64_t* h, const char* data, std::streamsize size) {
    for (std::streamsize i = 0; i < size; ++i)
        HashByte(h, static_cast<unsigned char>(data[i]));
}
}  // namespace detail

inline FileProbe StatFile(const std::string& path) {
    FileProbe probe;
    std::error_code ec;
    const std::filesystem::path p(path);
    if (!std::filesystem::is_regular_file(p, ec)) return probe;
    probe.size = std::filesystem::file_size(p, ec);
    if (ec) return {};
    const auto t = std::filesystem::last_write_time(p, ec);
    if (ec) return {};
    probe.mtime = std::chrono::duration_cast<std::chrono::seconds>(
        t.time_since_epoch()).count();
    probe.ok = true;
    return probe;
}

inline uint64_t PartialContentHash(const std::string& path, uint64_t size) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return 0;
    uint64_t h = detail::kFnvOffset;
    detail::HashBytes(&h, reinterpret_cast<const char*>(&size), sizeof(size));
    constexpr uint64_t kWindow = 64ull << 10;
    auto hash_window = [&](uint64_t offset, uint64_t count) {
        f.clear();
        f.seekg(static_cast<std::streamoff>(offset), std::ios::beg);
        char buf[8192];
        uint64_t remaining = count;
        while (remaining > 0 && f) {
            const auto want = static_cast<std::streamsize>(
                std::min<uint64_t>(remaining, sizeof(buf)));
            f.read(buf, want);
            const auto got = f.gcount();
            if (got <= 0) break;
            detail::HashBytes(&h, buf, got);
            remaining -= static_cast<uint64_t>(got);
        }
        detail::HashByte(&h, 0xff);
    };
    if (size <= kWindow * 3) {
        hash_window(0, size);
    } else {
        hash_window(0, kWindow);
        hash_window((size - kWindow) / 2, kWindow);
        hash_window(size - kWindow, kWindow);
    }
    return h;
}

inline FileProbe ProbeFileWithPartialHash(const std::filesystem::path& path) {
    FileProbe probe = StatFile(path.string());
    if (probe.ok) probe.partial_hash = PartialContentHash(path.string(), probe.size);
    return probe;
}

}  // namespace softloaf::trichrome
