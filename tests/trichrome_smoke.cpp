#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>
#include <unordered_map>

#include "softloaf_trichrome/composer.hpp"
#include "softloaf_trichrome/import.hpp"
#include "softloaf_trichrome/structure.hpp"

namespace fs = std::filesystem;
namespace tri = softloaf::trichrome;

namespace {

tri::ImageBuf Mono(int rows, int cols, float value) {
    tri::ImageBuf image;
    image.data = cv::Mat(rows, cols, CV_32FC1, cv::Scalar(value));
    image.state = tri::ColorState::kCameraLinear;
    return image;
}

tri::ImageBuf Pattern() {
    tri::ImageBuf image;
    image.data = cv::Mat(64, 64, CV_32FC1, cv::Scalar(0.0f));
    cv::rectangle(image.data, cv::Rect(8, 10, 20, 12), cv::Scalar(0.8f), cv::FILLED);
    cv::circle(image.data, cv::Point(40, 35), 8, cv::Scalar(0.4f), cv::FILLED);
    image.state = tri::ColorState::kCameraLinear;
    return image;
}

fs::path Touch(const fs::path& path, const std::string& bytes) {
    fs::create_directories(path.parent_path());
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    f << bytes;
    return path;
}

}  // namespace

int main() {
    const fs::path root = fs::temp_directory_path() / "softloaf_trichrome_smoke";
    std::error_code ec;
    fs::remove_all(root, ec);
    fs::create_directories(root, ec);

    Touch(root / "001_R.raf", "red");
    Touch(root / "002_G.raf", "green");
    Touch(root / "003_B.raf", "blue");
    tri::TrichromeImportRequest request;
    request.folder = root;
    request.sensor_type = tri::InputSensorType::kMono;
    request.artifact_dir = "trichrome";
    tri::TrichromeImportPlan plan = tri::TrichromeImportService().FromRequest(request);
    assert(plan.ok());
    assert(plan.report.valid_group_count == 1);

    tri::ProjectMeta meta = plan.recipe.ToProjectMeta();
    assert(meta.trichrome_groups.size() == 1);
    meta.trichrome_roll_white = {1.0, 1.0, 1.0};
    meta.trichrome_roll_white_valid = true;

    const std::unordered_map<std::string, tri::ImageBuf> images = {
        {(root / "001_R.raf").string(), Mono(2, 2, 0.25f)},
        {(root / "002_G.raf").string(), Mono(2, 2, 0.50f)},
        {(root / "003_B.raf").string(), Mono(2, 2, 0.75f)},
    };
    const auto decoder = [&images](const fs::path& path) {
        const auto it = images.find(path.string());
        return it == images.end() ? tri::ImageBuf{} : it->second;
    };
    const tri::ComposeResult composed = tri::ComposeGroup(meta.trichrome_groups[0], decoder);
    assert(composed.ok);
    const cv::Vec3f px = composed.rgb.data.at<cv::Vec3f>(0, 0);
    assert(px[0] == 0.25f && px[1] == 0.50f && px[2] == 0.75f);

    const tri::ArtifactBuildResult built = tri::BuildTrichromeFullArtifact(
        &meta, 0, root / "bundle", decoder);
    assert(built.ok);
    assert(fs::exists(built.artifact_path));
    assert(!meta.trichrome_groups[0].artifact_sig.empty());

    const tri::ImageBuf pattern = Pattern();
    tri::ProjectTrichromeGroup group;
    group.sources = {{"red", "R.raw"}, {"green", "G.raw"}, {"blue", "B.raw"}};
    const tri::StructureReport structure = tri::VerifyTrichromeStructure(
        group, [pattern](const std::string&) { return pattern; });
    assert(structure.ok);

    fs::remove_all(root, ec);
    return 0;
}
