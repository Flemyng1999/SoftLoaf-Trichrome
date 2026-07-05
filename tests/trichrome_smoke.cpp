#include <cassert>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <string>
#include <unordered_map>

#include "softloaf_trichrome/composer.hpp"
#include "softloaf_trichrome/color_management.hpp"
#include "softloaf_trichrome/import.hpp"
#include "softloaf_trichrome/raw_camera_matrix.hpp"
#include "softloaf_trichrome/raw_classification.hpp"
#include "softloaf_trichrome/raw_decode_hints.hpp"
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

tri::ImageBuf MonoWithRawProvenance(int rows,
                                    int cols,
                                    float value,
                                    tri::RawSensorClass raw_class) {
    tri::ImageBuf image = Mono(rows, cols, value);
    image.raw_provenance = tri::MakeRawDecodeProvenance(
        raw_class, tri::RawDecodeMode::kExport,
        tri::RawDecodeTarget::kCameraNativeLinear);
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

void TestRawSensorClassificationBoundaries() {
    tri::RawClassificationInput bayer;
    bayer.filters = 0x94949494;
    bayer.colors = 3;
    assert(tri::ClassifyRawSensor(bayer) == tri::RawSensorClass::kBayerOrOtherCfa);
    assert(tri::LinearRec2020PolicyFor(tri::ClassifyRawSensor(bayer)) ==
           tri::RawLinearRec2020Policy::kSupported);

    tri::RawClassificationInput xtrans;
    xtrans.filters = 9;
    xtrans.colors = 3;
    assert(tri::ClassifyRawSensor(xtrans) == tri::RawSensorClass::kXTrans);
    assert(tri::LinearRec2020PolicyFor(tri::ClassifyRawSensor(xtrans)) ==
           tri::RawLinearRec2020Policy::kFallbackOnly);

    tri::RawClassificationInput packed_rgb;
    packed_rgb.filters = 0;
    packed_rgb.colors = 3;
    packed_rgb.has_color3_image = true;
    assert(tri::ClassifyRawSensor(packed_rgb) == tri::RawSensorClass::kPackedRgb);
    assert(tri::LinearRec2020PolicyFor(tri::ClassifyRawSensor(packed_rgb)) ==
           tri::RawLinearRec2020Policy::kFallbackOnly);

    tri::RawClassificationInput packed_four;
    packed_four.filters = 0;
    packed_four.colors = 4;
    packed_four.has_float4_image = true;
    assert(tri::ClassifyRawSensor(packed_four) == tri::RawSensorClass::kPackedFourColor);
    assert(tri::LinearRec2020PolicyFor(tri::ClassifyRawSensor(packed_four)) ==
           tri::RawLinearRec2020Policy::kFallbackOnly);

    tri::RawClassificationInput packed_four_sony_color4;
    packed_four_sony_color4.filters = 0;
    packed_four_sony_color4.colors = 3;
    packed_four_sony_color4.has_color4_image = true;
    assert(tri::ClassifyRawSensor(packed_four_sony_color4) ==
           tri::RawSensorClass::kPackedFourColor);
    assert(tri::RawDecodePolicyAllowsTarget(
        tri::LinearRec2020PolicyFor(tri::ClassifyRawSensor(packed_four_sony_color4)),
        tri::RawDecodeTarget::kCameraNativeLinear));

    tri::RawClassificationInput mono;
    mono.filters = 0;
    mono.colors = 1;
    assert(tri::ClassifyRawSensor(mono) == tri::RawSensorClass::kMonochrome);
    assert(tri::LinearRec2020PolicyFor(tri::ClassifyRawSensor(mono)) ==
           tri::RawLinearRec2020Policy::kUnsupported);

    tri::RawClassificationInput foveon;
    foveon.filters = 0;
    foveon.colors = 3;
    foveon.is_foveon = true;
    foveon.has_color3_image = true;
    assert(tri::ClassifyRawSensor(foveon) == tri::RawSensorClass::kFoveon);
    assert(tri::LinearRec2020PolicyFor(tri::ClassifyRawSensor(foveon)) ==
           tri::RawLinearRec2020Policy::kUnsupported);

    tri::RawClassificationInput unknown;
    unknown.filters = 0;
    unknown.colors = 2;
    assert(tri::ClassifyRawSensor(unknown) == tri::RawSensorClass::kUnknownFiltersZero);
    assert(tri::LinearRec2020PolicyFor(tri::ClassifyRawSensor(unknown)) ==
           tri::RawLinearRec2020Policy::kUnsupported);
}

void TestLeicaSl2CameraMatrixOverride() {
    const auto matrix = tri::LeicaSl2CameraToXyzD50Override("Leica Camera AG", "SL2");
    assert(matrix.has_value());
    AssertMatrixClose(
        *matrix,
        {0.5098656859, 0.3875956683, 0.0667586458,
         0.2104797198, 0.8198900124, -0.0303697322,
         0.0180066179, -0.0945648437, 0.9017682258},
        2e-8);
    assert(tri::LeicaSl2CameraToXyzD50Override("Leica Camera AG", "SL2-S") ==
           std::nullopt);
}

void TestRawDecodeCameraHints() {
    tri::RawDecodeHintInput sony_packed;
    sony_packed.make = "SONY";
    sony_packed.model = "ILCE-7M4";
    sony_packed.filters = 0;
    sony_packed.colors = 3;
    sony_packed.has_raw_image = false;
    sony_packed.has_color4_image = true;
    sony_packed.black = 1024;
    assert(tri::IsSonyPackedFullColorColor4(sony_packed));
    assert(tri::SonyPackedFullColorBlackHint(sony_packed).value() == 512u);
    assert(tri::RawDecodeHintSummary(sony_packed) == "sony_packed_color4_black_v1");

    tri::RawDecodeHintInput sony_control = sony_packed;
    sony_control.has_raw_image = true;
    assert(!tri::SonyPackedFullColorBlackHint(sony_control).has_value());

    tri::RawDecodeHintInput canon70d;
    canon70d.make = "Canon";
    canon70d.model = "EOS 70D";
    canon70d.filters = 0;
    canon70d.colors = 3;
    canon70d.has_color4_image = true;
    assert(tri::Canon70DReducedRawWhiteHint(canon70d).value() == 13480u);

    tri::RawDecodeHintInput canon_control = canon70d;
    canon_control.model = "EOS 5D Mark IV";
    assert(!tri::Canon70DReducedRawWhiteHint(canon_control).has_value());

    tri::RawDecodeHintInput a1m2;
    a1m2.make = "Sony";
    a1m2.model = "ILCE-1M2";
    a1m2.raw_width = 5632;
    a1m2.raw_height = 4096;
    const auto crop = tri::ProcessedRawCropHint(a1m2);
    assert(crop.has_value());
    assert(crop->width == 5628);
    assert(crop->height == 3756);
}

void TestRawDecodeProvenanceMapping() {
    const tri::RawDecodeProvenance bayer = tri::MakeRawDecodeProvenance(
        tri::RawSensorClass::kBayerOrOtherCfa, tri::RawDecodeMode::kExport,
        "rec_2020_linear");
    assert(bayer.present);
    assert(bayer.policy == tri::RawLinearRec2020Policy::kSupported);
    assert(bayer.fallback_status == tri::RawDecodeFallbackStatus::kNone);
    assert(bayer.target_color_space == "rec_2020_linear");
    assert(tri::RawDecodePolicyAllowsTarget(
        bayer.policy, tri::RawDecodeTarget::kLinearRec2020));
    assert(tri::RawDecodeProvenanceSignature(bayer).find("class=bayer_or_other_cfa") !=
           std::string::npos);

    const tri::RawDecodeProvenance xtrans = tri::MakeRawDecodeProvenance(
        tri::RawSensorClass::kXTrans, tri::RawDecodeMode::kPreview,
        "camera_native_linear");
    assert(xtrans.policy == tri::RawLinearRec2020Policy::kFallbackOnly);
    assert(xtrans.fallback_status == tri::RawDecodeFallbackStatus::kFallbackOnly);
    assert(tri::RawDecodePolicyAllowsTarget(
        xtrans.policy, tri::RawDecodeTarget::kCameraNativeLinear));
    assert(!tri::RawDecodePolicyAllowsTarget(
        xtrans.policy, tri::RawDecodeTarget::kLinearRec2020));
    assert(std::string(tri::RawDecodePolicyTargetReason(
        xtrans.policy, tri::RawDecodeTarget::kLinearRec2020)) ==
        "raw_fallback_only_not_rec2020");
    assert(tri::RawDecodeProvenanceSignature(xtrans) !=
           tri::RawDecodeProvenanceSignature(bayer));

    const tri::RawDecodeProvenance foveon = tri::MakeRawDecodeProvenance(
        tri::RawSensorClass::kFoveon, tri::RawDecodeMode::kExport,
        "rec_2020_linear");
    assert(foveon.policy == tri::RawLinearRec2020Policy::kUnsupported);
    assert(foveon.fallback_status == tri::RawDecodeFallbackStatus::kUnsupported);
    assert(!tri::RawDecodePolicyAllowsTarget(
        foveon.policy, tri::RawDecodeTarget::kCameraNativeLinear));
    assert(std::string(tri::RawDecodePolicyTargetReason(
        foveon.policy, tri::RawDecodeTarget::kCameraNativeLinear)) ==
        "raw_unsupported");
}

void TestArtifactIdentityIncludesRawProvenancePolicy() {
    tri::ProjectTrichromeGroup group;
    group.mode = "trichrome";
    group.sensor_type = "bayer";
    group.strict_order = "RGB";
    group.validation_status = "valid";
    group.group_size = 3;
    group.artifact_format = tri::kDefaultArtifactFormat;
    group.artifact_pixel_type = "uint16";
    group.artifact_width = 32;
    group.artifact_height = 24;
    group.artifact_channel_order = "RGB";
    group.artifact_color_state = tri::kArtifactColorState;
    group.compose_algo_version = tri::kComposeAlgoVersion;
    group.input_preprocess_sig = tri::RawDecodePipelineIdentity();
    group.sources = {{"red", "R.raw"}, {"green", "G.raw"}, {"blue", "B.raw"}};
    for (tri::ProjectTrichromeSource& source : group.sources) {
        source.probe_ok = true;
        source.probe_size = 4;
        source.probe_mtime = 123;
        source.probe_partial_hash = 456;
    }
    const std::array<double, 3> white = {1.0, 1.0, 1.0};
    const std::string sig = tri::ComputeArtifactSignature(
        group, tri::ArtifactTier::kFull, white);
    group.input_preprocess_sig = "raw-provenance-test:changed";
    assert(tri::ComputeArtifactSignature(group, tri::ArtifactTier::kFull, white) != sig);
}

void TestImportSkipsAppleDoubleRawSidecars() {
    const fs::path root = fs::temp_directory_path() / "softloaf_phase_one_sidecars";
    std::error_code ec;
    fs::remove_all(root, ec);
    fs::create_directories(root, ec);

    Touch(root / "._Pro-Capture One 14170.dng", "appledouble red");
    Touch(root / "._Pro-Capture One 14171.dng", "appledouble green");
    Touch(root / "._Pro-Capture One 14172.dng", "appledouble blue");
    Touch(root / "Pro-Capture One 14170.dng", "red dng");
    Touch(root / "Pro-Capture One 14171.dng", "green dng");
    Touch(root / "Pro-Capture One 14172.dng", "blue dng");
    Touch(root / "Pro-Capture One 14170.iiq", "native iiq red");
    Touch(root / "Pro-Capture One 14171.iiq", "native iiq green");
    Touch(root / "Pro-Capture One 14172.iiq", "native iiq blue");
    Touch(root / "SDIM0555.X3F", "foveon boundary");

    assert(!tri::IsImportableRawPath(root / "._Pro-Capture One 14170.dng"));
    assert(tri::IsImportableRawPath(root / "Pro-Capture One 14170.dng"));
    assert(!tri::IsImportableRawPath(root / "Pro-Capture One 14170.iiq"));
    assert(tri::IsImportableRawPath(root / "SDIM0555.X3F"));
    assert(tri::DetectDominantRawSuffix(root) == ".dng");
    assert(tri::SupportedImagesFileFilter().find("*.x3f") != std::string::npos);
    assert(tri::SupportedImagesFileFilter().find("*.iiq") == std::string::npos);

    tri::TrichromeImportRequest request;
    request.folder = root;
    request.sensor_type = tri::InputSensorType::kBayer;
    const tri::TrichromeImportPlan plan =
        tri::TrichromeImportService().FromRequest(request);
    assert(plan.ok());
    assert(plan.report.file_count == 3);
    assert(plan.report.valid_group_count == 1);
    assert(plan.report.groups.size() == 1);
    assert(plan.report.groups[0].sources.size() == 3);
    assert(plan.report.groups[0].sources[0].path.filename() ==
           fs::path("Pro-Capture One 14170.dng"));
    assert(plan.report.groups[0].sources[1].path.filename() ==
           fs::path("Pro-Capture One 14171.dng"));
    assert(plan.report.groups[0].sources[2].path.filename() ==
           fs::path("Pro-Capture One 14172.dng"));

    fs::remove_all(root, ec);
}

}  // namespace

