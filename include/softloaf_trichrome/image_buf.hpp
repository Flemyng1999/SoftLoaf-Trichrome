#pragma once

#include <array>
#include <string>

#include <opencv2/core.hpp>

namespace softloaf::trichrome {

enum class ColorState {
    kCameraLinear,
    kWorkingLinear,
    kDensity,
    kDisplay,
};

struct ImageBuf {
    cv::Mat data;
    ColorState state = ColorState::kCameraLinear;
    std::string color_space = "camera_native_linear";
    std::array<double, 9> camera_to_xyz_d50 = {};
    bool has_camera_to_xyz_d50 = false;

    [[nodiscard]] bool empty() const { return data.empty(); }
    [[nodiscard]] int width() const { return data.cols; }
    [[nodiscard]] int height() const { return data.rows; }
    [[nodiscard]] int channels() const { return data.channels(); }
    [[nodiscard]] bool is_mono() const { return data.channels() == 1; }
};

}  // namespace softloaf::trichrome
