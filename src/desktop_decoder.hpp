#pragma once

#include <filesystem>

#include "softloaf_trichrome/image_buf.hpp"

namespace softloaf::trichrome::desktop {

ImageBuf DecodeLinear(const std::filesystem::path& path, bool force_mono);
bool LooksLikeRaw(const std::filesystem::path& path);

}  // namespace softloaf::trichrome::desktop
