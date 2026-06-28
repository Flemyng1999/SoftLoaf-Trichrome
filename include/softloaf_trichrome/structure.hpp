#pragma once

#include <algorithm>
#include <cmath>
#include <functional>
#include <string>

#include <opencv2/imgproc.hpp>

#include "softloaf_trichrome/image_buf.hpp"
#include "softloaf_trichrome/model.hpp"

namespace softloaf::trichrome {

struct StructurePairReport {
    double score = 0.0;
    int dx = 0;
    int dy = 0;
};

struct StructureReport {
    bool ok = false;
    std::string reason = "structure_not_started";
    StructurePairReport rg;
    StructurePairReport gb;
    StructurePairReport rb;
};

struct StructureOptions {
    int max_side = 256;
    int max_shift = 4;
    double pass_score = 0.72;
};

using StructureImageDecoder = std::function<ImageBuf(const std::string&)>;

inline cv::Mat ToStructureMap(const ImageBuf& image, int max_side = 256) {
    if (image.empty() || image.data.depth() != CV_32F) return {};
    cv::Mat gray;
    if (image.data.channels() == 1) {
        gray = image.data;
    } else if (image.data.channels() == 3) {
        cv::cvtColor(image.data, gray, cv::COLOR_RGB2GRAY);
    } else {
        return {};
    }
    cv::Mat resized = gray;
    if (max_side > 0 && std::max(gray.cols, gray.rows) > max_side) {
        const double scale = static_cast<double>(max_side) /
                             static_cast<double>(std::max(gray.cols, gray.rows));
        cv::resize(gray, resized, {}, scale, scale, cv::INTER_AREA);
    }
    cv::Scalar mean, stddev;
    cv::meanStdDev(resized, mean, stddev);
    cv::Mat normalized = stddev[0] > 1e-6 ? (resized - mean[0]) / stddev[0]
                                          : resized - mean[0];
    cv::Mat gx, gy, mag;
    cv::Sobel(normalized, gx, CV_32F, 1, 0, 3);
    cv::Sobel(normalized, gy, CV_32F, 0, 1, 3);
    cv::magnitude(gx, gy, mag);
    cv::meanStdDev(mag, mean, stddev);
    return stddev[0] > 1e-6 ? (mag - mean[0]) / stddev[0] : mag - mean[0];
}

inline double ShiftedCorrelation(const cv::Mat& a, const cv::Mat& b, int dx, int dy) {
    if (a.empty() || b.empty() || a.size() != b.size() ||
        a.type() != CV_32FC1 || b.type() != CV_32FC1) {
        return -1.0;
    }
    const int x0 = std::max(0, -dx);
    const int y0 = std::max(0, -dy);
    const int x1 = std::min(a.cols, b.cols - dx);
    const int y1 = std::min(a.rows, b.rows - dy);
    const int w = x1 - x0;
    const int h = y1 - y0;
    if (w < 4 || h < 4) return -1.0;
    const cv::Mat ar = a(cv::Rect(x0, y0, w, h));
    const cv::Mat br = b(cv::Rect(x0 + dx, y0 + dy, w, h));
    cv::Scalar mean_a, std_a, mean_b, std_b;
    cv::meanStdDev(ar, mean_a, std_a);
    cv::meanStdDev(br, mean_b, std_b);
    if (std_a[0] <= 1e-6 || std_b[0] <= 1e-6) return -1.0;
    const double denom = std_a[0] * std_b[0] * static_cast<double>(w * h);
    return cv::sum((ar - mean_a[0]).mul(br - mean_b[0]))[0] / denom;
}

inline StructurePairReport EstimateStructureShift(const cv::Mat& a,
                                                  const cv::Mat& b,
                                                  int max_shift = 4) {
    StructurePairReport best;
    best.score = -1.0;
    for (int dy = -max_shift; dy <= max_shift; ++dy) {
        for (int dx = -max_shift; dx <= max_shift; ++dx) {
            const double score = ShiftedCorrelation(a, b, dx, dy);
            if (score > best.score) best = {score, dx, dy};
        }
    }
    best.score = std::max(0.0, best.score);
    return best;
}

inline StructureReport VerifyTrichromeStructure(const ProjectTrichromeGroup& group,
                                                const StructureImageDecoder& decoder,
                                                const StructureOptions& options = {}) {
    StructureReport report;
    if (!decoder) {
        report.reason = "structure_decoder_missing";
        return report;
    }
    cv::Mat red, green, blue;
    cv::Size red_size, green_size, blue_size;
    for (const ProjectTrichromeSource& source : group.sources) {
        if (source.role != "red" && source.role != "green" && source.role != "blue") continue;
        const ImageBuf image = decoder(source.path);
        if (image.empty() || image.data.depth() != CV_32F ||
            (image.data.channels() != 1 && image.data.channels() != 3)) {
            report.reason = "structure_empty_or_invalid";
            return report;
        }
        cv::Mat structure = ToStructureMap(image, options.max_side);
        if (structure.empty()) {
            report.reason = "structure_empty_or_invalid";
            return report;
        }
        if (source.role == "red") {
            red_size = image.data.size();
            red = std::move(structure);
        } else if (source.role == "green") {
            green_size = image.data.size();
            green = std::move(structure);
        } else {
            blue_size = image.data.size();
            blue = std::move(structure);
        }
    }
    if (red.empty() || green.empty() || blue.empty()) {
        report.reason = "structure_missing_role";
        return report;
    }
    if (red_size != green_size || red_size != blue_size ||
        red.size() != green.size() || red.size() != blue.size()) {
        report.reason = "structure_dimension_mismatch";
        return report;
    }
    report.rg = EstimateStructureShift(red, green, options.max_shift);
    report.gb = EstimateStructureShift(green, blue, options.max_shift);
    report.rb = EstimateStructureShift(red, blue, options.max_shift);
    report.ok = report.rg.score >= options.pass_score &&
                report.gb.score >= options.pass_score &&
                report.rb.score >= options.pass_score;
    report.reason = report.ok ? "ok" : "structure_mismatch";
    return report;
}

}  // namespace softloaf::trichrome
