#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "desktop_decoder.hpp"

namespace fs = std::filesystem;
namespace desktop = softloaf::trichrome::desktop;
namespace tri = softloaf::trichrome;

namespace {

struct Row {
    std::string label;
    fs::path relative_path;
    std::string target;
    std::string expect_ok;
    std::string expect_reason;
    std::string expect_class;
    std::string expect_policy;
    std::string expect_hints_contains;
    std::string expect_matrix_source;
    std::string decode_target;
    int expect_decode_width = 0;
    int expect_decode_height = 0;
};

std::vector<std::string> SplitTab(const std::string& line) {
    std::vector<std::string> out;
    std::stringstream ss(line);
    std::string item;
    while (std::getline(ss, item, '\t')) out.push_back(item);
    return out;
}

std::vector<Row> ReadManifest(const fs::path& path) {
    std::ifstream in(path);
    assert(in.is_open());
    std::vector<Row> rows;
    std::string line;
    bool header = true;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        if (header) {
            header = false;
            continue;
        }
        const std::vector<std::string> cols = SplitTab(line);
        assert(cols.size() >= 12);
        Row row;
        row.label = cols[0];
        row.relative_path = cols[1];
        row.target = cols[2];
        row.expect_ok = cols[3];
        row.expect_reason = cols[4];
        row.expect_class = cols[5];
        row.expect_policy = cols[6];
        row.expect_hints_contains = cols[7];
        row.expect_matrix_source = cols[8];
        row.decode_target = cols[9];
        row.expect_decode_width = std::stoi(cols[10]);
        row.expect_decode_height = std::stoi(cols[11]);
        rows.push_back(row);
    }
    return rows;
}

tri::RawDecodeTarget TargetFor(const std::string& target) {
    if (target == "camera-native") return tri::RawDecodeTarget::kCameraNativeLinear;
    assert(target == "rec2020");
    return tri::RawDecodeTarget::kLinearRec2020;
}

void AssertContainsAll(const std::string& value, const std::string& expected) {
    if (expected == "none") {
        assert(value == "none");
        return;
    }
    std::stringstream ss(expected);
    std::string token;
    while (std::getline(ss, token, ';')) {
        assert(!token.empty());
        assert(value.find(token) != std::string::npos);
    }
}

void AssertDecode(const fs::path& path, const Row& row) {
    if (row.decode_target == "none") return;
    tri::ImageBuf image;
    if (row.decode_target == "camera-native") {
        image = desktop::DecodeLinear(path, false, desktop::DecodeMode::kExport);
    } else {
        assert(row.decode_target == "rec2020");
        image = desktop::DecodeRawToLinearRec2020(
            path, desktop::DecodeMode::kExport, false);
    }
    assert(!image.empty());
    assert(image.width() == row.expect_decode_width);
    assert(image.height() == row.expect_decode_height);
}

}  // namespace

int main() {
    const fs::path default_root =
        "/Volumes/T7 Touch/Film/RAW/raw_pixls_us_camera_only_rsync";
    const char* env_root = std::getenv("SOFTLOAF_TRICHROME_RAW_FIXTURE_ROOT");
    const fs::path root = env_root && *env_root ? fs::path(env_root) : default_root;
    if (!fs::is_directory(root)) {
        std::cout << "SKIP raw_fixture_regression: fixture root missing: "
                  << root << "\n";
        return 0;
    }

#if defined(SOFTLOAF_TRICHROME_RAW_FIXTURE_MANIFEST)
    const fs::path manifest = SOFTLOAF_TRICHROME_RAW_FIXTURE_MANIFEST;
#else
    const fs::path manifest =
        fs::path("tests") / "fixtures" / "raw_compat_manifest.tsv";
#endif
    const std::vector<Row> rows = ReadManifest(manifest);
    assert(!rows.empty());

    for (const Row& row : rows) {
        const fs::path path = root / row.relative_path;
        if (!fs::is_regular_file(path)) {
            std::cerr << "missing fixture for " << row.label << ": "
                      << path << "\n";
            return 1;
        }
        const desktop::RawProvenanceProbeResult result =
            desktop::ProbeRawProvenance(
                path, desktop::DecodeMode::kExport, TargetFor(row.target));
        assert((result.ok ? "ok" : "blocked") == row.expect_ok);
        assert(result.reason == row.expect_reason);
        if (row.expect_class != "none") {
            assert(result.provenance.present);
            assert(std::string(tri::RawSensorClassName(result.provenance.raw_class)) ==
                   row.expect_class);
            assert(std::string(tri::RawLinearRec2020PolicyName(result.provenance.policy)) ==
                   row.expect_policy);
        } else {
            assert(!result.provenance.present);
        }
        AssertContainsAll(result.sensor_hints, row.expect_hints_contains);
        assert(result.matrix_source == row.expect_matrix_source);
        AssertDecode(path, row);
    }

    std::cout << "PASS raw_fixture_regression rows=" << rows.size()
              << " root=" << root << "\n";
    return 0;
}
