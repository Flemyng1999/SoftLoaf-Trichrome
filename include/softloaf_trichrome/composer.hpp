#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <future>
#include <map>
#include <numeric>
#include <string>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

#include <opencv2/imgproc.hpp>

#include "softloaf_trichrome/artifact.hpp"
#include "softloaf_trichrome/image_buf.hpp"
#include "softloaf_trichrome/model.hpp"
#include "softloaf_trichrome/raw_classification.hpp"

namespace softloaf::trichrome {

inline constexpr const char* kDefaultArtifactFormat = "rgb16_npy";
inline constexpr const char* kTrichromeWorkingSpace = "linear_srgb_trichrome";
inline constexpr const char* kArtifactColorState = "linear_srgb_trichrome_roll_rgb_white_p999";

struct ComposeResult {
    bool ok = false;
    std::string reason = "compose_not_started";
    std::vector<std::string> warnings;
    std::vector<std::string> source_provenance_sigs;
    std::vector<ProjectTrichromeSource> source_provenance_records;
    ImageBuf rgb;
    ImageBuf nir;
    bool has_nir = false;
};

struct ArtifactBuildResult {
    bool ok = false;
    std::string reason = "artifact_not_started";
    std::vector<std::string> warnings;
    std::filesystem::path artifact_path;
    std::string artifact_sig;
};

using ImageDecoder = std::function<ImageBuf(const std::filesystem::path&)>;

inline ImageBuf ResizeLongEdge(const ImageBuf& image, int long_edge) {
    if (image.empty() || long_edge <= 0) return image;
    const int edge = std::max(image.width(), image.height());
    if (edge <= long_edge) return image;
    ImageBuf out = image;
    const double scale = static_cast<double>(long_edge) / static_cast<double>(edge);
    cv::resize(image.data, out.data, cv::Size(), scale, scale, cv::INTER_AREA);
    return out;
}

inline bool IsSceneLinearMono(const ImageBuf& image) {
    return !image.empty() &&
           (image.state == ColorState::kCameraLinear ||
            image.state == ColorState::kWorkingLinear) &&
           image.data.depth() == CV_32F && image.data.channels() == 1;
}

inline bool IsSceneLinearRgb(const ImageBuf& image) {
    return !image.empty() &&
           (image.state == ColorState::kCameraLinear ||
            image.state == ColorState::kWorkingLinear) &&
           image.data.depth() == CV_32F && image.data.channels() == 3;
}

inline bool IsCameraLinearMono(const ImageBuf& image) {
    return IsSceneLinearMono(image) && image.state == ColorState::kCameraLinear;
}

inline bool IsCameraLinearRgb(const ImageBuf& image) {
    return IsSceneLinearRgb(image) && image.state == ColorState::kCameraLinear;
}

inline bool IsBayerRoleRgb(const ImageBuf& image) {
    return !image.empty() &&
           (image.state == ColorState::kCameraLinear ||
            image.state == ColorState::kWorkingLinear) &&
           image.data.depth() == CV_32F && image.data.channels() == 3;
}

inline bool IsBayerRoleMono(const ImageBuf& image) {
    return !image.empty() &&
           (image.state == ColorState::kCameraLinear ||
            image.state == ColorState::kWorkingLinear) &&
           image.data.depth() == CV_32F && image.data.channels() == 1;
}

inline int ExpectedChannelForRole(const std::string& role) {
    if (role == "red") return 0;
    if (role == "green") return 1;
    if (role == "blue") return 2;
    return -1;
}

inline ImageBuf ExtractRgbChannel(const ImageBuf& image, int channel) {
    ImageBuf out;
    if (!IsBayerRoleRgb(image) || channel < 0 || channel >= 3) return out;
    cv::extractChannel(image.data, out.data, channel);
    out.state = image.state;
    out.color_space = image.color_space;
    out.camera_to_xyz_d50 = image.camera_to_xyz_d50;
    out.has_camera_to_xyz_d50 = image.has_camera_to_xyz_d50;
    out.raw_provenance = image.raw_provenance;
    return out;
}

inline ImageBuf AverageRgbChannels(const ImageBuf& image) {
    ImageBuf out;
    if (!IsBayerRoleRgb(image)) return out;
    out.data.create(image.height(), image.width(), CV_32FC1);
    for (int y = 0; y < image.height(); ++y) {
        const auto* src = image.data.ptr<cv::Vec3f>(y);
        float* dst = out.data.ptr<float>(y);
        for (int x = 0; x < image.width(); ++x)
            dst[x] = (src[x][0] + src[x][1] + src[x][2]) * (1.0f / 3.0f);
    }
    out.state = image.state;
    out.color_space = image.color_space;
    out.camera_to_xyz_d50 = image.camera_to_xyz_d50;
    out.has_camera_to_xyz_d50 = image.has_camera_to_xyz_d50;
    out.raw_provenance = image.raw_provenance;
    return out;
}

inline std::array<double, 3> ChannelMeansFinite(const ImageBuf& image) {
    std::array<double, 3> means = {0.0, 0.0, 0.0};
    if (!IsBayerRoleRgb(image)) return means;
    double sum[3] = {0.0, 0.0, 0.0};
    int count = 0;
    for (int y = 0; y < image.height(); ++y) {
        const auto* row = image.data.ptr<cv::Vec3f>(y);
        for (int x = 0; x < image.width(); ++x) {
            for (int c = 0; c < 3; ++c)
                if (std::isfinite(row[x][c])) sum[c] += row[x][c];
            ++count;
        }
    }
    for (int c = 0; c < 3; ++c) means[c] = sum[c] / std::max(1, count);
    return means;
}

inline bool CheckBayerRolesCrossFrame(const ImageBuf& red_frame,
                                      const ImageBuf& green_frame,
                                      const ImageBuf& blue_frame,
                                      std::vector<std::string>* warnings,
                                      std::string* reason) {
    const ImageBuf* frames[3] = {&red_frame, &green_frame, &blue_frame};
    for (const ImageBuf* frame : frames) {
        if (!IsBayerRoleRgb(*frame)) {
            if (reason) *reason = "compose_empty_or_non_rgb";
            return false;
        }
    }
    double means[3][3];
    for (int r = 0; r < 3; ++r) {
        const auto m = ChannelMeansFinite(*frames[r]);
        for (int c = 0; c < 3; ++c) means[r][c] = m[c];
    }
    for (int c = 0; c < 3; ++c) {
        const double expected = means[c][c];
        double best_other = 0.0;
        for (int r = 0; r < 3; ++r)
            if (r != c) best_other = std::max(best_other, means[r][c]);
        if (best_other > expected * 1.25 && best_other - expected > 0.02) {
            if (warnings) warnings->push_back("compose_bayer_wrong_order");
        } else if (expected <= best_other * 1.10 || expected - best_other < 0.01) {
            if (warnings) warnings->push_back("compose_bayer_ambiguous_response");
        }
    }
    if (reason) *reason = "ok";
    return true;
}

inline ComposeResult ComposeMonoRgb(const ImageBuf& red,
                                    const ImageBuf& green,
                                    const ImageBuf& blue) {
    ComposeResult result;
    if (!IsBayerRoleMono(red) || !IsBayerRoleMono(green) || !IsBayerRoleMono(blue)) {
        result.reason = "compose_empty_or_non_mono";
        return result;
    }
    if (red.width() != green.width() || red.width() != blue.width() ||
        red.height() != green.height() || red.height() != blue.height()) {
        result.reason = "compose_dimension_mismatch";
        return result;
    }
    result.rgb.data.create(red.height(), red.width(), CV_32FC3);
    const cv::Mat sources[] = {red.data, green.data, blue.data};
    const int from_to[] = {0, 0, 1, 1, 2, 2};
    cv::mixChannels(sources, 3, &result.rgb.data, 1, from_to, 3);
    result.rgb.state = ColorState::kWorkingLinear;
    result.rgb.color_space = kTrichromeWorkingSpace;
    result.ok = true;
    result.reason = "ok";
    return result;
}

inline double Percentile(std::vector<float>* values, double percentile) {
    if (!values || values->empty()) return 1.0;
    const double clamped = std::clamp(percentile, 0.0, 100.0);
    const size_t idx = static_cast<size_t>(
        std::round((clamped / 100.0) * static_cast<double>(values->size() - 1)));
    std::nth_element(values->begin(), values->begin() + static_cast<std::ptrdiff_t>(idx),
                     values->end());
    return std::max(1e-6, static_cast<double>((*values)[idx]));
}

using RgbChannelSamples = std::array<std::vector<float>, 3>;

inline void AppendRgbChannelWhiteSamples(const ImageBuf& rgb,
                                         RgbChannelSamples* samples,
                                         int estimate_long_edge = 1024) {
    if (!samples || !IsSceneLinearRgb(rgb)) return;
    cv::Mat estimate = rgb.data;
    if (estimate_long_edge > 0 && std::max(rgb.width(), rgb.height()) > estimate_long_edge) {
        const double scale = static_cast<double>(estimate_long_edge) /
                             static_cast<double>(std::max(rgb.width(), rgb.height()));
        cv::resize(rgb.data, estimate, cv::Size(), scale, scale, cv::INTER_AREA);
    }
    for (auto& channel : *samples)
        channel.reserve(channel.size() + static_cast<size_t>(estimate.total()));
    for (int y = 0; y < estimate.rows; ++y) {
        const auto* row = estimate.ptr<cv::Vec3f>(y);
        for (int x = 0; x < estimate.cols; ++x) {
            for (int c = 0; c < 3; ++c)
                if (std::isfinite(row[x][c])) (*samples)[static_cast<size_t>(c)].push_back(row[x][c]);
        }
    }
}

inline cv::Vec3d EstimateRgbChannelWhiteFromSamples(RgbChannelSamples samples,
                                                    double white_percentile = 99.9) {
    cv::Vec3d white(1.0, 1.0, 1.0);
    for (int c = 0; c < 3; ++c)
        white[c] = Percentile(&samples[static_cast<size_t>(c)], white_percentile);
    return white;
}

inline cv::Vec3d EstimateRgbChannelWhite(const ImageBuf& rgb,
                                         int estimate_long_edge = 1024,
                                         double white_percentile = 99.9) {
    RgbChannelSamples samples;
    AppendRgbChannelWhiteSamples(rgb, &samples, estimate_long_edge);
    return EstimateRgbChannelWhiteFromSamples(std::move(samples), white_percentile);
}

inline bool NormalizeRgbByChannelWhite(ImageBuf* rgb, const cv::Vec3d& white) {
    if (!rgb || !IsSceneLinearRgb(*rgb)) return false;
    const float inv[3] = {
        static_cast<float>(1.0 / std::max(1e-6, white[0])),
        static_cast<float>(1.0 / std::max(1e-6, white[1])),
        static_cast<float>(1.0 / std::max(1e-6, white[2])),
    };
    for (int y = 0; y < rgb->height(); ++y) {
        auto* row = rgb->data.ptr<cv::Vec3f>(y);
        for (int x = 0; x < rgb->width(); ++x)
            for (int c = 0; c < 3; ++c) row[x][c] *= inv[c];
    }
    return true;
}

inline void AppendInputPreprocessSig(ProjectTrichromeGroup* group,
                                     const std::string& sig) {
    if (!group || sig.empty()) return;
    if (group->input_preprocess_sig.empty()) {
        group->input_preprocess_sig = sig;
    } else if (group->input_preprocess_sig.find(sig) == std::string::npos) {
        group->input_preprocess_sig += "|";
        group->input_preprocess_sig += sig;
    }
}

inline std::string SourceRawProvenanceIdentity(
    const std::vector<std::string>& source_provenance_sigs) {
    if (source_provenance_sigs.empty()) return {};
    std::string identity = "raw-source-provenance-v1";
    for (const std::string& sig : source_provenance_sigs) {
        identity += "|";
        identity += sig;
    }
    return identity;
}

inline void SetSourceRawProvenance(ProjectTrichromeSource* source,
                                   const RawDecodeProvenance& provenance) {
    if (!source || !provenance.present) return;
    source->raw_class = RawSensorClassName(provenance.raw_class);
    source->raw_policy = RawLinearRec2020PolicyName(provenance.policy);
    source->raw_decode_mode = RawDecodeModeName(provenance.decode_mode);
    source->raw_fallback_status =
        RawDecodeFallbackStatusName(provenance.fallback_status);
    source->raw_target_color_space = provenance.target_color_space;
    source->raw_provenance_sig = RawDecodeProvenanceSignature(provenance);
}

inline void ApplySourceRawProvenance(
    ProjectTrichromeGroup* group,
    const std::vector<ProjectTrichromeSource>& provenance_records) {
    if (!group || provenance_records.empty()) return;
    for (ProjectTrichromeSource& source : group->sources) {
        for (const ProjectTrichromeSource& record : provenance_records) {
            if (source.role == record.role && source.path == record.path) {
                source.raw_class = record.raw_class;
                source.raw_policy = record.raw_policy;
                source.raw_decode_mode = record.raw_decode_mode;
                source.raw_fallback_status = record.raw_fallback_status;
                source.raw_target_color_space = record.raw_target_color_space;
                source.raw_provenance_sig = record.raw_provenance_sig;
                break;
            }
        }
    }
}

inline ComposeResult ComposeGroup(const ProjectTrichromeGroup& group,
                                  const ImageDecoder& decoder) {
    ComposeResult result;
    if (!decoder) {
        result.reason = "compose_decoder_missing";
        return result;
    }
    struct DecodedSource { std::string role; std::string path; ImageBuf image; };
    std::vector<std::future<DecodedSource>> futures;
    for (const ProjectTrichromeSource& source : group.sources) {
        futures.push_back(std::async(std::launch::async, [source, &decoder]() {
            return DecodedSource{source.role, source.path, decoder(source.path)};
        }));
    }
    std::vector<DecodedSource> decoded;
    try {
        for (auto& future : futures) decoded.push_back(future.get());
    } catch (...) {
        result.reason = "compose_decode_exception";
        return result;
    }
    std::vector<std::string> source_provenance_sigs;
    std::vector<ProjectTrichromeSource> source_provenance_records;
    for (const DecodedSource& source : decoded) {
        if (!source.image.raw_provenance.present) continue;
        source_provenance_sigs.push_back(
            source.role + "=" +
            RawDecodeProvenanceSignature(source.image.raw_provenance));
        ProjectTrichromeSource record;
        record.role = source.role;
        record.path = source.path;
        SetSourceRawProvenance(&record, source.image.raw_provenance);
        source_provenance_records.push_back(std::move(record));
    }
    const bool bayer = group.sensor_type == "bayer";
    if (bayer) {
        const ImageBuf* role_imgs[3] = {nullptr, nullptr, nullptr};
        for (const auto& ds : decoded) {
            const int ch = ExpectedChannelForRole(ds.role);
            if (ch >= 0 && ch < 3) role_imgs[ch] = &ds.image;
        }
        if (role_imgs[0] && role_imgs[1] && role_imgs[2]) {
            std::string cross_reason;
            if (!CheckBayerRolesCrossFrame(ResizeLongEdge(*role_imgs[0], 1024),
                                           ResizeLongEdge(*role_imgs[1], 1024),
                                           ResizeLongEdge(*role_imgs[2], 1024),
                                           &result.warnings, &cross_reason)) {
                result.reason = cross_reason;
                return result;
            }
        }
    }
    ImageBuf red, green, blue, nir;
    for (DecodedSource& source : decoded) {
        if (bayer && source.role != "nir")
            source.image = ExtractRgbChannel(source.image, ExpectedChannelForRole(source.role));
        else if (bayer && source.role == "nir")
            source.image = AverageRgbChannels(source.image);
        if (source.role == "red") red = std::move(source.image);
        else if (source.role == "green") green = std::move(source.image);
        else if (source.role == "blue") blue = std::move(source.image);
        else if (source.role == "nir") nir = std::move(source.image);
    }
    result = ComposeMonoRgb(red, green, blue);
    if (!result.ok) return result;
    result.source_provenance_sigs = std::move(source_provenance_sigs);
    result.source_provenance_records = std::move(source_provenance_records);
    if (!nir.empty()) {
        if (!IsSceneLinearMono(nir) ||
            nir.width() != result.rgb.width() || nir.height() != result.rgb.height()) {
            result.ok = false;
            result.reason = "compose_nir_mismatch";
            result.rgb = {};
            return result;
        }
        result.nir = std::move(nir);
        result.has_nir = true;
    }
    return result;
}

inline cv::Mat FloatRgbToUint16(const ImageBuf& rgb) {
    cv::Mat rgb16(rgb.height(), rgb.width(), CV_16UC3);
    for (int y = 0; y < rgb.height(); ++y) {
        const auto* src = rgb.data.ptr<cv::Vec3f>(y);
        auto* dst = rgb16.ptr<cv::Vec<uint16_t, 3>>(y);
        for (int x = 0; x < rgb.width(); ++x) {
            for (int c = 0; c < 3; ++c) {
                const float clamped = std::clamp(src[x][c], 0.0f, 1.0f);
                dst[x][c] = static_cast<uint16_t>(std::lround(clamped * 65535.0f));
            }
        }
    }
    return rgb16;
}

inline std::filesystem::path TempArtifactPath(const std::filesystem::path& artifact_path) {
    static std::atomic<uint64_t> counter{0};
    const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    std::filesystem::path filename = artifact_path.stem();
    filename += ".tmp.";
    filename += std::to_string(now);
    filename += ".";
    filename += std::to_string(counter.fetch_add(1));
    filename += artifact_path.extension();
    return artifact_path.parent_path() / filename;
}

inline std::string NpyHeaderV1(int height, int width, int channels) {
    std::ostringstream dict;
    dict << "{'descr': '<u2', 'fortran_order': False, 'shape': ("
         << height << ", " << width;
    if (channels != 1) dict << ", " << channels;
    dict << "), }";
    std::string header = dict.str();
    const size_t prefix_len = 10;
    size_t padded_len = header.size() + 1;
    while ((prefix_len + padded_len) % 16 != 0) ++padded_len;
    header.append(padded_len - header.size() - 1, ' ');
    header.push_back('\n');
    return header;
}

inline bool WriteRgb16NpyAtomic(const ImageBuf& rgb,
                                const std::filesystem::path& artifact_path,
                                std::string* reason) {
    if (!IsSceneLinearRgb(rgb)) {
        if (reason) *reason = "artifact_empty_or_non_rgb";
        return false;
    }
    std::error_code ec;
    std::filesystem::create_directories(artifact_path.parent_path(), ec);
    if (ec) {
        if (reason) *reason = "artifact_create_dir_failed";
        return false;
    }
    const cv::Mat rgb16 = FloatRgbToUint16(rgb);
    const std::filesystem::path tmp = TempArtifactPath(artifact_path);
    bool wrote = false;
    {
        std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
        if (f) {
            const std::string header = NpyHeaderV1(rgb16.rows, rgb16.cols, rgb16.channels());
            const uint16_t header_len = static_cast<uint16_t>(header.size());
            const unsigned char len[2] = {
                static_cast<unsigned char>(header_len & 0xff),
                static_cast<unsigned char>((header_len >> 8) & 0xff),
            };
            f.write("\x93NUMPY", 6);
            const unsigned char version[2] = {1, 0};
            f.write(reinterpret_cast<const char*>(version), 2);
            f.write(reinterpret_cast<const char*>(len), 2);
            f.write(header.data(), static_cast<std::streamsize>(header.size()));
            if (rgb16.isContinuous()) {
                f.write(reinterpret_cast<const char*>(rgb16.data),
                        static_cast<std::streamsize>(rgb16.total() * rgb16.elemSize()));
            } else {
                for (int y = 0; y < rgb16.rows; ++y) {
                    f.write(reinterpret_cast<const char*>(rgb16.ptr(y)),
                            static_cast<std::streamsize>(rgb16.cols * rgb16.elemSize()));
                }
            }
            wrote = static_cast<bool>(f);
        }
    }
    if (!wrote) {
        std::filesystem::remove(tmp, ec);
        if (reason) *reason = "artifact_imwrite_failed";
        return false;
    }
    std::filesystem::rename(tmp, artifact_path, ec);
    if (ec) {
        std::filesystem::remove(tmp, ec);
        if (reason) *reason = "artifact_rename_failed";
        return false;
    }
    if (reason) *reason = "ok";
    return true;
}

inline ProjectTrichromeGroup PrepareWorkingGroup(const ProjectTrichromeGroup& group) {
    ProjectTrichromeGroup working = group;
    RefreshSourceProbes(&working);
    if (working.artifact_format.empty()) working.artifact_format = kDefaultArtifactFormat;
    working.artifact_pixel_type = "uint16";
    working.artifact_channel_order = "RGB";
    working.artifact_color_state = kArtifactColorState;
    working.compose_algo_version = kComposeAlgoVersion;
    AppendInputPreprocessSig(&working, RawDecodePipelineIdentity());
    return working;
}

inline ArtifactBuildResult BuildTrichromePreviewArtifact(ProjectMeta* meta,
                                                         int group_index,
                                                         const std::filesystem::path& bundle,
                                                         ImageBuf preview_rgb_unnormalized) {
    ArtifactBuildResult result;
    if (!meta || group_index < 0 || group_index >= static_cast<int>(meta->trichrome_groups.size())) {
        result.reason = "artifact_group_missing";
        return result;
    }
    ProjectTrichromeGroup& group = meta->trichrome_groups[group_index];
    if (!meta->trichrome_roll_white_valid) {
        group.preview_artifact_valid = false;
        group.preview_artifact_dirty_reason = "artifact_roll_white_missing";
        result.reason = "artifact_roll_white_missing";
        return result;
    }
    ProjectTrichromeGroup working = PrepareWorkingGroup(group);
    working.preview_max_edge = kPreviewArtifactMaxEdge;
    if (working.preview_artifact_path.empty()) {
        working.preview_artifact_path =
            PathUtf8String(PreviewArtifactPathFor(working.artifact_path,
                                                  working.preview_max_edge));
    }
    NormalizeRgbByChannelWhite(&preview_rgb_unnormalized,
                               cv::Vec3d(meta->trichrome_roll_white[0],
                                         meta->trichrome_roll_white[1],
                                         meta->trichrome_roll_white[2]));
    working.preview_artifact_width = preview_rgb_unnormalized.width();
    working.preview_artifact_height = preview_rgb_unnormalized.height();
    const auto path = ResolveArtifactPath(bundle, working.preview_artifact_path);
    std::string write_reason;
    if (!WriteRgb16NpyAtomic(preview_rgb_unnormalized, path, &write_reason)) {
        group.preview_artifact_valid = false;
        group.preview_artifact_dirty_reason = write_reason;
        result.reason = write_reason;
        return result;
    }
    working.preview_artifact_sig =
        ComputeArtifactSignature(working, ArtifactTier::kPreview, meta->trichrome_roll_white);
    working.preview_artifact_valid = true;
    working.preview_artifact_dirty_reason.clear();
    group = std::move(working);
    if (group.logical_frame_index >= 0 &&
        group.logical_frame_index < static_cast<int>(meta->frames.size())) {
        meta->frames[group.logical_frame_index].path = group.preview_artifact_path;
    }
    result.ok = true;
    result.reason = "ok";
    result.artifact_path = path;
    result.artifact_sig = group.preview_artifact_sig;
    return result;
}

inline ArtifactBuildResult BuildTrichromeFullArtifact(ProjectMeta* meta,
                                                      int group_index,
                                                      const std::filesystem::path& bundle,
                                                      const ImageDecoder& decoder) {
    ArtifactBuildResult result;
    if (!meta || group_index < 0 || group_index >= static_cast<int>(meta->trichrome_groups.size())) {
        result.reason = "artifact_group_missing";
        return result;
    }
    ProjectTrichromeGroup& group = meta->trichrome_groups[group_index];
    if (!meta->trichrome_roll_white_valid) {
        group.artifact_valid = false;
        group.artifact_dirty_reason = "artifact_roll_white_missing";
        result.reason = "artifact_roll_white_missing";
        return result;
    }
    ProjectTrichromeGroup working = PrepareWorkingGroup(group);
    const auto path = ResolveArtifactPath(bundle, working.artifact_path);
    const ArtifactGuardResult guard = EvaluateArtifactReadiness(
        working, ArtifactTier::kFull, meta->trichrome_roll_white, StatFile(path));
    if (guard.ok()) {
        result.ok = true;
        result.reason = "ok";
        result.artifact_path = path;
        result.artifact_sig = working.artifact_sig;
        return result;
    }
    ComposeResult composed = ComposeGroup(working, decoder);
    result.warnings = composed.warnings;
    if (!composed.ok) {
        group.artifact_valid = false;
        group.artifact_dirty_reason = composed.reason;
        result.reason = composed.reason;
        return result;
    }
    AppendInputPreprocessSig(&working,
                             SourceRawProvenanceIdentity(
                                 composed.source_provenance_sigs));
    ApplySourceRawProvenance(&working, composed.source_provenance_records);
    ImageBuf artifact_rgb = composed.rgb;
    NormalizeRgbByChannelWhite(&artifact_rgb,
                               cv::Vec3d(meta->trichrome_roll_white[0],
                                         meta->trichrome_roll_white[1],
                                         meta->trichrome_roll_white[2]));
    working.artifact_width = artifact_rgb.width();
    working.artifact_height = artifact_rgb.height();
    std::string write_reason;
    if (!WriteRgb16NpyAtomic(artifact_rgb, path, &write_reason)) {
        group.artifact_valid = false;
        group.artifact_dirty_reason = write_reason;
        result.reason = write_reason;
        return result;
    }
    working.artifact_sig =
        ComputeArtifactSignature(working, ArtifactTier::kFull, meta->trichrome_roll_white);
    working.artifact_valid = true;
    working.artifact_dirty_reason.clear();
    group = std::move(working);
    result.ok = true;
    result.reason = "ok";
    result.artifact_path = path;
    result.artifact_sig = group.artifact_sig;
    return result;
}

}  // namespace softloaf::trichrome
