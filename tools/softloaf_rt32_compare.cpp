#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <cmath>
#include <iterator>
#include <optional>
#include <string>
#include <vector>

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

namespace fs = std::filesystem;

namespace {

void Usage() {
    std::cerr << "usage: softloaf_rt32_compare --oracle-dir <dir> "
                 "--project-dir <dir> --out-csv <csv>\n";
}

std::string StemBeforeRt32(const fs::path& path) {
    std::string name = path.filename().string();
    const std::string marker = ".rt32.";
    const size_t pos = name.find(marker);
    return pos == std::string::npos ? path.stem().string() : name.substr(0, pos);
}

std::optional<fs::path> FindProject(const fs::path& dir, const std::string& stem) {
    std::error_code ec;
    for (const fs::directory_entry& entry : fs::directory_iterator(dir, ec)) {
        if (!entry.is_regular_file()) continue;
        const std::string name = entry.path().filename().string();
        if (name.rfind(stem + ".project.", 0) == 0) return entry.path();
    }
    return std::nullopt;
}

std::string Csv(const std::string& value) {
    std::string out = "\"";
    for (char c : value) {
        if (c == '"') out += "\"\"";
        else out += c;
    }
    out += '"';
    return out;
}

cv::Mat ReadFloatRgb(const fs::path& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return {};
    std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(f)),
                                     std::istreambuf_iterator<char>());
    cv::Mat image = cv::imdecode(bytes, cv::IMREAD_UNCHANGED);
    if (image.empty()) return {};
    if (image.channels() == 1) cv::cvtColor(image, image, cv::COLOR_GRAY2RGB);
    if (image.channels() > 3) {
        std::vector<cv::Mat> channels;
        cv::split(image, channels);
        cv::merge(std::vector<cv::Mat>{channels[0], channels[1], channels[2]}, image);
    }
    if (image.depth() != CV_32F) {
        const double scale = image.depth() == CV_16U ? 1.0 / 65535.0
                           : image.depth() == CV_8U ? 1.0 / 255.0
                           : 1.0;
        image.convertTo(image, CV_32F, scale);
    }
    return image;
}

cv::Mat CenterCrop(const cv::Mat& image, int width, int height, int* x, int* y) {
    *x = std::max(0, (image.cols - width) / 2);
    *y = std::max(0, (image.rows - height) / 2);
    return image(cv::Rect(*x, *y, width, height)).clone();
}

double MeanAbs(const cv::Mat& a, const cv::Mat& b) {
    cv::Mat diff;
    cv::absdiff(a, b, diff);
    const cv::Scalar mean = cv::mean(diff);
    return (mean[0] + mean[1] + mean[2]) / 3.0;
}

void CompareRow(const fs::path& oracle,
                const fs::path& project,
                std::ofstream& csv) {
    cv::Mat o = ReadFloatRgb(oracle);
    cv::Mat p = ReadFloatRgb(project);
    const int oracle_width = o.cols;
    const int oracle_height = o.rows;
    const int project_width = p.cols;
    const int project_height = p.rows;
    if (o.empty() || p.empty()) {
        csv << Csv(StemBeforeRt32(oracle)) << ',' << Csv(oracle.string()) << ','
            << Csv(project.string())
            << ",failed,,,,,,,,could_not_read_image\n";
        return;
    }

    const int width = std::min(o.cols, p.cols);
    const int height = std::min(o.rows, p.rows);
    int ox = 0, oy = 0, px = 0, py = 0;
    o = CenterCrop(o, width, height, &ox, &oy);
    p = CenterCrop(p, width, height, &px, &py);

    const double sigma = std::max(1.0, std::min(width, height) / 512.0);
    cv::GaussianBlur(o, o, cv::Size(), sigma, sigma);
    cv::GaussianBlur(p, p, cv::Size(), sigma, sigma);

    const double low_mean_abs = MeanAbs(o, p);
    const cv::Scalar om = cv::mean(o);
    const cv::Scalar pm = cv::mean(p);
    std::vector<cv::Mat> channels;
    cv::split(p, channels);
    double ratios[3] = {};
    for (int c = 0; c < 3; ++c) {
        ratios[c] = std::abs(om[c]) > 1e-12 ? pm[c] / om[c] : 0.0;
        const double gain = std::abs(pm[c]) > 1e-12 ? om[c] / pm[c] : 1.0;
        channels[static_cast<size_t>(c)] *= gain;
    }
    cv::Mat corrected;
    cv::merge(channels, corrected);
    const double gain_mean_abs = MeanAbs(o, corrected);

    csv << Csv(StemBeforeRt32(oracle)) << ',' << Csv(oracle.string()) << ','
        << Csv(project.string()) << ",ok,"
        << oracle_width << ',' << oracle_height << ','
        << project_width << ',' << project_height << ','
        << "\"oracle_x=" << ox << ";oracle_y=" << oy
        << ";project_x=" << px << ";project_y=" << py << "\","
        << std::setprecision(9) << low_mean_abs << ','
        << gain_mean_abs << ','
        << '"' << ratios[0] << ';' << ratios[1] << ';' << ratios[2] << "\",\n";
}

}  // namespace

int main(int argc, char** argv) {
    fs::path oracle_dir;
    fs::path project_dir;
    fs::path out_csv;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--oracle-dir" && i + 1 < argc) oracle_dir = argv[++i];
        else if (arg == "--project-dir" && i + 1 < argc) project_dir = argv[++i];
        else if (arg == "--out-csv" && i + 1 < argc) out_csv = argv[++i];
        else {
            Usage();
            return 2;
        }
    }
    if (oracle_dir.empty() || project_dir.empty() || out_csv.empty()) {
        Usage();
        return 2;
    }

    std::ofstream csv(out_csv, std::ios::trunc);
    csv << "sample,oracle,project,status,oracle_width,oracle_height,"
           "project_width,project_height,crop_offset,low_mean_abs,"
           "gain_mean_abs,channel_ratios,failure_reason\n";

    std::error_code ec;
    for (const fs::directory_entry& entry : fs::directory_iterator(oracle_dir, ec)) {
        if (!entry.is_regular_file()) continue;
        const std::string name = entry.path().filename().string();
        if (name.find(".rt32.tif") == std::string::npos) continue;
        const std::string stem = StemBeforeRt32(entry.path());
        const auto project = FindProject(project_dir, stem);
        if (!project) {
            csv << Csv(stem) << ',' << Csv(entry.path().string())
                << ",,missing_project,,,,,,,,missing_project\n";
            continue;
        }
        CompareRow(entry.path(), *project, csv);
    }

    return 0;
}
