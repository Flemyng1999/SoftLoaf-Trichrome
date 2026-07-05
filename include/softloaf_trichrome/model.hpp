#pragma once

#include <algorithm>
#include <array>
#include <cctype>
#include <filesystem>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

namespace softloaf::trichrome {

inline std::string PathUtf8String(const std::filesystem::path& path) {
    const auto utf8 = path.u8string();
    return std::string(reinterpret_cast<const char*>(utf8.data()), utf8.size());
}

inline std::string PathGenericUtf8String(const std::filesystem::path& path) {
    const auto utf8 = path.generic_u8string();
    return std::string(reinterpret_cast<const char*>(utf8.data()), utf8.size());
}

enum class InputMode {
    kSingle,
    kRoll,
    kTrichromeRgb,
    kTrichromeRgbNir,
};

enum class InputGroupRole {
    kPrimary,
    kTrichromeRed,
    kTrichromeGreen,
    kTrichromeBlue,
    kTrichromeNir,
};

enum class InputSensorType {
    kMono,
    kBayer,
};

inline std::string InputModeName(InputMode mode) {
    switch (mode) {
        case InputMode::kSingle: return "single";
        case InputMode::kRoll: return "roll";
        case InputMode::kTrichromeRgb: return "trichrome_rgb";
        case InputMode::kTrichromeRgbNir: return "trichrome_rgb_nir";
    }
    return "roll";
}

inline std::string InputGroupRoleName(InputGroupRole role) {
    switch (role) {
        case InputGroupRole::kPrimary: return "primary";
        case InputGroupRole::kTrichromeRed: return "trichrome_red";
        case InputGroupRole::kTrichromeGreen: return "trichrome_green";
        case InputGroupRole::kTrichromeBlue: return "trichrome_blue";
        case InputGroupRole::kTrichromeNir: return "trichrome_nir";
    }
    return "primary";
}

inline std::string TrichromeSourceRoleName(InputGroupRole role) {
    switch (role) {
        case InputGroupRole::kTrichromeRed: return "red";
        case InputGroupRole::kTrichromeGreen: return "green";
        case InputGroupRole::kTrichromeBlue: return "blue";
        case InputGroupRole::kTrichromeNir: return "nir";
        case InputGroupRole::kPrimary: return "primary";
    }
    return "primary";
}

inline std::string InputSensorTypeName(InputSensorType sensor) {
    switch (sensor) {
        case InputSensorType::kMono: return "mono";
        case InputSensorType::kBayer: return "bayer";
    }
    return "mono";
}

inline int TrichromeGroupSize(InputMode mode) {
    return mode == InputMode::kTrichromeRgbNir ? 4 : 3;
}

inline std::string TrichromeStrictOrder(InputMode mode) {
    return mode == InputMode::kTrichromeRgbNir ? "RGBNIR" : "RGB";
}

struct FrameEntry {
    std::string path;
};

struct ProjectInputGroup {
    std::string mode = "roll";
    std::string role = "primary";
    std::vector<int> frame_indices;
};

struct ProjectTrichromeSource {
    std::string role;
    std::string path;
    bool probe_ok = false;
    uint64_t probe_size = 0;
    int64_t probe_mtime = 0;
    uint64_t probe_partial_hash = 0;
    std::string raw_class;
    std::string raw_policy;
    std::string raw_decode_mode;
    std::string raw_fallback_status;
    std::string raw_target_color_space;
    std::string raw_provenance_sig;
};

struct ProjectTrichromeRoleShift {
    std::string role;
    double dx = 0.0;
    double dy = 0.0;
};

struct ProjectTrichromeGroup {
    std::string mode = "trichrome_rgb";
    std::string sensor_type = "mono";
    std::string strict_order = "RGB";
    std::string exposure_policy = "camera_exposure_must_match";
    std::string validation_status = "pending";
    int group_index = 0;
    int group_size = 3;
    int logical_frame_index = -1;
    std::string artifact_path;
    std::string artifact_format = "rgb16_npy";
    std::string artifact_pixel_type = "uint16";
    int artifact_width = 0;
    int artifact_height = 0;
    std::string artifact_channel_order = "RGB";
    std::string artifact_color_state = "camera_linear";
    bool artifact_valid = false;
    std::string artifact_dirty_reason;
    std::string artifact_sig;
    std::string preview_artifact_path;
    int preview_artifact_width = 0;
    int preview_artifact_height = 0;
    int preview_max_edge = 0;
    bool preview_artifact_valid = false;
    std::string preview_artifact_dirty_reason;
    std::string preview_artifact_sig;
    int compose_algo_version = 1;
    std::string input_preprocess_sig;
    std::string alignment_policy = "identity";
    bool alignment_applied = false;
    std::vector<ProjectTrichromeRoleShift> alignment_shifts;
    std::vector<ProjectTrichromeSource> sources;
};

struct ProjectMeta {
    int schema_version = 1;
    std::string name;
    std::string input_mode = "roll";
    std::string default_demask = "roll";
    bool demask_active = false;
    std::vector<ProjectInputGroup> input_groups;
    std::vector<FrameEntry> frames;
    std::vector<ProjectTrichromeGroup> trichrome_groups;
    bool trichrome_roll_white_valid = false;
    std::array<double, 3> trichrome_roll_white{1.0, 1.0, 1.0};
};

struct InputGroup {
    InputMode mode = InputMode::kRoll;
    InputGroupRole role = InputGroupRole::kPrimary;
    std::vector<std::filesystem::path> files;
};

struct TrichromeInputGroup {
    InputMode mode = InputMode::kTrichromeRgb;
    InputSensorType sensor_type = InputSensorType::kMono;
    int group_index = 0;
    int logical_frame_index = 0;
    std::filesystem::path artifact_path;
    std::string artifact_format = "rgb16_npy";
    std::string artifact_sig;
    int compose_algo_version = 1;
    std::string validation_status = "pending";
    std::vector<InputGroup> sources;
};

struct InputRecipe {
    std::string name;
    InputMode mode = InputMode::kRoll;
    std::vector<InputGroup> groups;
    std::vector<TrichromeInputGroup> trichrome_groups;
    std::vector<std::filesystem::path> files;

