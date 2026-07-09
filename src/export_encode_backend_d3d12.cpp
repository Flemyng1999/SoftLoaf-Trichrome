#include "export_encode_backend.hpp"

#if defined(_WIN32)

// Windows D3D12 implementation placeholder.
//
// This file intentionally preserves the same factory seam as the Metal backend
// and Negative's D3D12 apply backend: app code asks for an optional GPU encode
// capability and falls back to CPU when construction or execution is unavailable.
// The actual HLSL compute path should mirror export_encode_backend_metal.mm:
// upload CV_32FC3, apply the CPU color matrix/transfer constants, write
// CV_16UC4-compatible RGBX64, read back for the TIFF writer boundary.

namespace softloaf::trichrome::desktop {

std::unique_ptr<IExportEncodeBackend> MakeD3D12ExportEncodeBackend() {
    return nullptr;
}

}  // namespace softloaf::trichrome::desktop

#endif
