#pragma once

#include <filesystem>

#include "softloaf_trichrome/image_buf.hpp"

namespace softloaf::trichrome::desktop {

enum class DecodeMode {
    // Preview RAW decoding is pinned to LibRaw half-size/no-interpolation mode.
    kPreview,
    // Export RAW decoding is always full-resolution.
    kExport,
};

ImageBuf DecodeLinear(const std::filesystem::path& path,
                      bool force_mono,
                      DecodeMode decode_mode);
bool LooksLikeRaw(const std::filesystem::path& path);

}  // namespace softloaf::trichrome::desktop
