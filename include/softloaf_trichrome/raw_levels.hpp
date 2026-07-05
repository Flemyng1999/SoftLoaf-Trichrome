#pragma once

#include <algorithm>
#include <array>
#include <optional>

namespace softloaf::trichrome {

struct RawWhiteLevelInputs {
    unsigned black = 0;
    std::array<unsigned, 4> linear_max = {};
    unsigned maximum = 0;
    unsigned data_maximum = 0;
};

inline bool IsPlausibleWhiteLevel(unsigned white,
                                  unsigned black,
                                  unsigned margin = 64) {
    return white > black + margin;
}

inline std::optional<unsigned> SelectTrustedRawWhiteLevel(
    const RawWhiteLevelInputs& input,
    unsigned margin = 64) {
    if (IsPlausibleWhiteLevel(input.maximum, input.black, margin))
        return input.maximum;

    unsigned linear = 0;
    for (unsigned candidate : input.linear_max) {
        if (!IsPlausibleWhiteLevel(candidate, input.black, margin)) continue;
        linear = linear == 0 ? candidate : std::min(linear, candidate);
    }
    if (linear != 0) return linear;

    if (IsPlausibleWhiteLevel(input.data_maximum, input.black, margin))
        return input.data_maximum;
    return std::nullopt;
}

}  // namespace softloaf::trichrome
