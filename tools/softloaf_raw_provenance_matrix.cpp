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

    std::cout << "input,target,status,reason,raw_sensor_hints,raw_class,raw_policy,"
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
