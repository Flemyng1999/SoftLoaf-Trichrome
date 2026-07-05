#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#include "desktop_decoder.hpp"

namespace fs = std::filesystem;
namespace desktop = softloaf::trichrome::desktop;
namespace tri = softloaf::trichrome;

namespace {

void Usage() {
    std::cerr << "usage: softloaf_raw_provenance_matrix <raw> [<raw> ...]\n";
}

void PrintRow(const fs::path& path,
              tri::RawDecodeTarget target,
              const desktop::RawProvenanceProbeResult& result) {
    const auto& p = result.provenance;
    std::cout << path.string() << ","
              << tri::RawDecodeTargetColorSpaceName(target) << ","
              << (result.ok ? "ok" : "blocked") << ","
              << result.reason << ","
              << result.sensor_hints << ","
              << result.make << ","
              << result.model << ","
              << result.matrix_source << ","
              << result.filters << ","
              << result.colors << ","
              << (result.has_raw_image ? 1 : 0) << ","
              << (result.has_color3_image ? 1 : 0) << ","
              << (result.has_color4_image ? 1 : 0) << ","
              << (result.has_float3_image ? 1 : 0) << ","
              << (result.has_float4_image ? 1 : 0) << ","
              << result.black << ","
              << result.cblack[0] << ","
              << result.cblack[1] << ","
              << result.cblack[2] << ","
              << result.cblack[3] << ","
              << result.linear_max[0] << ","
              << result.linear_max[1] << ","
              << result.linear_max[2] << ","
              << result.linear_max[3] << ","
              << result.maximum << ","
              << result.data_maximum << ","
              << result.raw_width << ","
              << result.raw_height << ","
              << result.visible_width << ","
              << result.visible_height << ","
              << tri::RawSensorClassName(p.raw_class) << ","
              << tri::RawLinearRec2020PolicyName(p.policy) << ","
              << tri::RawDecodeModeName(p.decode_mode) << ","
              << tri::RawDecodeFallbackStatusName(p.fallback_status) << ","
              << p.target_color_space << ","
              << tri::RawDecodeProvenanceSignature(p) << "\n";
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        Usage();
        return 2;
    }

    std::cout << "input,target,status,reason,raw_sensor_hints,"
                 "raw_make,raw_model,raw_matrix_source,"
                 "raw_filters,raw_colors,raw_has_raw_image,raw_has_color3_image,"
                 "raw_has_color4_image,raw_has_float3_image,raw_has_float4_image,"
                 "raw_black,raw_cblack0,raw_cblack1,raw_cblack2,raw_cblack3,"
                 "raw_linear_max0,raw_linear_max1,raw_linear_max2,raw_linear_max3,"
                 "raw_maximum,raw_data_maximum,raw_width,raw_height,"
                 "raw_visible_width,raw_visible_height,"
                 "raw_class,raw_policy,"
                 "raw_decode_mode,raw_fallback_status,raw_target_color_space,"
                 "raw_provenance_sig\n";
    int failures = 0;
    for (int i = 1; i < argc; ++i) {
        const fs::path path = argv[i];
        for (const tri::RawDecodeTarget target :
             {tri::RawDecodeTarget::kLinearRec2020,
              tri::RawDecodeTarget::kCameraNativeLinear}) {
            const desktop::RawProvenanceProbeResult result =
                desktop::ProbeRawProvenance(
                    path, desktop::DecodeMode::kExport, target);
            PrintRow(path, target, result);
            if (!result.provenance.present) ++failures;
        }
    }
    return failures == 0 ? 0 : 1;
}
