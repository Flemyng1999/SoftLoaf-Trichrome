#pragma once

#include <string>

namespace softloaf::trichrome {

inline constexpr const char* kRawDecodePolicyKey =
    "raw-boundary-v3:rec2020-cfa-only:mono-intensity-linear-no-matrix:packed-fullcolor-fallback:rt-white:sony-packed-hints:canon70d-sraw-white:leica-sl2-matrix";
inline constexpr const char* kRawProvenanceSchemaKey = "raw-provenance-v1";

enum class RawSensorClass {
    kBayerOrOtherCfa,
    kXTrans,
    kPackedRgb,
    kPackedFourColor,
    kMonochrome,
    kFoveon,
    kUnknownFiltersZero,
};

enum class RawLinearRec2020Policy {
    kSupported,
    kFallbackOnly,
    kUnsupported,
};

enum class RawDecodeMode {
    kPreview,
    kExport,
};

enum class RawDecodeFallbackStatus {
    kNone,
    kFallbackOnly,
    kUnsupported,
};

enum class RawDecodeTarget {
    kCameraNativeLinear,
    // A monochrome RAW is a single intensity plane.  It must never be passed
    // through an RGB camera matrix; LibRaw's gamma=1 output is already linear.
    kMonochromeIntensityLinear,
    kLinearRec2020,
};

struct RawClassificationInput {
    unsigned filters = 0;
    int colors = 0;
    bool is_foveon = false;
    bool has_xtrans = false;
    bool has_color3_image = false;
    bool has_color4_image = false;
    bool has_float3_image = false;
    bool has_float4_image = false;
};

struct RawDecodeProvenance {
    RawSensorClass raw_class = RawSensorClass::kUnknownFiltersZero;
    RawLinearRec2020Policy policy = RawLinearRec2020Policy::kUnsupported;
    RawDecodeMode decode_mode = RawDecodeMode::kExport;
    RawDecodeFallbackStatus fallback_status = RawDecodeFallbackStatus::kUnsupported;
    std::string target_color_space = "unknown";
    std::string policy_key = kRawDecodePolicyKey;
    bool present = false;
};

inline RawSensorClass ClassifyRawSensor(const RawClassificationInput& input) {
    if (input.is_foveon) return RawSensorClass::kFoveon;
    if (input.filters == 9 || input.has_xtrans) return RawSensorClass::kXTrans;
    if (input.filters != 0) return RawSensorClass::kBayerOrOtherCfa;
    if (input.colors <= 1) return RawSensorClass::kMonochrome;
    if (input.has_color4_image || input.has_float4_image || input.colors == 4)
        return RawSensorClass::kPackedFourColor;
    if (input.has_color3_image || input.has_float3_image || input.colors == 3)
        return RawSensorClass::kPackedRgb;
    return RawSensorClass::kUnknownFiltersZero;
}

inline RawLinearRec2020Policy LinearRec2020PolicyFor(RawSensorClass raw_class) {
    switch (raw_class) {
        case RawSensorClass::kBayerOrOtherCfa:
            return RawLinearRec2020Policy::kSupported;
        case RawSensorClass::kXTrans:
        case RawSensorClass::kPackedRgb:
        case RawSensorClass::kPackedFourColor:
            return RawLinearRec2020Policy::kFallbackOnly;
        case RawSensorClass::kMonochrome:
        case RawSensorClass::kFoveon:
        case RawSensorClass::kUnknownFiltersZero:
            return RawLinearRec2020Policy::kUnsupported;
    }
    return RawLinearRec2020Policy::kUnsupported;
}

inline const char* RawSensorClassName(RawSensorClass raw_class) {
    switch (raw_class) {
        case RawSensorClass::kBayerOrOtherCfa:
            return "bayer_or_other_cfa";
        case RawSensorClass::kXTrans:
            return "xtrans";
        case RawSensorClass::kPackedRgb:
            return "packed_rgb";
        case RawSensorClass::kPackedFourColor:
            return "packed_four_color";
        case RawSensorClass::kMonochrome:
            return "monochrome";
        case RawSensorClass::kFoveon:
            return "foveon";
        case RawSensorClass::kUnknownFiltersZero:
            return "unknown_filters_zero";
    }
    return "unknown_filters_zero";
}

inline const char* RawLinearRec2020PolicyName(RawLinearRec2020Policy policy) {
    switch (policy) {
        case RawLinearRec2020Policy::kSupported:
            return "supported";
        case RawLinearRec2020Policy::kFallbackOnly:
            return "fallback_only";
        case RawLinearRec2020Policy::kUnsupported:
            return "unsupported";
    }
    return "unsupported";
}

inline const char* RawDecodeModeName(RawDecodeMode decode_mode) {
    switch (decode_mode) {
        case RawDecodeMode::kPreview:
            return "preview";
        case RawDecodeMode::kExport:
            return "export";
    }
    return "export";
}

inline RawDecodeFallbackStatus FallbackStatusFor(RawLinearRec2020Policy policy) {
    switch (policy) {
        case RawLinearRec2020Policy::kSupported:
            return RawDecodeFallbackStatus::kNone;
        case RawLinearRec2020Policy::kFallbackOnly:
            return RawDecodeFallbackStatus::kFallbackOnly;
        case RawLinearRec2020Policy::kUnsupported:
            return RawDecodeFallbackStatus::kUnsupported;
    }
    return RawDecodeFallbackStatus::kUnsupported;
}

inline const char* RawDecodeFallbackStatusName(RawDecodeFallbackStatus status) {
    switch (status) {
        case RawDecodeFallbackStatus::kNone:
            return "none";
        case RawDecodeFallbackStatus::kFallbackOnly:
            return "fallback_only";
        case RawDecodeFallbackStatus::kUnsupported:
            return "unsupported";
    }
    return "unsupported";
}

inline const char* RawDecodeTargetColorSpaceName(RawDecodeTarget target) {
    switch (target) {
        case RawDecodeTarget::kCameraNativeLinear:
            return "camera_native_linear";
        case RawDecodeTarget::kMonochromeIntensityLinear:
            return "intensity_linear";
        case RawDecodeTarget::kLinearRec2020:
            return "rec_2020_linear";
    }
    return "unknown";
}

inline bool RawDecodePolicyAllowsTarget(RawLinearRec2020Policy policy,
                                        RawDecodeTarget target) {
    if (target == RawDecodeTarget::kLinearRec2020)
        return policy == RawLinearRec2020Policy::kSupported;
    if (target == RawDecodeTarget::kCameraNativeLinear)
        return policy == RawLinearRec2020Policy::kSupported ||
               policy == RawLinearRec2020Policy::kFallbackOnly;
    return false;
}

// Target eligibility cannot be inferred from the Rec.2020 policy alone:
// monochrome and unsupported multi-channel sensors both use "unsupported" for
// Rec.2020, but only monochrome RAW has the safe intensity-linear path.
inline bool RawSensorAllowsTarget(RawSensorClass raw_class, RawDecodeTarget target) {
    if (target == RawDecodeTarget::kMonochromeIntensityLinear)
        return raw_class == RawSensorClass::kMonochrome;
    return RawDecodePolicyAllowsTarget(LinearRec2020PolicyFor(raw_class), target);
}

inline const char* RawDecodePolicyTargetReason(RawLinearRec2020Policy policy,
                                               RawDecodeTarget target) {
    if (RawDecodePolicyAllowsTarget(policy, target)) return "ok";
    if (policy == RawLinearRec2020Policy::kFallbackOnly &&
        target == RawDecodeTarget::kLinearRec2020) {
        return "raw_fallback_only_not_rec2020";
    }
    return "raw_unsupported";
}

inline const char* RawSensorTargetReason(RawSensorClass raw_class,
                                         RawDecodeTarget target) {
    if (RawSensorAllowsTarget(raw_class, target)) return "ok";
    if (target == RawDecodeTarget::kMonochromeIntensityLinear)
        return "raw_not_monochrome";
    return RawDecodePolicyTargetReason(LinearRec2020PolicyFor(raw_class), target);
}

inline RawDecodeProvenance MakeRawDecodeProvenance(
    RawSensorClass raw_class,
    RawDecodeMode decode_mode,
    const std::string& target_color_space) {
    const RawLinearRec2020Policy policy = LinearRec2020PolicyFor(raw_class);
    return {raw_class,
            policy,
            decode_mode,
            FallbackStatusFor(policy),
            target_color_space,
            kRawDecodePolicyKey,
            true};
}

inline RawDecodeProvenance MakeRawDecodeProvenance(
    RawSensorClass raw_class,
    RawDecodeMode decode_mode,
    RawDecodeTarget target) {
    return MakeRawDecodeProvenance(raw_class, decode_mode,
                                   RawDecodeTargetColorSpaceName(target));
}

inline std::string RawDecodeProvenanceSignature(const RawDecodeProvenance& provenance) {
    if (!provenance.present) return "raw-provenance:none";
    return std::string(kRawProvenanceSchemaKey) +
           ":class=" + RawSensorClassName(provenance.raw_class) +
           ":policy=" + RawLinearRec2020PolicyName(provenance.policy) +
           ":mode=" + RawDecodeModeName(provenance.decode_mode) +
           ":fallback=" + RawDecodeFallbackStatusName(provenance.fallback_status) +
           ":target=" + provenance.target_color_space +
           ":key=" + provenance.policy_key;
}

inline std::string RawDecodePipelineIdentity() {
    return std::string(kRawProvenanceSchemaKey) + ":" + kRawDecodePolicyKey;
}

}  // namespace softloaf::trichrome
