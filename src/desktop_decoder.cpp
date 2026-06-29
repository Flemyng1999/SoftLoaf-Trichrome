#include "desktop_decoder.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <memory>
#include <string>
#include <vector>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <dlfcn.h>
#endif

#include <libraw/libraw.h>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include "softloaf_trichrome/model.hpp"
#include "softloaf_trichrome/raw_levels.hpp"
#include "qt_path_utils.hpp"

namespace softloaf::trichrome::desktop {
namespace {

std::string LowerExt(const std::filesystem::path& path) {
    std::string ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return ext;
}

float SrgbToLinear(float v) {
    v = std::clamp(v, 0.0f, 1.0f);
    if (v <= 0.04045f) return v / 12.92f;
    return std::pow((v + 0.055f) / 1.055f, 2.4f);
}

void ConvertSrgbMatToLinear(cv::Mat* image) {
    if (!image || image->empty() || image->depth() != CV_32F) return;
    const int channels = image->channels();
    for (int y = 0; y < image->rows; ++y) {
        float* row = image->ptr<float>(y);
        for (int x = 0; x < image->cols * channels; ++x)
            row[x] = SrgbToLinear(row[x]);
    }
}

std::array<double, 9> CameraToXyzD50(const LibRaw& raw, bool* ok) {
    std::array<double, 9> m = {};
    bool any = false;
    for (int r = 0; r < 3; ++r) {
        for (int c = 0; c < 3; ++c) {
            const double v = raw.imgdata.color.cam_xyz[r][c];
            m[static_cast<size_t>(r * 3 + c)] = v;
            any = any || std::abs(v) > 1e-12;
        }
    }
    if (ok) *ok = any;
    return m;
}

const char* DecodeModeName(DecodeMode decode_mode) {
    return decode_mode == DecodeMode::kPreview ? "preview" : "export";
}

void ConfigureRawDecodeParams(LibRaw* raw, DecodeMode decode_mode) {
    raw->imgdata.params.output_color = 0;
    raw->imgdata.params.output_bps = 16;
    raw->imgdata.params.no_auto_bright = 1;
    raw->imgdata.params.use_auto_wb = 0;
    raw->imgdata.params.use_camera_wb = 0;
    raw->imgdata.params.gamm[0] = 1.0;
    raw->imgdata.params.gamm[1] = 1.0;
    for (float& v : raw->imgdata.params.user_mul) v = 1.0f;

    const auto& color = raw->imgdata.color;
    const auto selected_white = softloaf::trichrome::SelectTrustedRawWhiteLevel(
        {color.black,
         {color.linear_max[0], color.linear_max[1],
          color.linear_max[2], color.linear_max[3]},
         color.maximum,
         color.data_maximum});
    if (selected_white)
        raw->imgdata.params.user_sat = static_cast<int>(*selected_white);

    if (decode_mode == DecodeMode::kPreview) {
        raw->imgdata.params.half_size = 1;
        raw->imgdata.params.no_interpolation = 1;
    } else {
        raw->imgdata.params.half_size = 0;
        raw->imgdata.params.no_interpolation = 0;
    }
}

void ConfigureRawRec2020Params(LibRaw* raw,
                               DecodeMode decode_mode,
                               bool use_camera_wb) {
    ConfigureRawDecodeParams(raw, decode_mode);
    raw->imgdata.params.output_color = 5;
    raw->imgdata.params.use_camera_wb = use_camera_wb ? 1 : 0;
    raw->imgdata.params.use_auto_wb = 0;
}

cv::Vec3f XyzToLinearRec2020(const cv::Vec3f& xyz) {
    return {
        static_cast<float>( 1.7166511880 * xyz[0] - 0.3556707838 * xyz[1] - 0.2533662814 * xyz[2]),
        static_cast<float>(-0.6666843518 * xyz[0] + 1.6164812366 * xyz[1] + 0.0157685458 * xyz[2]),
        static_cast<float>( 0.0176398574 * xyz[0] - 0.0427706133 * xyz[1] + 0.9421031212 * xyz[2]),
    };
}

void ConvertXyzMatToLinearRec2020(cv::Mat* image) {
    if (!image || image->empty() || image->depth() != CV_32F || image->channels() < 3)
        return;
    for (int y = 0; y < image->rows; ++y) {
        auto* row = image->ptr<cv::Vec3f>(y);
        for (int x = 0; x < image->cols; ++x)
            row[x] = XyzToLinearRec2020(row[x]);
    }
}

void CropRawTherapeeActiveArea(cv::Mat* image) {
    if (!image || image->cols <= 8 || image->rows <= 8) return;
    *image = (*image)(cv::Rect(4, 4, image->cols - 8, image->rows - 8)).clone();
}

void LimitOpenMpForPreviewDecode(DecodeMode decode_mode) {
    if (decode_mode != DecodeMode::kPreview) return;
    using OmpSetNumThreads = void (*)(int);
#if defined(_WIN32)
    static const auto omp_set_num_threads = []() -> OmpSetNumThreads {
        static constexpr const char* kOpenMpModules[] = {
            "vcomp140.dll",
            "libomp.dll",
            "libiomp5md.dll",
        };
        for (const char* module_name : kOpenMpModules) {
            HMODULE module = GetModuleHandleA(module_name);
            if (!module) continue;
            FARPROC proc = GetProcAddress(module, "omp_set_num_threads");
            if (proc) return reinterpret_cast<OmpSetNumThreads>(proc);
        }
        return nullptr;
    }();
#else
    static const auto omp_set_num_threads = reinterpret_cast<OmpSetNumThreads>(
        dlsym(RTLD_DEFAULT, "omp_set_num_threads"));
#endif
    if (omp_set_num_threads) omp_set_num_threads(1);
}

ImageBuf DecodeRegularImage(const std::filesystem::path& path, bool force_mono) {
    ImageBuf out;
    const QByteArray bytes = ReadFileBytes(path);
    if (bytes.isEmpty()) return out;
    const auto* first = reinterpret_cast<const unsigned char*>(bytes.constData());
    std::vector<unsigned char> encoded(first, first + bytes.size());
    cv::Mat input = cv::imdecode(encoded, cv::IMREAD_UNCHANGED);
    if (input.empty()) return out;

    cv::Mat float_img;
    const double scale = input.depth() == CV_8U ? 1.0 / 255.0
        : input.depth() == CV_16U ? 1.0 / 65535.0
        : 1.0;
    input.convertTo(float_img, CV_32F, scale);

    if (float_img.channels() == 4) {
        cv::cvtColor(float_img, float_img, cv::COLOR_BGRA2BGR);
    }
    ConvertSrgbMatToLinear(&float_img);
    if (force_mono) {
        if (float_img.channels() == 1) {
            out.data = float_img;
        } else {
            cv::cvtColor(float_img, out.data, cv::COLOR_BGR2GRAY);
        }
    } else {
        if (float_img.channels() == 1) {
            cv::cvtColor(float_img, out.data, cv::COLOR_GRAY2RGB);
        } else {
            cv::cvtColor(float_img, out.data, cv::COLOR_BGR2RGB);
        }
    }
    out.state = ColorState::kWorkingLinear;
    out.color_space = "linear_srgb";
    return out;
}

ImageBuf DecodeRawImage(const std::filesystem::path& path,
                        bool force_mono,
                        DecodeMode decode_mode) {
    ImageBuf out;
    // Keep LibRaw off the stack; large RAW decode paths can be stack-hungry.
    auto raw = std::make_unique<LibRaw>();
#if defined(_WIN32)
    if (raw->open_file(path.wstring().c_str()) != LIBRAW_SUCCESS) return out;
#else
    const std::string raw_path = QStringFromPath(path).toUtf8().toStdString();
    if (raw->open_file(raw_path.c_str()) != LIBRAW_SUCCESS) return out;
#endif
    if (raw->unpack() != LIBRAW_SUCCESS) return out;
    out.camera_to_xyz_d50 = CameraToXyzD50(*raw, &out.has_camera_to_xyz_d50);

    ConfigureRawDecodeParams(raw.get(), decode_mode);
    LimitOpenMpForPreviewDecode(decode_mode);

    if (raw->dcraw_process() != LIBRAW_SUCCESS) return out;
    int err = LIBRAW_SUCCESS;
    libraw_processed_image_t* img = raw->dcraw_make_mem_image(&err);
    if (!img || err != LIBRAW_SUCCESS) return out;

    if (img->type == LIBRAW_IMAGE_BITMAP && img->bits == 16 && img->colors >= 1) {
        const int h = static_cast<int>(img->height);
        const int w = static_cast<int>(img->width);
        const int colors = static_cast<int>(img->colors);
        cv::Mat src16(h, w, CV_MAKETYPE(CV_16U, colors), img->data);
        cv::Mat f32;
        src16.convertTo(f32, CV_32F, 1.0 / 65535.0);
        if (force_mono) {
            if (colors == 1) {
                out.data = f32.clone();
            } else {
                cv::cvtColor(f32, out.data, cv::COLOR_RGB2GRAY);
            }
        } else {
            if (colors == 1) {
                cv::cvtColor(f32, out.data, cv::COLOR_GRAY2RGB);
            } else if (colors >= 3) {
                std::vector<cv::Mat> channels;
                cv::split(f32, channels);
                cv::merge(std::vector<cv::Mat>{channels[0], channels[1], channels[2]}, out.data);
            }
        }
        out.state = ColorState::kCameraLinear;
        out.color_space = std::string("camera_native_linear_") + DecodeModeName(decode_mode);
    }

    LibRaw::dcraw_clear_mem(img);
    return out;
}

ImageBuf DecodeRawRec2020Image(const std::filesystem::path& path,
                               DecodeMode decode_mode,
                               bool use_camera_wb) {
    ImageBuf out;
    auto raw = std::make_unique<LibRaw>();
#if defined(_WIN32)
    if (raw->open_file(path.wstring().c_str()) != LIBRAW_SUCCESS) return out;
#else
    const std::string raw_path = QStringFromPath(path).toUtf8().toStdString();
    if (raw->open_file(raw_path.c_str()) != LIBRAW_SUCCESS) return out;
#endif
    if (raw->unpack() != LIBRAW_SUCCESS) return out;
    ConfigureRawRec2020Params(raw.get(), decode_mode, use_camera_wb);
    LimitOpenMpForPreviewDecode(decode_mode);
    if (raw->dcraw_process() != LIBRAW_SUCCESS) return out;

    int err = LIBRAW_SUCCESS;
    libraw_processed_image_t* img = raw->dcraw_make_mem_image(&err);
    if (!img || err != LIBRAW_SUCCESS) return out;

    if (img->type == LIBRAW_IMAGE_BITMAP && img->bits == 16 && img->colors >= 3) {
        const int h = static_cast<int>(img->height);
        const int w = static_cast<int>(img->width);
        const int colors = static_cast<int>(img->colors);
        cv::Mat src16(h, w, CV_MAKETYPE(CV_16U, colors), img->data);
        cv::Mat f32;
        src16.convertTo(f32, CV_32F, 1.0 / 65535.0);
        std::vector<cv::Mat> channels;
        cv::split(f32, channels);
        cv::merge(std::vector<cv::Mat>{channels[0], channels[1], channels[2]}, out.data);
        ConvertXyzMatToLinearRec2020(&out.data);
        CropRawTherapeeActiveArea(&out.data);
        out.state = ColorState::kWorkingLinear;
        out.color_space = "rec_2020_linear";
    }

    LibRaw::dcraw_clear_mem(img);
    return out;
}

}  // namespace

bool LooksLikeRaw(const std::filesystem::path& path) {
    return IsRawLikeExtension(LowerExt(path));
}

ImageBuf DecodeLinear(const std::filesystem::path& path,
                      bool force_mono,
                      DecodeMode decode_mode) {
    if (LooksLikeRaw(path)) {
        return DecodeRawImage(path, force_mono, decode_mode);
    }
    return DecodeRegularImage(path, force_mono);
}

ImageBuf DecodeRawToLinearRec2020(const std::filesystem::path& path,
                                  DecodeMode decode_mode,
                                  bool use_camera_wb) {
    if (!LooksLikeRaw(path)) return {};
    return DecodeRawRec2020Image(path, decode_mode, use_camera_wb);
}

}  // namespace softloaf::trichrome::desktop
