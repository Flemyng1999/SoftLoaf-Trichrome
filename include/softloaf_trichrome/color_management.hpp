#pragma once

#include <array>
#include <cmath>
#include <cstddef>
#include <optional>
#include <string_view>
#include <vector>

namespace softloaf::trichrome::color {

struct Xy {
    double x = 0.0;
    double y = 0.0;
};

struct Primaries {
    Xy red;
    Xy green;
    Xy blue;
};

using Vec3 = std::array<double, 3>;
using Mat3 = std::array<double, 9>;

enum class TransferCurve {
    kLinear,
    kSrgb,
    kGamma,
};

struct RgbColorSpace {
    std::string_view id;
    std::string_view label;
    Xy white;
    Primaries primaries;
    TransferCurve transfer = TransferCurve::kLinear;
    double gamma = 1.0;
};

inline constexpr Xy kD50{0.3457, 0.3585};
inline constexpr Xy kD60{0.32168, 0.33767};
inline constexpr Xy kD65{0.3127, 0.3290};

inline constexpr RgbColorSpace kSrgb{
    "srgb",
    "sRGB",
    kD65,
    {{0.6400, 0.3300}, {0.3000, 0.6000}, {0.1500, 0.0600}},
    TransferCurve::kSrgb,
    2.4,
};

inline constexpr RgbColorSpace kDisplayP3{
    "display_p3",
    "Display P3",
    kD65,
    {{0.6800, 0.3200}, {0.2650, 0.6900}, {0.1500, 0.0600}},
    TransferCurve::kSrgb,
    2.4,
};

inline constexpr RgbColorSpace kP3D65Gamma26{
    "p3_d65_gamma_2_6",
    "P3-D65 Gamma 2.6",
    kD65,
    {{0.6800, 0.3200}, {0.2650, 0.6900}, {0.1500, 0.0600}},
    TransferCurve::kGamma,
    2.6,
};

inline constexpr RgbColorSpace kRec709Gamma24{
    "rec_709",
    "Rec.709 Gamma 2.4",
    kD65,
    {{0.6400, 0.3300}, {0.3000, 0.6000}, {0.1500, 0.0600}},
    TransferCurve::kGamma,
    2.4,
};

inline constexpr RgbColorSpace kAdobeRgb{
    "adobe_rgb",
    "Adobe RGB",
    kD65,
    {{0.6400, 0.3300}, {0.2100, 0.7100}, {0.1500, 0.0600}},
    TransferCurve::kGamma,
    2.2,
};

inline constexpr RgbColorSpace kRec2020Linear{
    "rec_2020_linear",
    "Rec.2020 Linear",
    kD65,
    {{0.7080, 0.2920}, {0.1700, 0.7970}, {0.1310, 0.0460}},
    TransferCurve::kLinear,
    1.0,
};

inline constexpr RgbColorSpace kProPhotoLinear{
    "prophoto_linear",
    "ProPhoto RGB Linear",
    kD50,
    {{0.7347, 0.2653}, {0.1596, 0.8404}, {0.0366, 0.0001}},
    TransferCurve::kLinear,
    1.0,
};

inline constexpr RgbColorSpace kAcesAp0Linear{
    "aces_ap0_linear",
    "ACES AP0 Linear",
    kD60,
    {{0.73470, 0.26530}, {0.00000, 1.00000}, {0.00010, -0.07700}},
    TransferCurve::kLinear,
    1.0,
};

inline constexpr RgbColorSpace kAcesCgLinear{
    "acescg_linear",
    "ACEScg AP1 Linear",
    kD60,
    {{0.7130, 0.2930}, {0.1650, 0.8300}, {0.1280, 0.0440}},
    TransferCurve::kLinear,
    1.0,
};

inline constexpr std::array<const RgbColorSpace*, 9> kExportColorSpaces{
    &kSrgb,
    &kDisplayP3,
    &kP3D65Gamma26,
    &kRec709Gamma24,
    &kAdobeRgb,
    &kRec2020Linear,
    &kProPhotoLinear,
    &kAcesAp0Linear,
    &kAcesCgLinear,
};

inline const RgbColorSpace* LookupColorSpace(std::string_view id) {
    for (const RgbColorSpace* space : kExportColorSpaces) {
        if (space->id == id) return space;
    }
    return nullptr;
}

inline Vec3 XyzFromXy(Xy p) {
    if (std::abs(p.y) < 1e-12) return {0.0, 0.0, 0.0};
    return {p.x / p.y, 1.0, (1.0 - p.x - p.y) / p.y};
}

inline Vec3 Mul(const Mat3& m, const Vec3& v) {
    return {
        m[0] * v[0] + m[1] * v[1] + m[2] * v[2],
        m[3] * v[0] + m[4] * v[1] + m[5] * v[2],
        m[6] * v[0] + m[7] * v[1] + m[8] * v[2],
    };
}

inline Mat3 Mul(const Mat3& a, const Mat3& b) {
    Mat3 out = {};
    for (int row = 0; row < 3; ++row) {
        for (int col = 0; col < 3; ++col) {
            out[row * 3 + col] =
                a[row * 3 + 0] * b[0 * 3 + col] +
                a[row * 3 + 1] * b[1 * 3 + col] +
                a[row * 3 + 2] * b[2 * 3 + col];
        }
    }
    return out;
}

inline Mat3 Invert(const Mat3& m) {
    const double a = m[0], b = m[1], c = m[2];
    const double d = m[3], e = m[4], f = m[5];
    const double g = m[6], h = m[7], i = m[8];
    const double det =
        a * (e * i - f * h) - b * (d * i - f * g) +
        c * (d * h - e * g);
    if (std::abs(det) < 1e-20) return {};
    const double inv = 1.0 / det;
    return {
         (e * i - f * h) * inv, -(b * i - c * h) * inv,  (b * f - c * e) * inv,
        -(d * i - f * g) * inv,  (a * i - c * g) * inv, -(a * f - c * d) * inv,
         (d * h - e * g) * inv, -(a * h - b * g) * inv,  (a * e - b * d) * inv,
    };
}

inline Mat3 RgbToXyzMatrix(const RgbColorSpace& space) {
    const Vec3 r = XyzFromXy(space.primaries.red);
    const Vec3 g = XyzFromXy(space.primaries.green);
    const Vec3 b = XyzFromXy(space.primaries.blue);
    const Mat3 primaries = {
        r[0], g[0], b[0],
        r[1], g[1], b[1],
        r[2], g[2], b[2],
    };
    const Vec3 white = XyzFromXy(space.white);
    const Vec3 scale = Mul(Invert(primaries), white);
    return {
        primaries[0] * scale[0], primaries[1] * scale[1], primaries[2] * scale[2],
        primaries[3] * scale[0], primaries[4] * scale[1], primaries[5] * scale[2],
        primaries[6] * scale[0], primaries[7] * scale[1], primaries[8] * scale[2],
    };
}

inline Mat3 BradfordAdaptation(Xy src_white, Xy dst_white) {
    constexpr Mat3 kBradford = {
         0.8951000,  0.2664000, -0.1614000,
        -0.7502000,  1.7135000,  0.0367000,
         0.0389000, -0.0685000,  1.0296000,
    };
    constexpr Mat3 kBradfordInv = {
         0.9869929, -0.1470543,  0.1599627,
         0.4323053,  0.5183603,  0.0492912,
        -0.0085287,  0.0400428,  0.9684867,
    };
    const Vec3 src_cone = Mul(kBradford, XyzFromXy(src_white));
    const Vec3 dst_cone = Mul(kBradford, XyzFromXy(dst_white));
    const Mat3 scale = {
        dst_cone[0] / src_cone[0], 0.0, 0.0,
        0.0, dst_cone[1] / src_cone[1], 0.0,
        0.0, 0.0, dst_cone[2] / src_cone[2],
    };
    return Mul(kBradfordInv, Mul(scale, kBradford));
}

inline Mat3 LinearSrgbToColorSpaceMatrix(const RgbColorSpace& dst) {
    const Mat3 srgb_to_xyz = RgbToXyzMatrix(kSrgb);
    const Mat3 adapt = BradfordAdaptation(kSrgb.white, dst.white);
    const Mat3 dst_xyz_to_rgb = Invert(RgbToXyzMatrix(dst));
    return Mul(dst_xyz_to_rgb, Mul(adapt, srgb_to_xyz));
}

inline double Clamp01(double value) {
    if (value < 0.0) return 0.0;
    if (value > 1.0) return 1.0;
    return value;
}

inline double EncodeTransfer(double linear, const RgbColorSpace& dst) {
    linear = Clamp01(linear);
    switch (dst.transfer) {
        case TransferCurve::kLinear:
            return linear;
        case TransferCurve::kSrgb:
            if (linear <= 0.0031308) return 12.92 * linear;
            return 1.055 * std::pow(linear, 1.0 / 2.4) - 0.055;
        case TransferCurve::kGamma:
            return std::pow(linear, 1.0 / dst.gamma);
    }
    return linear;
}

inline Vec3 ConvertLinearSrgbToEncoded(const RgbColorSpace& dst, Vec3 rgb) {
    const Vec3 target_linear = Mul(LinearSrgbToColorSpaceMatrix(dst), rgb);
    return {
        EncodeTransfer(target_linear[0], dst),
        EncodeTransfer(target_linear[1], dst),
        EncodeTransfer(target_linear[2], dst),
    };
}

std::vector<unsigned char> CreateIccProfile(const RgbColorSpace& space);

}  // namespace softloaf::trichrome::color
