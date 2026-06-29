#pragma once

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <functional>
#include <iomanip>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "softloaf_trichrome/model.hpp"
#include "softloaf_trichrome/raw_metadata.hpp"

namespace softloaf::trichrome {

enum class TrichromeSortPolicy { kSelectionOrder, kFilename };
enum class TrichromeMetadataPolicy { kSkip, kStrict };
enum class TrichromeStructurePolicy { kSkip, kCheck };

struct TrichromeImportRequest {
    std::string name;
    std::filesystem::path folder;
    std::vector<std::filesystem::path> files;
    InputMode mode = InputMode::kTrichromeRgb;
    InputSensorType sensor_type = InputSensorType::kBayer;
    TrichromeSortPolicy sort_policy = TrichromeSortPolicy::kFilename;
    TrichromeMetadataPolicy metadata_policy = TrichromeMetadataPolicy::kSkip;
    TrichromeStructurePolicy structure_policy = TrichromeStructurePolicy::kSkip;
    std::filesystem::path artifact_dir = "trichrome";
    std::string artifact_format = "rgb16_npy";
};

struct TrichromeImportSourceReport {
    std::string role;
    std::filesystem::path path;
};

struct TrichromeImportGroupReport {
    int group_index = 0;
    int logical_frame_index = -1;
    std::string status = "skipped";
    std::string reason = "incomplete_group";
    std::filesystem::path artifact_path;
    std::vector<TrichromeImportSourceReport> sources;
    std::vector<std::string> warnings;
    std::string structure_reason = "structure_not_started";
    double structure_score_rg = 0.0;
    double structure_score_gb = 0.0;
    double structure_score_rb = 0.0;
    int estimated_shift_rg_dx = 0;
    int estimated_shift_rg_dy = 0;
    int estimated_shift_gb_dx = 0;
    int estimated_shift_gb_dy = 0;
    int estimated_shift_rb_dx = 0;
    int estimated_shift_rb_dy = 0;
};

struct TrichromeStructureDiagnostic {
    std::string reason = "structure_not_started";
    double score_rg = 0.0;
    double score_gb = 0.0;
    double score_rb = 0.0;
    int shift_rg_dx = 0;
    int shift_rg_dy = 0;
    int shift_gb_dx = 0;
    int shift_gb_dy = 0;
    int shift_rb_dx = 0;
    int shift_rb_dy = 0;
};

inline void ApplyStructureDiagnosticToImportGroup(
    const TrichromeStructureDiagnostic& structure,
    TrichromeImportGroupReport* group) {
    if (!group) return;
    group->structure_reason = structure.reason;
    group->structure_score_rg = structure.score_rg;
    group->structure_score_gb = structure.score_gb;
    group->structure_score_rb = structure.score_rb;
    group->estimated_shift_rg_dx = structure.shift_rg_dx;
    group->estimated_shift_rg_dy = structure.shift_rg_dy;
    group->estimated_shift_gb_dx = structure.shift_gb_dx;
    group->estimated_shift_gb_dy = structure.shift_gb_dy;
    group->estimated_shift_rb_dx = structure.shift_rb_dx;
    group->estimated_shift_rb_dy = structure.shift_rb_dy;
}

struct TrichromeImportReport {
    std::string mode = "trichrome_rgb";
    std::string sensor_type = "bayer";
    std::string strict_order = "RGB";
    std::string sort_policy = "filename";
    int group_size = 3;
    int file_count = 0;
    int valid_group_count = 0;
    int incomplete_group_count = 0;
    int skipped_group_count = 0;
    std::vector<TrichromeImportGroupReport> groups;

    [[nodiscard]] std::string ToStableJson() const;
};

struct TrichromeImportPlan {
    TrichromeImportReport report;
    InputRecipe recipe;

