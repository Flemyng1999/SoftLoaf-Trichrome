#pragma once

#include <filesystem>
#include <vector>

namespace softloaf::trichrome::desktop {

std::vector<std::filesystem::path> NativeOpenFilesOrFolders(const char* title);
std::filesystem::path NativeChooseExportTarget(const char* title);

}  // namespace softloaf::trichrome::desktop