int main() {
    TestColourScienceReferenceMatrices();
    TestLargeExportSpacesRemainLinear();
    TestRawSensorClassificationBoundaries();
    TestLeicaSl2CameraMatrixOverride();
    TestRawDecodeCameraHints();
    TestRawDecodeProvenanceMapping();
    TestArtifactIdentityIncludesRawProvenancePolicy();
    TestImportSkipsAppleDoubleRawSidecars();

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
    assert(meta.trichrome_groups[0].input_preprocess_sig == tri::RawDecodePipelineIdentity());

    tri::ProjectMeta meta_with_provenance = plan.recipe.ToProjectMeta();
    meta_with_provenance.trichrome_roll_white = {1.0, 1.0, 1.0};
    meta_with_provenance.trichrome_roll_white_valid = true;
    const std::unordered_map<std::string, tri::ImageBuf> raw_images = {
        {(root / "001_R.raf").string(),
         MonoWithRawProvenance(2, 2, 0.25f, tri::RawSensorClass::kBayerOrOtherCfa)},
        {(root / "002_G.raf").string(),
         MonoWithRawProvenance(2, 2, 0.50f, tri::RawSensorClass::kXTrans)},
        {(root / "003_B.raf").string(),
         MonoWithRawProvenance(2, 2, 0.75f, tri::RawSensorClass::kPackedRgb)},
    };
    const auto raw_decoder = [&raw_images](const fs::path& path) {
        const auto it = raw_images.find(path.string());
        return it == raw_images.end() ? tri::ImageBuf{} : it->second;
    };
    const tri::ComposeResult raw_composed =
        tri::ComposeGroup(meta_with_provenance.trichrome_groups[0], raw_decoder);
    assert(raw_composed.ok);
    assert(raw_composed.source_provenance_sigs.size() == 3);
    assert(raw_composed.source_provenance_records.size() == 3);
    const tri::ArtifactBuildResult raw_built = tri::BuildTrichromeFullArtifact(
        &meta_with_provenance, 0, root / "bundle_provenance", raw_decoder);
    assert(raw_built.ok);
    assert(meta_with_provenance.trichrome_groups[0].input_preprocess_sig.find(
               "raw-source-provenance-v1") != std::string::npos);
    assert(meta_with_provenance.trichrome_groups[0].input_preprocess_sig.find(
               "class=xtrans") != std::string::npos);
    const auto& provenance_sources = meta_with_provenance.trichrome_groups[0].sources;
    assert(provenance_sources.size() == 3);
    assert(provenance_sources[0].raw_class == "bayer_or_other_cfa");
    assert(provenance_sources[0].raw_policy == "supported");
    assert(provenance_sources[1].raw_class == "xtrans");
    assert(provenance_sources[1].raw_fallback_status == "fallback_only");
    assert(provenance_sources[2].raw_class == "packed_rgb");
    assert(provenance_sources[2].raw_target_color_space == "camera_native_linear");
    assert(!provenance_sources[2].raw_provenance_sig.empty());
    assert(meta_with_provenance.trichrome_groups[0].artifact_sig !=
           meta.trichrome_groups[0].artifact_sig);

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
    levels.linear_max = {12000, 12000, 12000, 12000};
    const auto rt_like_white = tri::SelectTrustedRawWhiteLevel(levels);
    assert(rt_like_white.has_value());
    assert(*rt_like_white == 16383);

    fs::remove_all(root, ec);
    return 0;
}
