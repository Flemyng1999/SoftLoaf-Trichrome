#pragma once

#include <memory>

#include <opencv2/core.hpp>

#include "softloaf_trichrome/color_management.hpp"
#include "softloaf_trichrome/image_buf.hpp"

namespace softloaf::trichrome::desktop {

class IExportEncodeBackend {
 public:
    virtual ~IExportEncodeBackend() = default;

    virtual const char* name() const { return "cpu"; }
    virtual bool EncodeRgbx64(const ImageBuf& image,
                              const color::RgbColorSpace& dst,
                              cv::Mat* out_rgbx64) const {
        if (out_rgbx64) out_rgbx64->release();
        (void)image;
        (void)dst;
        return false;
    }
};

std::unique_ptr<IExportEncodeBackend> MakeCpuExportEncodeBackend();
std::unique_ptr<IExportEncodeBackend> MakeExportEncodeBackend();
IExportEncodeBackend& DefaultExportEncodeBackend();

#if defined(__APPLE__)
std::unique_ptr<IExportEncodeBackend> MakeMetalExportEncodeBackend();
#endif

#if defined(_WIN32)
std::unique_ptr<IExportEncodeBackend> MakeD3D12ExportEncodeBackend();
#endif

}  // namespace softloaf::trichrome::desktop
