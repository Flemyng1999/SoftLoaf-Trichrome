#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#include <opencv2/imgcodecs.hpp>

#include "desktop_decoder.hpp"
#include "qt_path_utils.hpp"

namespace fs = std::filesystem;
namespace desktop = softloaf::trichrome::desktop;

namespace {

void Usage() {
    std::cerr << "usage: softloaf_raw_probe --input <raw> --output <float-tiff> "
                 "[--space camera-native|rec2020] [--wb camera|none] "
                 "[--provenance-only]\n";
}

softloaf::trichrome::RawDecodeTarget TargetForSpace(const std::string& space) {
    return space == "camera-native"
        ? softloaf::trichrome::RawDecodeTarget::kCameraNativeLinear
        : softloaf::trichrome::RawDecodeTarget::kLinearRec2020;
}

void PrintProbeCsv(const fs::path& input,
                   const fs::path& output,
                   const std::string& status,
                   const std::string& reason,
                   int width,
                   int height,
                   const std::string& color_state,
                   const std::string& color_space,
                   const std::string& sensor_hints,
                   const desktop::RawProvenanceProbeResult& probe,
                   const softloaf::trichrome::RawDecodeProvenance& provenance) {
    std::cout << "input,output,status,reason,width,height,color_state,color_space,"
                 "raw_sensor_hints,raw_make,raw_model,raw_matrix_source,"
                 "raw_filters,raw_colors,raw_has_raw_image,raw_has_color3_image,"
                 "raw_has_color4_image,raw_has_float3_image,raw_has_float4_image,"
                 "raw_black,raw_cblack0,raw_cblack1,raw_cblack2,raw_cblack3,"
                 "raw_linear_max0,raw_linear_max1,raw_linear_max2,raw_linear_max3,"
                 "raw_maximum,raw_data_maximum,raw_width,raw_height,"
                 "raw_visible_width,raw_visible_height,"
                 "raw_class,raw_policy,raw_decode_mode,raw_fallback_status,"
                 "raw_target_color_space,raw_provenance_sig\n"
              << input.string() << "," << output.string() << ","
              << status << "," << reason << ","
              << width << "," << height << ","
              << color_state << "," << color_space << ","
              << sensor_hints << ","
              << probe.make << "," << probe.model << "," << probe.matrix_source << ","
              << probe.filters << "," << probe.colors << ","
              << (probe.has_raw_image ? 1 : 0) << ","
              << (probe.has_color3_image ? 1 : 0) << ","
              << (probe.has_color4_image ? 1 : 0) << ","
              << (probe.has_float3_image ? 1 : 0) << ","
              << (probe.has_float4_image ? 1 : 0) << ","
              << probe.black << ","
              << probe.cblack[0] << "," << probe.cblack[1] << ","
              << probe.cblack[2] << "," << probe.cblack[3] << ","
              << probe.linear_max[0] << "," << probe.linear_max[1] << ","
              << probe.linear_max[2] << "," << probe.linear_max[3] << ","
              << probe.maximum << "," << probe.data_maximum << ","
              << probe.raw_width << "," << probe.raw_height << ","
              << probe.visible_width << "," << probe.visible_height << ","
              << softloaf::trichrome::RawSensorClassName(provenance.raw_class) << ","
              << softloaf::trichrome::RawLinearRec2020PolicyName(provenance.policy) << ","
              << softloaf::trichrome::RawDecodeModeName(provenance.decode_mode) << ","
              << softloaf::trichrome::RawDecodeFallbackStatusName(provenance.fallback_status) << ","
              << provenance.target_color_space << ","
              << softloaf::trichrome::RawDecodeProvenanceSignature(provenance)
              << "\n";
}

}  // namespace

int main(int argc, char** argv) {
    fs::path input;
    fs::path output;
    std::string space = "rec2020";
    std::string wb = "camera";
    bool provenance_only = false;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--input" && i + 1 < argc) {
            input = argv[++i];
        } else if (arg == "--output" && i + 1 < argc) {
            output = argv[++i];
        } else if (arg == "--space" && i + 1 < argc) {
            space = argv[++i];
        } else if (arg == "--wb" && i + 1 < argc) {
            wb = argv[++i];
        } else if (arg == "--provenance-only") {
            provenance_only = true;
        } else {
            Usage();
            return 2;
        }
    }
    if (input.empty() || (!provenance_only && output.empty())) {
        Usage();
        return 2;
    }
    if (space != "camera-native" && space != "rec2020") {
        Usage();
        return 2;
    }

    const softloaf::trichrome::RawDecodeTarget target = TargetForSpace(space);
    const desktop::RawProvenanceProbeResult probe =
        desktop::ProbeRawProvenance(input, desktop::DecodeMode::kExport, target);
    if (provenance_only) {
        PrintProbeCsv(input, output, probe.ok ? "ok" : "blocked", probe.reason,
                      0, 0, "", "", probe.sensor_hints, probe, probe.provenance);
        return probe.provenance.present ? 0 : 1;
    }

    softloaf::trichrome::ImageBuf image =
        space == "camera-native"
            ? desktop::DecodeLinear(input, false, desktop::DecodeMode::kExport)
            : desktop::DecodeRawToLinearRec2020(
                  input, desktop::DecodeMode::kExport, wb != "none");
    if (image.empty() || image.data.depth() != CV_32F || image.data.channels() != 3) {
        PrintProbeCsv(input, output, "decode_failed",
                      probe.reason == "ok" ? "decode_failed_after_probe" : probe.reason,
                      0, 0, "", "", probe.sensor_hints, probe, probe.provenance);
        return 1;
    }

    std::error_code ec;
    if (!output.parent_path().empty()) fs::create_directories(output.parent_path(), ec);
    std::vector<unsigned char> encoded;
    if (!cv::imencode(".tiff", image.data, encoded) ||
        !desktop::WriteFileBytes(output, QByteArray(
            reinterpret_cast<const char*>(encoded.data()),
            static_cast<qsizetype>(encoded.size())))) {
        PrintProbeCsv(input, output, "write_failed", "write_failed",
                      image.width(), image.height(),
                      space == "camera-native" ? "camera_linear" : "working_linear",
                      image.color_space, probe.sensor_hints, probe, image.raw_provenance);
        return 1;
    }

    PrintProbeCsv(input, output, "ok", "ok", image.width(), image.height(),
                  space == "camera-native" ? "camera_linear" : "working_linear",
                  image.color_space, probe.sensor_hints, probe, image.raw_provenance);
    return 0;
}