    [[nodiscard]] bool ok() const { return report.valid_group_count > 0; }
};

namespace import_detail {

inline std::string JsonEscape(const std::string& value) {
    std::ostringstream out;
    for (const char c : value) {
        switch (c) {
            case '\\': out << "\\\\"; break;
            case '"': out << "\\\""; break;
            case '\n': out << "\\n"; break;
            case '\r': out << "\\r"; break;
            case '\t': out << "\\t"; break;
            default: out << c; break;
        }
    }
    return out.str();
}

inline std::string JsonString(const std::string& value) {
    return "\"" + JsonEscape(value) + "\"";
}

inline std::string SortPolicyName(TrichromeSortPolicy policy) {
    return policy == TrichromeSortPolicy::kSelectionOrder ? "selection_order" : "filename";
}

inline bool IsTrichromeMode(InputMode mode) {
    return mode == InputMode::kTrichromeRgb || mode == InputMode::kTrichromeRgbNir;
}

inline InputGroupRole RoleForOffset(InputMode mode, int offset) {
    if (offset == 0) return InputGroupRole::kTrichromeRed;
    if (offset == 1) return InputGroupRole::kTrichromeGreen;
    if (offset == 2) return InputGroupRole::kTrichromeBlue;
    return mode == InputMode::kTrichromeRgbNir
        ? InputGroupRole::kTrichromeNir
        : InputGroupRole::kPrimary;
}

inline std::filesystem::path ArtifactPath(const std::filesystem::path& dir,
                                          int logical_frame_index,
                                          const std::string& format) {
    std::ostringstream name;
    name << "frame_" << std::setw(4) << std::setfill('0') << logical_frame_index
         << (format == "rgb16_npy" ? ".npy" : ".dat");
    return dir / name.str();
}

inline void SortAndDeduplicate(std::vector<std::filesystem::path>* files,
                               TrichromeSortPolicy policy) {
    if (policy == TrichromeSortPolicy::kFilename) std::sort(files->begin(), files->end());
    std::vector<std::filesystem::path> ordered;
    ordered.reserve(files->size());
    for (const auto& file : *files) {
        if (std::find(ordered.begin(), ordered.end(), file) == ordered.end())
            ordered.push_back(file);
    }
    *files = std::move(ordered);
}

template <typename T>
inline std::optional<std::string> CompareRequiredOptional(
    const std::optional<T>& a,
    const std::optional<T>& b,
    const std::string& missing_reason,
    const std::string& mismatch_reason) {
    if (!a.has_value() || !b.has_value()) return missing_reason;
    if (*a != *b) return mismatch_reason;
    return std::nullopt;
}

inline bool OptionalDoubleNear(const std::optional<double>& a,
                               const std::optional<double>& b,
                               double relative_tolerance) {
    if (!a.has_value() || !b.has_value()) return false;
    const double scale = std::max(std::max(std::abs(*a), std::abs(*b)), 1e-12);
    return std::abs(*a - *b) <= scale * relative_tolerance;
}

inline void PushUniqueWarning(std::vector<std::string>* warnings, const std::string& reason) {
    if (!warnings) return;
    if (std::find(warnings->begin(), warnings->end(), reason) == warnings->end())
        warnings->push_back(reason);
}

inline std::string ValidateMetadataGroup(const std::vector<RawImageMetadata>& metadata,
                                         std::vector<std::string>* warnings) {
    if (metadata.empty()) return "metadata_missing";
    for (const RawImageMetadata& item : metadata) {
        if (!item.ok) return "metadata_probe_failed";
    }
    const RawImageMetadata& ref = metadata.front();
    for (size_t i = 1; i < metadata.size(); ++i) {
        const RawImageMetadata& item = metadata[i];
        if (!OptionalDoubleNear(ref.shutter_seconds, item.shutter_seconds, 0.001))
            return "metadata_shutter_mismatch";
        if (auto reason = CompareRequiredOptional(
                ref.iso, item.iso, "metadata_iso_missing", "metadata_iso_mismatch")) {
            return *reason;
        }
        if (ref.aperture.has_value() != item.aperture.has_value()) {
            PushUniqueWarning(warnings, "metadata_aperture_missing");
        }
        if (ref.make.empty() || item.make.empty() ||
            ref.model.empty() || item.model.empty()) {
            return "metadata_camera_model_missing";
        }
        if (ref.make != item.make || ref.model != item.model)
            return "metadata_camera_model_mismatch";
    }
    return "ok";
}

}  // namespace import_detail

inline std::string TrichromeImportReport::ToStableJson() const {
    std::ostringstream out;
    out << "{";
    out << "\"fileCount\":" << file_count << ",";
    out << "\"groupSize\":" << group_size << ",";
    out << "\"groups\":[";
    for (size_t i = 0; i < groups.size(); ++i) {
        const TrichromeImportGroupReport& group = groups[i];
        if (i > 0) out << ",";
        out << "{";
        out << "\"artifactPath\":"
            << import_detail::JsonString(PathGenericUtf8String(group.artifact_path)) << ",";
        out << "\"groupIndex\":" << group.group_index << ",";
        out << "\"logicalFrameIndex\":" << group.logical_frame_index << ",";
        out << "\"reason\":" << import_detail::JsonString(group.reason) << ",";
        out << "\"sources\":[";
        for (size_t j = 0; j < group.sources.size(); ++j) {
            const auto& source = group.sources[j];
            if (j > 0) out << ",";
            out << "{\"path\":" << import_detail::JsonString(source.path.generic_string())
                << ",\"role\":" << import_detail::JsonString(source.role) << "}";
        }
        out << "],";
        out << "\"status\":" << import_detail::JsonString(group.status) << ",";
        out << "\"structure_reason\":" << import_detail::JsonString(group.structure_reason) << ",";
        out << "\"structure_score_gb\":" << group.structure_score_gb << ",";
        out << "\"structure_score_rb\":" << group.structure_score_rb << ",";
        out << "\"structure_score_rg\":" << group.structure_score_rg << ",";
        out << "\"warnings\":[";
        for (size_t j = 0; j < group.warnings.size(); ++j) {
            if (j > 0) out << ",";
            out << import_detail::JsonString(group.warnings[j]);
        }
        out << "]}";
    }
    out << "],";
    out << "\"incompleteGroupCount\":" << incomplete_group_count << ",";
    out << "\"mode\":" << import_detail::JsonString(mode) << ",";
    out << "\"sensorType\":" << import_detail::JsonString(sensor_type) << ",";
    out << "\"skippedGroupCount\":" << skipped_group_count << ",";
    out << "\"sortPolicy\":" << import_detail::JsonString(sort_policy) << ",";
    out << "\"strictOrder\":" << import_detail::JsonString(strict_order) << ",";
    out << "\"validGroupCount\":" << valid_group_count << "}";
    return out.str();
}

class TrichromeImportService {
 public:
    using SuffixDetector = std::function<std::string(const std::filesystem::path&)>;
    using FolderCollector = std::function<std::vector<std::filesystem::path>(
        const std::filesystem::path&, const std::string&)>;
    using MetadataProbe = std::function<RawImageMetadata(const std::filesystem::path&)>;
    using StructureProbe = std::function<TrichromeStructureDiagnostic(const TrichromeInputGroup&)>;

