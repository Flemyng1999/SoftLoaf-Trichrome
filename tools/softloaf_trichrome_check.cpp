#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "softloaf_trichrome/composer.hpp"
#include "softloaf_trichrome/import.hpp"

namespace fs = std::filesystem;
namespace tri = softloaf::trichrome;

namespace {

tri::ImageBuf Mono(float value) {
    tri::ImageBuf image;
    image.data = cv::Mat(2, 2, CV_32FC1, cv::Scalar(value));
    image.state = tri::ColorState::kCameraLinear;
    return image;
}

int SyntheticOracle() {
    const fs::path root = fs::temp_directory_path() / "softloaf_trichrome_oracle";
    std::error_code ec;
    fs::remove_all(root, ec);
    fs::create_directories(root, ec);
    if (ec) {
        std::cerr << "create temp dir failed\n";
        return 1;
    }
    const auto touch = [](const fs::path& path) {
        std::ofstream f(path, std::ios::binary);
        f << path.filename().string();
    };
    touch(root / "001_R.raf");
    touch(root / "002_G.raf");
    touch(root / "003_B.raf");

    tri::TrichromeImportRequest request;
    request.folder = root;
    request.name = "synthetic";
    request.sensor_type = tri::InputSensorType::kMono;
    request.artifact_dir = "trichrome";
    tri::TrichromeImportPlan plan = tri::TrichromeImportService().FromRequest(request);
    if (!plan.ok()) {
        std::cerr << plan.report.ToStableJson() << "\n";
        return 1;
    }
    tri::ProjectMeta meta = plan.recipe.ToProjectMeta();
    meta.trichrome_roll_white = {1.0, 1.0, 1.0};
    meta.trichrome_roll_white_valid = true;
    const tri::ArtifactBuildResult built = tri::BuildTrichromeFullArtifact(
        &meta, 0, root / "bundle", [](const fs::path& path) {
            const std::string name = path.filename().string();
            if (name.find("_R") != std::string::npos) return Mono(0.25f);
            if (name.find("_G") != std::string::npos) return Mono(0.50f);
            return Mono(0.75f);
        });
    if (!built.ok) {
        std::cerr << built.reason << "\n";
        return 1;
    }
    std::cout << plan.report.ToStableJson() << "\n";
    std::cout << "artifact=" << built.artifact_path << "\n";
    return 0;
}

void Usage() {
    std::cerr << "usage: softloaf_trichrome_check --synthetic-mono-oracle\n"
              << "       softloaf_trichrome_check --folder <dir>\n"
              << "       softloaf_trichrome_check --file <path> [--file <path>...]\n";
}

}  // namespace

int main(int argc, char** argv) {
    if (argc == 2 && std::string(argv[1]) == "--synthetic-mono-oracle")
        return SyntheticOracle();

    tri::TrichromeImportRequest request;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--folder" && i + 1 < argc) {
            request.folder = argv[++i];
        } else if (arg == "--file" && i + 1 < argc) {
            request.files.emplace_back(argv[++i]);
            request.sort_policy = tri::TrichromeSortPolicy::kSelectionOrder;
        } else {
            Usage();
            return 2;
        }
    }
    if (request.folder.empty() && request.files.empty()) {
        Usage();
        return 2;
    }
    const tri::TrichromeImportPlan plan = tri::TrichromeImportService().FromRequest(request);
    std::cout << plan.report.ToStableJson() << "\n";
    return plan.ok() ? 0 : 1;
}
