#include "export_encode_backend.hpp"

namespace softloaf::trichrome::desktop {
namespace {

class CpuExportEncodeBackend final : public IExportEncodeBackend {
 public:
    const char* name() const override { return "cpu"; }
};

}  // namespace

std::unique_ptr<IExportEncodeBackend> MakeCpuExportEncodeBackend() {
    return std::make_unique<CpuExportEncodeBackend>();
}

std::unique_ptr<IExportEncodeBackend> MakeExportEncodeBackend() {
#if defined(__APPLE__)
    if (auto backend = MakeMetalExportEncodeBackend()) return backend;
#elif defined(_WIN32)
    if (auto backend = MakeD3D12ExportEncodeBackend()) return backend;
#endif
    return MakeCpuExportEncodeBackend();
}

IExportEncodeBackend& DefaultExportEncodeBackend() {
    static std::unique_ptr<IExportEncodeBackend> backend = MakeExportEncodeBackend();
    return *backend;
}

}  // namespace softloaf::trichrome::desktop
