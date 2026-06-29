#include <cassert>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <string>
#include <unordered_map>

#include "softloaf_trichrome/composer.hpp"
#include "softloaf_trichrome/color_management.hpp"
#include "softloaf_trichrome/import.hpp"
#include "softloaf_trichrome/raw_levels.hpp"
#include "softloaf_trichrome/structure.hpp"

namespace fs = std::filesystem;
namespace tri = softloaf::trichrome;
namespace color = softloaf::trichrome::color;

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

void AssertMatrixClose(const color::Mat3& actual,
                       const color::Mat3& expected,
                       double tolerance) {
    for (size_t i = 0; i < actual.size(); ++i)
        assert(std::abs(actual[i] - expected[i]) <= tolerance);
}

void TestColourScienceReferenceMatrices() {
    // Reference values generated with colour-science 0.4.7:
    // colour.matrix_RGB_to_RGB(sRGB, target, chromatic_adaptation_transform="Bradford").
    AssertMatrixClose(color::LinearSrgbToColorSpaceMatrix(color::kAcesAp0Linear),
                      {0.439643004019961, 0.383005471371792, 0.177399308886895,
                       0.089715731865361, 0.813475053791709, 0.096782252404812,
                       0.017512720476296, 0.111551438549134, 0.870882792975247},
                      3e-4);
    AssertMatrixClose(color::LinearSrgbToColorSpaceMatrix(color::kAcesCgLinear),
                      {0.613132422390542, 0.339538015799666, 0.047416696048269,
                       0.070124380833917, 0.916394011313573, 0.013451523958235,
                       0.020587657528185, 0.109574571610682, 0.869785404035327},
                      3e-4);
    AssertMatrixClose(color::LinearSrgbToColorSpaceMatrix(color::kRec2020Linear),
                      {0.627441372057978, 0.329297459521909, 0.043351458394495,
                       0.069027617147078, 0.919580666887028, 0.011361422575401,
                       0.016364235071681, 0.088017162471727, 0.895564972725983},
                      3e-4);
    AssertMatrixClose(color::LinearSrgbToColorSpaceMatrix(color::kProPhotoLinear),
                      {0.529388470711727, 0.330231217729904, 0.140627167763333,
                       0.098305088514438, 0.873483901238103, 0.028146056090101,
                       0.016851063807660, 0.117696843742385, 0.865673268565668},
                      3e-4);
}

void TestLargeExportSpacesRemainLinear() {
    assert(color::kAcesAp0Linear.transfer == color::TransferCurve::kLinear);
    assert(color::kAcesCgLinear.transfer == color::TransferCurve::kLinear);
    assert(color::kRec2020Linear.transfer == color::TransferCurve::kLinear);
    assert(color::kProPhotoLinear.transfer == color::TransferCurve::kLinear);
}

}  // namespace

int main() {
    TestColourScienceReferenceMatrices();
    TestLargeExportSpacesRemainLinear();

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

    tri::RawWhiteLevelInputs levels;
    levels.black = 511;
    levels.linear_max = {153, 153, 153, 153};
    levels.maximum = 16383;
    const auto white = tri::SelectTrustedRawWhiteLevel(levels);
    assert(white.has_value());
    assert(*white == 16383);

    fs::remove_all(root, ec);
    return 0;
}
