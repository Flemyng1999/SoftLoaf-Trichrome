#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace softloaf::trichrome::desktop {

struct ProjectSourceFile {
    std::filesystem::path path;
    int selection_index = 0;
};

struct ProjectDocument {
    int schema_version = 1;
    std::string sensor_mode = "mono";
    std::string role_order = "RGB";
    std::string sort_mode = "filename";
    int active_group = -1;
    std::vector<ProjectSourceFile> files;
};

bool SaveProjectDocument(const std::filesystem::path& project_path,
                         const ProjectDocument& project,
                         std::string* err);

bool LoadProjectDocument(const std::filesystem::path& project_path,
                         ProjectDocument* project,
                         std::string* err);

}  // namespace softloaf::trichrome::desktop