    [[nodiscard]] ProjectMeta ToProjectMeta() const {
        ProjectMeta meta;
        meta.name = name;
        meta.input_mode = InputModeName(mode);
        if (!trichrome_groups.empty()) {
            meta.schema_version = 2;
            meta.default_demask = "roll";
            meta.demask_active = false;
        }
        for (const auto& p : files) meta.frames.push_back(FrameEntry{PathUtf8String(p)});
        if (!trichrome_groups.empty()) {
            ProjectInputGroup pg;
            pg.mode = meta.input_mode;
            pg.role = InputGroupRoleName(InputGroupRole::kPrimary);
            for (int i = 0; i < static_cast<int>(files.size()); ++i) pg.frame_indices.push_back(i);
            meta.input_groups.push_back(std::move(pg));
            for (const TrichromeInputGroup& group : trichrome_groups) {
                ProjectTrichromeGroup out;
                out.mode = InputModeName(group.mode);
                out.sensor_type = InputSensorTypeName(group.sensor_type);
                out.strict_order = TrichromeStrictOrder(group.mode);
                out.validation_status = group.validation_status;
                out.group_index = group.group_index;
                out.group_size = TrichromeGroupSize(group.mode);
                out.logical_frame_index = group.logical_frame_index;
                out.artifact_path = PathUtf8String(group.artifact_path);
                out.artifact_format = group.artifact_format;
                out.artifact_sig = group.artifact_sig;
                out.compose_algo_version = group.compose_algo_version;
                for (const InputGroup& source_group : group.sources) {
                    ProjectTrichromeRoleShift shift;
                    shift.role = TrichromeSourceRoleName(source_group.role);
                    out.alignment_shifts.push_back(std::move(shift));
                    for (const auto& source_path : source_group.files) {
                        ProjectTrichromeSource source;
                        source.role = TrichromeSourceRoleName(source_group.role);
                        source.path = PathUtf8String(source_path);
                        out.sources.push_back(std::move(source));
                    }
                }
                meta.trichrome_groups.push_back(std::move(out));
            }
        }
        return meta;
    }
};

inline std::string LowerExtension(std::filesystem::path path) {
    std::string ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return ext;
}

inline const std::vector<std::string>& RawLikeExtensions() {
    static const std::vector<std::string> kRawExts = {
        ".3fr", ".fff", ".dng", ".arw", ".cr2", ".cr3", ".nef", ".raf",
        ".raw", ".rw2", ".orf", ".pef", ".srw", ".x3f", ".tif", ".tiff"};
    return kRawExts;
}

inline const std::vector<std::string>& RegularImageExtensions() {
    static const std::vector<std::string> kImageExts = {".jpg", ".jpeg", ".png"};
    return kImageExts;
}

inline std::vector<std::string> SupportedStillImageExtensions() {
    std::vector<std::string> exts = RawLikeExtensions();
    const std::vector<std::string>& images = RegularImageExtensions();
    exts.insert(exts.end(), images.begin(), images.end());
    return exts;
}

inline bool IsRawLikeExtension(const std::string& ext) {
    const std::vector<std::string>& raw_exts = RawLikeExtensions();
    return std::find(raw_exts.begin(), raw_exts.end(), ext) != raw_exts.end();
}

inline bool IsSupportedStillImageExtension(const std::string& ext) {
    const std::vector<std::string>& image_exts = RegularImageExtensions();
    return IsRawLikeExtension(ext) ||
           std::find(image_exts.begin(), image_exts.end(), ext) != image_exts.end();
}

inline std::string SupportedImagesFileFilter() {
    std::string filter = "Supported images (";
    bool first = true;
    for (const std::string& ext : SupportedStillImageExtensions()) {
        if (!first) filter += ' ';
        first = false;
        filter += '*';
        filter += ext;
    }
    filter += ");;All files (*)";
    return filter;
}

inline bool IsAppleDoubleSidecarPath(const std::filesystem::path& path) {
    const std::string name = PathUtf8String(path.filename());
    return name.rfind("._", 0) == 0;
}

inline bool IsImportableRawPath(const std::filesystem::path& path) {
    return !IsAppleDoubleSidecarPath(path) &&
           IsSupportedStillImageExtension(LowerExtension(path));
}

inline InputRecipe MakeTrichromeInputRecipe(std::string name,
                                            InputMode mode,
                                            InputSensorType sensor_type,
                                            std::vector<TrichromeInputGroup> groups) {
    InputRecipe recipe;
    recipe.name = std::move(name);
    recipe.mode = mode;
    recipe.trichrome_groups = std::move(groups);
    for (const TrichromeInputGroup& group : recipe.trichrome_groups)
        recipe.files.push_back(group.artifact_path);
    for (TrichromeInputGroup& group : recipe.trichrome_groups) {
        group.mode = mode;
        group.sensor_type = sensor_type;
    }
    return recipe;
}

}  // namespace softloaf::trichrome
