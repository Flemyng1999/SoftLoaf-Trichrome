#include "project_store.hpp"

#include <system_error>

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QString>

namespace softloaf::trichrome::desktop {
namespace {

QString ToQString(const std::filesystem::path& path) {
    return QString::fromStdString(path.string());
}

std::filesystem::path ResolveStoredPath(const std::filesystem::path& project_path,
                                        const QJsonObject& object) {
    const std::filesystem::path base = project_path.parent_path();
    const std::filesystem::path relative =
        object.value("relativePath").toString().toStdString();
    if (!relative.empty()) {
        std::error_code ec;
        const std::filesystem::path resolved =
            std::filesystem::weakly_canonical(base / relative, ec);
        if (!ec && std::filesystem::exists(resolved)) return resolved;
        if (std::filesystem::exists(base / relative)) return base / relative;
    }
    return std::filesystem::path(object.value("path").toString().toStdString());
}

QString RelativePathFor(const std::filesystem::path& project_path,
                        const std::filesystem::path& source_path) {
    std::error_code ec;
    const std::filesystem::path relative =
        std::filesystem::relative(source_path, project_path.parent_path(), ec);
    if (ec || relative.empty()) return {};
    return ToQString(relative);
}

}  // namespace

bool SaveProjectDocument(const std::filesystem::path& project_path,
                         const ProjectDocument& project,
                         std::string* err) {
    QJsonObject root;
    root["schemaVersion"] = project.schema_version;
    root["sensorMode"] = QString::fromStdString(project.sensor_mode);
    root["roleOrder"] = QString::fromStdString(project.role_order);
    root["sortMode"] = QString::fromStdString(project.sort_mode);
    root["activeGroup"] = project.active_group;

    QJsonArray files;
    for (const ProjectSourceFile& source : project.files) {
        QJsonObject file;
        file["path"] = ToQString(source.path);
        file["relativePath"] = RelativePathFor(project_path, source.path);
        file["selectionIndex"] = source.selection_index;
        files.push_back(file);
    }
    root["files"] = files;

    QSaveFile out(ToQString(project_path));
    if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (err) *err = "open_failed";
        return false;
    }
    out.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    if (!out.commit()) {
        if (err) *err = "commit_failed";
        return false;
    }
    if (err) *err = "ok";
    return true;
}

bool LoadProjectDocument(const std::filesystem::path& project_path,
                         ProjectDocument* project,
                         std::string* err) {
    if (!project) {
        if (err) *err = "null_project";
        return false;
    }
    QFile in(ToQString(project_path));
    if (!in.open(QIODevice::ReadOnly)) {
        if (err) *err = "open_failed";
        return false;
    }
    QJsonParseError parse_error;
    const QJsonDocument doc = QJsonDocument::fromJson(in.readAll(), &parse_error);
    if (parse_error.error != QJsonParseError::NoError || !doc.isObject()) {
        if (err) *err = "parse_failed";
        return false;
    }
    const QJsonObject root = doc.object();
    ProjectDocument loaded;
    loaded.schema_version = root.value("schemaVersion").toInt(1);
    if (loaded.schema_version != 1) {
        if (err) *err = "unsupported_schema";
        return false;
    }
    loaded.sensor_mode = root.value("sensorMode").toString("mono").toStdString();
    loaded.role_order = root.value("roleOrder").toString("RGB").toStdString();
    loaded.sort_mode = root.value("sortMode").toString("filename").toStdString();
    loaded.active_group = root.value("activeGroup").toInt(-1);
    const QJsonArray files = root.value("files").toArray();
    loaded.files.reserve(files.size());
    for (const QJsonValue& value : files) {
        if (!value.isObject()) continue;
        const QJsonObject file = value.toObject();
        ProjectSourceFile source;
        source.path = ResolveStoredPath(project_path, file);
        source.selection_index = file.value("selectionIndex").toInt(
            static_cast<int>(loaded.files.size()));
        loaded.files.push_back(std::move(source));
    }
    *project = std::move(loaded);
    if (err) *err = "ok";
    return true;
}

}  // namespace softloaf::trichrome::desktop
