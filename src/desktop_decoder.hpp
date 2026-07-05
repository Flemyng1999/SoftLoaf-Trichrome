#pragma once

#include <array>
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
    std::string sensor_hints = "none";
    std::string make;
    std::string model;
    std::string matrix_source = "unknown";
    unsigned filters = 0;
    int colors = 0;
    bool has_raw_image = false;
    bool has_color3_image = false;
    bool has_color4_image = false;
    bool has_float3_image = false;
    bool has_float4_image = false;
    unsigned black = 0;
    std::array<unsigned, 4> cblack = {};
    std::array<unsigned, 4> linear_max = {};
    unsigned maximum = 0;
    unsigned data_maximum = 0;
    int raw_width = 0;
    int raw_height = 0;
    int visible_width = 0;
    int visible_height = 0;
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
