#include "export_encode_backend.hpp"

#include <algorithm>
#include <cstring>
#include <cstdio>
#include <string_view>

#if !__has_feature(objc_arc)
#error "export_encode_backend_metal.mm requires ARC"
#endif

#import <Foundation/Foundation.h>
#import <Metal/Metal.h>

namespace softloaf::trichrome::desktop {
namespace {

struct EncodeParams {
    uint32_t npx = 0;
    uint32_t transfer = 0;
    float gamma = 1.0f;
    float pad = 0.0f;
    float m[9] = {};
};

uint32_t TransferCode(color::TransferCurve transfer) {
    switch (transfer) {
        case color::TransferCurve::kLinear:
            return 0;
        case color::TransferCurve::kSrgb:
            return 1;
        case color::TransferCurve::kGamma:
            return 2;
    }
    return 0;
}

EncodeParams ParamsFor(const color::RgbColorSpace& dst, uint32_t npx) {
    EncodeParams p;
    p.npx = npx;
    p.transfer = TransferCode(dst.transfer);
    p.gamma = static_cast<float>(dst.gamma);
    const color::Mat3 matrix = color::LinearSrgbToColorSpaceMatrix(dst);
    for (size_t i = 0; i < matrix.size(); ++i) p.m[i] = static_cast<float>(matrix[i]);
    return p;
}

NSString* KernelSource() {
    return @R"MSL(
#include <metal_stdlib>
using namespace metal;

struct EncodeParams {
    uint npx;
    uint transfer;
    float gamma;
    float pad;
    float m[9];
};

inline float clamp01(float v) {
    return clamp(v, 0.0f, 1.0f);
}

inline float encode_transfer(float linear, constant EncodeParams& p) {
    linear = clamp01(linear);
    if (p.transfer == 1u) {
        if (linear <= 0.0031308f) return 12.92f * linear;
        return 1.055f * pow(linear, 1.0f / 2.4f) - 0.055f;
    }
    if (p.transfer == 2u) {
        return pow(linear, 1.0f / p.gamma);
    }
    return linear;
}

kernel void encode_rgbx64(device const float* in_rgb [[buffer(0)]],
                          device ushort4* out_rgbx64 [[buffer(1)]],
                          constant EncodeParams& p [[buffer(2)]],
                          uint gid [[thread_position_in_grid]]) {
    if (gid >= p.npx) return;
    const uint b = gid * 3u;
    const float r = in_rgb[b + 0u];
    const float g = in_rgb[b + 1u];
    const float bl = in_rgb[b + 2u];
    const float tr = p.m[0] * r + p.m[1] * g + p.m[2] * bl;
    const float tg = p.m[3] * r + p.m[4] * g + p.m[5] * bl;
    const float tb = p.m[6] * r + p.m[7] * g + p.m[8] * bl;
    const float er = encode_transfer(tr, p);
    const float eg = encode_transfer(tg, p);
    const float eb = encode_transfer(tb, p);
    out_rgbx64[gid] = ushort4(ushort(clamp01(er) * 65535.0f + 0.5f),
                              ushort(clamp01(eg) * 65535.0f + 0.5f),
                              ushort(clamp01(eb) * 65535.0f + 0.5f),
                              ushort(65535));
}
)MSL";
}

class MetalExportEncodeBackend final : public IExportEncodeBackend {
 public:
    MetalExportEncodeBackend() {
        @autoreleasepool {
            device_ = MTLCreateSystemDefaultDevice();
            if (!device_) return;
            queue_ = [device_ newCommandQueue];
            if (!queue_) return;
            NSError* error = nil;
            id<MTLLibrary> library = [device_ newLibraryWithSource:KernelSource()
                                                           options:nil
                                                             error:&error];
            if (!library) {
                std::fprintf(stderr, "Metal export encode unavailable: %s\n",
                             error ? error.localizedDescription.UTF8String : "library compile failed");
                return;
            }
            id<MTLFunction> function = [library newFunctionWithName:@"encode_rgbx64"];
            if (!function) return;
            pso_ = [device_ newComputePipelineStateWithFunction:function error:&error];
            if (!pso_) {
                std::fprintf(stderr, "Metal export encode PSO unavailable: %s\n",
                             error ? error.localizedDescription.UTF8String : "pipeline compile failed");
                return;
            }
        }
    }

    const char* name() const override { return pso_ ? "metal" : "metal_unavailable"; }

    bool EncodeRgbx64(const ImageBuf& image,
                      const color::RgbColorSpace& dst,
                      cv::Mat* out_rgbx64) const override {
        if (out_rgbx64) out_rgbx64->release();
        if (!out_rgbx64 || !pso_ || image.empty() ||
            image.data.type() != CV_32FC3 || image.width() <= 0 || image.height() <= 0) {
            return false;
        }
        const cv::Mat input = image.data.isContinuous() ? image.data : image.data.clone();
        const uint32_t npx = static_cast<uint32_t>(image.width()) *
                             static_cast<uint32_t>(image.height());
        if (npx == 0) return false;
        const size_t input_bytes = static_cast<size_t>(npx) * 3 * sizeof(float);
        const size_t output_bytes = static_cast<size_t>(npx) * 4 * sizeof(uint16_t);
        EncodeParams params = ParamsFor(dst, npx);

        @autoreleasepool {
            id<MTLBuffer> in = [device_ newBufferWithBytes:input.ptr()
                                                     length:input_bytes
                                                    options:MTLResourceStorageModeShared];
            id<MTLBuffer> out = [device_ newBufferWithLength:output_bytes
                                                     options:MTLResourceStorageModeShared];
            if (!in || !out) return false;

            id<MTLCommandBuffer> cb = [queue_ commandBuffer];
            id<MTLComputeCommandEncoder> enc = [cb computeCommandEncoder];
            if (!cb || !enc) return false;
            [enc setComputePipelineState:pso_];
            [enc setBuffer:in offset:0 atIndex:0];
            [enc setBuffer:out offset:0 atIndex:1];
            [enc setBytes:&params length:sizeof(params) atIndex:2];
            const NSUInteger w = pso_.threadExecutionWidth;
            const MTLSize tg = MTLSizeMake(w, 1, 1);
            const MTLSize grid = MTLSizeMake(npx, 1, 1);
            [enc dispatchThreads:grid threadsPerThreadgroup:tg];
            [enc endEncoding];
            [cb commit];
            [cb waitUntilCompleted];
            if (cb.status != MTLCommandBufferStatusCompleted) return false;

            cv::Mat encoded(image.height(), image.width(), CV_16UC4);
            std::memcpy(encoded.ptr(), [out contents], output_bytes);
            *out_rgbx64 = std::move(encoded);
            return true;
        }
    }

 private:
    id<MTLDevice> device_ = nil;
    id<MTLCommandQueue> queue_ = nil;
    id<MTLComputePipelineState> pso_ = nil;
};

}  // namespace

std::unique_ptr<IExportEncodeBackend> MakeMetalExportEncodeBackend() {
    auto backend = std::make_unique<MetalExportEncodeBackend>();
    if (std::string_view(backend->name()) == "metal") return backend;
    return nullptr;
}

}  // namespace softloaf::trichrome::desktop
