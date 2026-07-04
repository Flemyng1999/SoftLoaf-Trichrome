#pragma once

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <optional>
#include <string>
#include <string_view>

#include "softloaf_trichrome/color_management.hpp"

namespace softloaf::trichrome {

inline std::string NormalizeRawCameraName(std::string_view value) {
    std::string out;
    bool pending_space = false;
    for (unsigned char c : value) {
        if (c == '\0') break;
        if (std::isalnum(c)) {
            if (pending_space && !out.empty()) out.push_back(' ');
            out.push_back(static_cast<char>(std::tolower(c)));
            pending_space = false;
        } else {
            pending_space = true;
        }
    }
    return out;
}

inline bool IsLeicaSl2(std::string_view make, std::string_view model) {
    const std::string normalized_make = NormalizeRawCameraName(make);
    const std::string normalized_model = NormalizeRawCameraName(model);
    if (normalized_model == "leica sl2") return true;
    return normalized_make.find("leica") != std::string::npos &&
           normalized_model == "sl2";
}

inline std::optional<color::Mat3> XyzD50FromDcrawMatrix(
    const std::array<double, 9>& dcraw_matrix) {
    color::Mat3 camera_from_xyz = {};
    for (size_t i = 0; i < camera_from_xyz.size(); ++i)
        camera_from_xyz[i] = dcraw_matrix[i] / 10000.0;

    color::Mat3 camera_from_srgb =
        color::Mul(camera_from_xyz, color::kRtLegacyXyzD65FromSrgbD65);
    for (int r = 0; r < 3; ++r) {
        const double row_sum = camera_from_srgb[r * 3 + 0] +
                               camera_from_srgb[r * 3 + 1] +
                               camera_from_srgb[r * 3 + 2];
        if (!std::isfinite(row_sum) || std::abs(row_sum) < 1e-12)
            return std::nullopt;
        for (int c = 0; c < 3; ++c) camera_from_srgb[r * 3 + c] /= row_sum;
    }

    const color::Mat3 srgb_from_camera = color::Invert(camera_from_srgb);
    return color::Mul(color::kXyzD50FromSrgbD50, srgb_from_camera);
}

inline std::optional<color::Mat3> LeicaSl2CameraToXyzD50Override(
    std::string_view make,
    std::string_view model) {
    if (!IsLeicaSl2(make, model)) return std::nullopt;
    constexpr std::array<double, 9> kLeicaSl2DcrawMatrix{
        12312.0, -5440.0, -1307.0,
        -6408.0, 15499.0, 824.0,
        -1075.0, 1676.0, 7220.0,
    };
    return XyzD50FromDcrawMatrix(kLeicaSl2DcrawMatrix);
}

}  // namespace softloaf::trichrome
