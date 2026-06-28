#pragma once

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

    [[nodiscard]] bool empty() const { return data.empty(); }
    [[nodiscard]] int width() const { return data.cols; }
    [[nodiscard]] int height() const { return data.rows; }
    [[nodiscard]] int channels() const { return data.channels(); }
    [[nodiscard]] bool is_mono() const { return data.channels() == 1; }
};

}  // namespace softloaf::trichrome