    explicit TrichromeImportService(SuffixDetector detect_suffix = DetectDominantRawSuffix,
                                    FolderCollector collect_folder = {},
                                    MetadataProbe metadata_probe = ProbeRawImageMetadata,
                                    StructureProbe structure_probe = {})
        : detect_suffix_(std::move(detect_suffix)),
          collect_folder_(std::move(collect_folder)),
          metadata_probe_(std::move(metadata_probe)),
          structure_probe_(std::move(structure_probe)) {}

    [[nodiscard]] TrichromeImportPlan FromRequest(TrichromeImportRequest request) const {
        const bool has_folder_source = !request.folder.empty();
        const TrichromeSortPolicy effective_sort_policy = has_folder_source
            ? TrichromeSortPolicy::kFilename
            : request.sort_policy;
        TrichromeImportPlan plan;
        std::vector<std::filesystem::path> files = ResolveFiles(request);
        import_detail::SortAndDeduplicate(&files, effective_sort_policy);
        plan.report.mode = InputModeName(request.mode);
        plan.report.sensor_type = InputSensorTypeName(request.sensor_type);
        plan.report.strict_order = TrichromeStrictOrder(request.mode);
        plan.report.sort_policy = import_detail::SortPolicyName(effective_sort_policy);
        plan.report.group_size = TrichromeGroupSize(request.mode);
        plan.report.file_count = static_cast<int>(files.size());
        if (!import_detail::IsTrichromeMode(request.mode)) return plan;

        std::vector<TrichromeInputGroup> valid_groups;
        const int group_size = plan.report.group_size;
        const int n_groups = group_size > 0
            ? static_cast<int>((files.size() + group_size - 1) / group_size)
            : 0;
        for (int group_index = 0; group_index < n_groups; ++group_index) {
            const int begin = group_index * group_size;
            const int count = std::min(group_size, static_cast<int>(files.size()) - begin);
            const bool complete = count == group_size;

            TrichromeImportGroupReport group_report;
            group_report.group_index = group_index;
            group_report.logical_frame_index = complete ? static_cast<int>(valid_groups.size()) : -1;
            group_report.status = complete ? "valid" : "skipped";
            group_report.reason = complete ? "ok" : "incomplete_group";
            group_report.artifact_path = complete
                ? import_detail::ArtifactPath(request.artifact_dir, group_report.logical_frame_index,
                                              request.artifact_format)
                : std::filesystem::path();

            TrichromeInputGroup valid_group;
            valid_group.mode = request.mode;
            valid_group.sensor_type = request.sensor_type;
            valid_group.group_index = group_report.group_index;
            valid_group.logical_frame_index = group_report.logical_frame_index;
            valid_group.artifact_path = group_report.artifact_path;
            valid_group.artifact_format = request.artifact_format;
            valid_group.validation_status = "valid";

            for (int offset = 0; offset < count; ++offset) {
                const InputGroupRole role = import_detail::RoleForOffset(request.mode, offset);
                group_report.sources.push_back(
                    {TrichromeSourceRoleName(role), files[begin + offset]});
                if (complete) {
                    InputGroup source_group;
                    source_group.mode = request.mode;
                    source_group.role = role;
                    source_group.files = {files[begin + offset]};
                    valid_group.sources.push_back(std::move(source_group));
                }
            }

            if (complete && request.metadata_policy == TrichromeMetadataPolicy::kStrict) {
                std::vector<RawImageMetadata> metadata;
                for (int offset = 0; offset < count; ++offset)
                    metadata.push_back(metadata_probe_ ? metadata_probe_(files[begin + offset])
                                                       : RawImageMetadata{});
                group_report.reason = import_detail::ValidateMetadataGroup(
                    metadata, &group_report.warnings);
                if (group_report.reason != "ok") {
                    group_report.status = "skipped";
                    group_report.logical_frame_index = -1;
                    group_report.artifact_path = std::filesystem::path();
                    valid_group.validation_status = "invalid";
                }
            }

            if (complete && group_report.status == "valid" &&
                request.structure_policy == TrichromeStructurePolicy::kCheck) {
                TrichromeStructureDiagnostic diagnostic;
                diagnostic.reason = "structure_decoder_missing";
                if (structure_probe_) diagnostic = structure_probe_(valid_group);
                ApplyStructureDiagnosticToImportGroup(diagnostic, &group_report);
            }

            if (complete && group_report.status == "valid") {
                ++plan.report.valid_group_count;
                valid_groups.push_back(std::move(valid_group));
            } else {
                ++plan.report.skipped_group_count;
                if (!complete) ++plan.report.incomplete_group_count;
            }
            plan.report.groups.push_back(std::move(group_report));
        }

        if (!valid_groups.empty()) {
            std::string name = request.name;
            if (name.empty()) {
                name = !request.folder.empty()
                    ? PathUtf8String(request.folder.filename())
                    : PathUtf8String(files.front().parent_path().filename());
            }
            plan.recipe = MakeTrichromeInputRecipe(
                std::move(name), request.mode, request.sensor_type, std::move(valid_groups));
        }
        return plan;
    }

 private:
    [[nodiscard]] std::vector<std::filesystem::path> ResolveFiles(
        const TrichromeImportRequest& request) const {
        if (!request.folder.empty()) {
            const std::string suffix = detect_suffix_(request.folder);
            if (suffix.empty()) return {};
            if (collect_folder_) return collect_folder_(request.folder, suffix);
            return CollectRollFrames(request.folder, suffix);
        }
        std::vector<std::filesystem::path> files;
        for (const auto& file : request.files) {
            if (IsImportableRawPath(file)) files.push_back(file);
        }
        return files;
    }

    SuffixDetector detect_suffix_;
    FolderCollector collect_folder_;
    MetadataProbe metadata_probe_;
    StructureProbe structure_probe_;
};

}  // namespace softloaf::trichrome
