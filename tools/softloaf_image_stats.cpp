#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

namespace fs = std::filesystem;

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "usage: softloaf_image_stats <image> [image...]\n";
        return 2;
    }
    std::cout << "image,width,height,min0,min1,min2,max0,max1,max2,mean0,mean1,mean2,over1_frac\n";
    for (int i = 1; i < argc; ++i) {
        const fs::path path = argv[i];
        std::ifstream f(path, std::ios::binary);
        std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(f)),
                                         std::istreambuf_iterator<char>());
        cv::Mat image = cv::imdecode(bytes, cv::IMREAD_UNCHANGED);
        if (image.empty()) {
            std::cout << path.string() << ",failed\n";
            continue;
        }
        if (image.channels() > 3) {
            std::vector<cv::Mat> channels;
            cv::split(image, channels);
            cv::merge(std::vector<cv::Mat>{channels[0], channels[1], channels[2]}, image);
        }
        if (image.channels() == 1) cv::cvtColor(image, image, cv::COLOR_GRAY2RGB);
        if (image.depth() != CV_32F) {
            const double scale = image.depth() == CV_16U ? 1.0 / 65535.0
                               : image.depth() == CV_8U ? 1.0 / 255.0
                               : 1.0;
            image.convertTo(image, CV_32F, scale);
        }
        std::vector<cv::Mat> ch;
        cv::split(image, ch);
        double mn[3] = {}, mx[3] = {};
        for (int c = 0; c < 3; ++c) cv::minMaxLoc(ch[static_cast<size_t>(c)], &mn[c], &mx[c]);
        const cv::Scalar mean = cv::mean(image);
        uint64_t over = 0;
        for (int y = 0; y < image.rows; ++y) {
            const auto* row = image.ptr<cv::Vec3f>(y);
            for (int x = 0; x < image.cols; ++x)
                if (row[x][0] > 1.0f || row[x][1] > 1.0f || row[x][2] > 1.0f) ++over;
        }
        const double total = static_cast<double>(image.rows) * image.cols;
        std::cout << path.string() << ',' << image.cols << ',' << image.rows
                  << ',' << mn[0] << ',' << mn[1] << ',' << mn[2]
                  << ',' << mx[0] << ',' << mx[1] << ',' << mx[2]
                  << ',' << mean[0] << ',' << mean[1] << ',' << mean[2]
                  << ',' << (total > 0.0 ? over / total : 0.0) << '\n';
    }
    return 0;
}
