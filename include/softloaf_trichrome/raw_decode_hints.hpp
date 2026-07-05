#pragma once

#include <algorithm>
#include <array>
#include <cctype>
#include <optional>
#include <string>
#include <string_view>

namespace softloaf::trichrome {

struct RawDecodeHintInput {
    std::string_view make;
    std::string_view model;
    unsigned filters = 0;
    int colors = 0;
    bool has_raw_image = false;
    bool has_color3_image = false;
    bool has_color4_image = false;
    bool has_float3_image = false;
    bool has_float4_image = false;
    unsigned black = 0;
    std::array<unsigned, 6> cblack = {};
    int raw_width = 0;
    int raw_height = 0;
};

struct RawCropHint {
    int left = 0;
    int top = 0;
    int width = 0;
    int height = 0;
    const char* label = "";
};

inline std::string RawHintMakeModel(const RawDecodeHintInput& input) {
    std::string out(input.make);
    if (!out.empty() && !input.model.empty()) out.push_back(' ');
    out.append(input.model);
    return out;
}

inline std::string NormalizeRawHintName(std::string_view value) {
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

inline bool RawHintModelIs(const RawDecodeHintInput& input,
                           std::string_view expected) {
    return NormalizeRawHintName(input.model) == NormalizeRawHintName(expected);
}

inline bool RawHintMakeContains(const RawDecodeHintInput& input,
                                std::string_view expected) {
    return NormalizeRawHintName(input.make).find(NormalizeRawHintName(expected)) !=
           std::string::npos;
}

inline bool IsSonyPackedFullColorColor4(const RawDecodeHintInput& input) {
    return RawHintMakeContains(input, "sony") &&
           input.filters == 0 &&
           input.colors == 3 &&
           !input.has_raw_image &&
           input.has_color4_image;
}

inline std::optional<unsigned> SonyPackedFullColorBlackHint(
    const RawDecodeHintInput& input) {
    if (!IsSonyPackedFullColorColor4(input) || input.black != 1024) return std::nullopt;
    for (unsigned v : input.cblack) {
        if (v != 0) return std::nullopt;
    }
    return 512u;
}

inline std::optional<unsigned> Canon70DReducedRawWhiteHint(
    const RawDecodeHintInput& input) {
    if (!RawHintMakeContains(input, "canon") ||
        !RawHintModelIs(input, "EOS 70D")) {
        return std::nullopt;
    }
    if (input.filters != 0 || input.colors != 3 ||
        input.has_raw_image || !input.has_color4_image) {
        return std::nullopt;
    }
    return 13480u;
}

inline std::optional<RawCropHint> ProcessedRawCropHint(
    const RawDecodeHintInput& input) {
    if (!RawHintMakeContains(input, "sony") ||
        !RawHintModelIs(input, "ILCE-1M2") ||
        input.raw_width != 5632 ||
        input.raw_height != 4096) {
        return std::nullopt;
    }
    return RawCropHint{0, 0, 5628, 3756,
                       "sony_ilce_1m2_lossless_m_raw_crop_v1"};
}

inline std::string RawDecodeHintSummary(const RawDecodeHintInput& input) {
    std::string out;
    const auto append = [&](const char* label) {
        if (!out.empty()) out.push_back(';');
        out.append(label);
    };
    if (SonyPackedFullColorBlackHint(input))
        append("sony_packed_color4_black_v1");
    if (Canon70DReducedRawWhiteHint(input))
        append("canon_70d_sraw_white_v1");
    if (const auto crop = ProcessedRawCropHint(input))
        append(crop->label);
    return out.empty() ? "none" : out;
}

}  // namespace softloaf::trichrome
