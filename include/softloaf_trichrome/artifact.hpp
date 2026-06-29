#pragma once

#include <array>
#include <bit>
#include <cstdint>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <string>

#include "softloaf_trichrome/cache_probe.hpp"
#include "softloaf_trichrome/model.hpp"

namespace softloaf::trichrome {

inline constexpr int kComposeAlgoVersion = 6;
inline constexpr int kPreviewArtifactMaxEdge = 4096;

enum class ArtifactTier { kPreview, kFull };

inline const char* ArtifactTierName(ArtifactTier tier) {
    return tier == ArtifactTier::kPreview ? "preview" : "full";
}

enum class ArtifactReadiness { kCurrent, kMissing, kDirty };

struct ArtifactGuardResult {
    ArtifactReadiness readiness = ArtifactReadiness::kMissing;
    std::string reason = "artifact_missing";
    [[nodiscard]] bool ok() const { return readiness == ArtifactReadiness::kCurrent; }
};

namespace artifact_detail {
inline void HashString(uint64_t* h, const std::string& value) {
    for (unsigned char c : value) detail::HashByte(h, c);
    detail::HashByte(h, 0xff);
}

inline void HashUint64(uint64_t* h, uint64_t value) {
    for (int i = 0; i < 8; ++i)
        detail::HashByte(h, static_cast<unsigned char>((value >> (i * 8)) & 0xff));
}

inline void HashInt64(uint64_t* h, int64_t value) {
    HashUint64(h, static_cast<uint64_t>(value));
}

inline std::string HexDigest(uint64_t h) {
    std::ostringstream out;
    out << std::hex << std::setw(16) << std::setfill('0') << h;
    return out.str();
}
}  // namespace artifact_detail

inline void SetSourceProbe(ProjectTrichromeSource* source, const FileProbe& probe) {
    if (!source) return;
    source->probe_ok = probe.ok;
    source->probe_size = probe.size;
    source->probe_mtime = probe.mtime;
    source->probe_partial_hash = probe.partial_hash;
}

inline void RefreshSourceProbes(ProjectTrichromeGroup* group) {
    if (!group) return;
    for (ProjectTrichromeSource& source : group->sources)
        SetSourceProbe(&source, ProbeFileWithPartialHash(source.path));
}

inline std::string ComputeArtifactSignature(const ProjectTrichromeGroup& group,
                                            ArtifactTier tier,
                                            const std::array<double, 3>& roll_white) {
    uint64_t h = detail::kFnvOffset;
    artifact_detail::HashString(&h, "softloaf.trichrome.artifact.v1");
    artifact_detail::HashString(&h, ArtifactTierName(tier));
    artifact_detail::HashString(&h, group.mode);
    artifact_detail::HashString(&h, group.sensor_type);
    artifact_detail::HashString(&h, group.strict_order);
    artifact_detail::HashString(&h, group.validation_status);
    artifact_detail::HashInt64(&h, group.group_size);
    artifact_detail::HashString(&h, group.artifact_format);
    artifact_detail::HashString(&h, group.artifact_pixel_type);
    if (tier == ArtifactTier::kPreview) {
        artifact_detail::HashInt64(&h, group.preview_artifact_width);
        artifact_detail::HashInt64(&h, group.preview_artifact_height);
        artifact_detail::HashInt64(&h, group.preview_max_edge);
    } else {
        artifact_detail::HashInt64(&h, group.artifact_width);
        artifact_detail::HashInt64(&h, group.artifact_height);
    }
    artifact_detail::HashString(&h, group.artifact_channel_order);
    artifact_detail::HashString(&h, group.artifact_color_state);
    artifact_detail::HashInt64(&h, group.compose_algo_version);
    artifact_detail::HashString(&h, group.input_preprocess_sig);
    for (const double white : roll_white)
        artifact_detail::HashUint64(&h, std::bit_cast<uint64_t>(white));
    artifact_detail::HashInt64(&h, static_cast<int64_t>(group.sources.size()));
    for (const ProjectTrichromeSource& source : group.sources) {
        artifact_detail::HashString(&h, source.role);
        artifact_detail::HashString(&h, source.path);
        detail::HashByte(&h, source.probe_ok ? 1 : 0);
        artifact_detail::HashUint64(&h, source.probe_size);
        artifact_detail::HashInt64(&h, source.probe_mtime);
        artifact_detail::HashUint64(&h, source.probe_partial_hash);
    }
    return artifact_detail::HexDigest(h);
}

inline ArtifactGuardResult EvaluateArtifactReadiness(const ProjectTrichromeGroup& group,
                                                     ArtifactTier tier,
                                                     const std::array<double, 3>& roll_white,
                                                     const FileProbe& artifact_probe) {
    const bool preview = tier == ArtifactTier::kPreview;
    const std::string& path = preview ? group.preview_artifact_path : group.artifact_path;
    const bool valid = preview ? group.preview_artifact_valid : group.artifact_valid;
    const std::string& sig = preview ? group.preview_artifact_sig : group.artifact_sig;
    if (path.empty() || !artifact_probe.ok) return {ArtifactReadiness::kMissing, "artifact_missing"};
    if (!valid) return {ArtifactReadiness::kDirty, "artifact_not_valid"};
    if (sig != ComputeArtifactSignature(group, tier, roll_white))
        return {ArtifactReadiness::kDirty, "artifact_identity_mismatch"};
    return {ArtifactReadiness::kCurrent, "ok"};
}

inline std::filesystem::path ResolveArtifactPath(const std::filesystem::path& bundle,
                                                 const std::filesystem::path& artifact_path) {
    return artifact_path.is_absolute() ? artifact_path : bundle / artifact_path;
}

inline std::filesystem::path PreviewArtifactPathFor(const std::filesystem::path& full_artifact_path,
                                                    int max_edge = kPreviewArtifactMaxEdge) {
    std::filesystem::path preview = full_artifact_path;
    std::filesystem::path filename = full_artifact_path.stem();
    filename += "_p";
    filename += std::to_string(max_edge);
    filename += full_artifact_path.extension();
    preview.replace_filename(filename);
    return preview;
}

inline bool CanEnterNormalPipeline(const ProjectMeta& meta,
                                   const std::filesystem::path& bundle,
                                   std::string* reason = nullptr) {
    if (meta.trichrome_groups.empty()) {
        if (reason) *reason = "ok";
        return true;
    }
    for (const ProjectTrichromeGroup& group : meta.trichrome_groups) {
        const auto artifact_path = ResolveArtifactPath(bundle, group.preview_artifact_path);
        const ArtifactGuardResult guard = EvaluateArtifactReadiness(
            group, ArtifactTier::kPreview, meta.trichrome_roll_white,
            StatFile(artifact_path));
        if (!guard.ok()) {
            if (reason) *reason = guard.reason;
            return false;
        }
    }
    if (reason) *reason = "ok";
    return true;
}

}  // namespace softloaf::trichrome
