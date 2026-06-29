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
                 "[--space camera-native|rec2020] [--wb camera|none]\n";
}

}  // namespace

int main(int argc, char** argv) {
    fs::path input;
    fs::path output;
    std::string space = "rec2020";
    std::string wb = "camera";
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
        } else {
            Usage();
            return 2;
        }
    }
    if (input.empty() || output.empty()) {
        Usage();
        return 2;
    }

    softloaf::trichrome::ImageBuf image =
        space == "camera-native"
            ? desktop::DecodeLinear(input, false, desktop::DecodeMode::kExport)
            : desktop::DecodeRawToLinearRec2020(
                  input, desktop::DecodeMode::kExport, wb != "none");
    if (image.empty() || image.data.depth() != CV_32F || image.data.channels() != 3) {
        std::cerr << "decode_failed," << input.string() << "\n";
        return 1;
    }

    std::error_code ec;
    if (!output.parent_path().empty()) fs::create_directories(output.parent_path(), ec);
    std::vector<unsigned char> encoded;
    if (!cv::imencode(".tiff", image.data, encoded) ||
        !desktop::WriteFileBytes(output, QByteArray(
            reinterpret_cast<const char*>(encoded.data()),
            static_cast<qsizetype>(encoded.size())))) {
        std::cerr << "write_failed," << output.string() << "\n";
        return 1;
    }

    std::cout << "input,output,width,height,color_state,color_space\n"
              << input.string() << "," << output.string() << ","
              << image.width() << "," << image.height() << ","
              << (space == "camera-native" ? "camera_linear" : "working_linear") << ","
              << image.color_space << "\n";
    return 0;
}
