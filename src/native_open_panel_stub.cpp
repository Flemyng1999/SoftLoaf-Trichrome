#include "native_open_panel.hpp"

namespace softloaf::trichrome::desktop {

std::vector<std::filesystem::path> NativeOpenFilesOrFolders(const char* title) {
    (void)title;
    return {};
}

std::filesystem::path NativeChooseExportTarget(const char* title) {
    (void)title;
    return {};
}

}  // namespace softloaf::trichrome::desktop
