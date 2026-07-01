#pragma once

#include <filesystem>
#include <string>

#include "softloaf_trichrome/image_buf.hpp"

namespace softloaf::trichrome::desktop {

enum class DecodeMode {
    // Preview RAW decoding is pinned to LibRaw half-size/no-interpolation mode.
    kPreview,
    // Export RAW decoding is always full-resolution.
    kExport,
};

struct RawProvenanceProbeResult {
    bool ok = false;
    std::string reason = "probe_not_started";
    RawDecodeProvenance provenance;
};

ImageBuf DecodeLinear(const std::filesystem::path& path,
                      bool force_mono,
                      DecodeMode decode_mode);
ImageBuf DecodeRawToLinearRec2020(const std::filesystem::path& path,
                                  DecodeMode decode_mode,
                                  bool use_camera_wb = true);
RawProvenanceProbeResult ProbeRawProvenance(const std::filesystem::path& path,
                                            DecodeMode decode_mode,
                                            RawDecodeTarget target);
bool LooksLikeRaw(const std::filesystem::path& path);

}  // namespace softloaf::trichrome::desktop
