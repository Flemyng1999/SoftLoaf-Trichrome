#pragma once

#include <QDir>
#include <QFileInfo>
#include <QSet>
#include <QString>

#include "softloaf_trichrome/model.hpp"

namespace softloaf::trichrome::desktop {

inline QString NormalizeExportNameSuffix(QString suffix) {
    suffix = suffix.trimmed();
    if (suffix.isEmpty()) return QStringLiteral("_rgb");
    for (QChar& c : suffix) {
        if (c.unicode() < 0x20 || QStringLiteral("\\/:*?\"<>|").contains(c))
            c = QLatin1Char('_');
    }
    return suffix;
}

inline QString ExportBaseNameForGroup(const ProjectTrichromeGroup& group,
                                      const QString& suffix = QStringLiteral("_rgb")) {
    QString base;
    if (!group.sources.empty()) {
        const std::string& source_path = group.sources.front().path;
        base = QFileInfo(QString::fromUtf8(source_path.data(),
                                           static_cast<qsizetype>(source_path.size())))
                   .completeBaseName();
    }
    if (base.isEmpty()) {
        int frame_index = group.logical_frame_index >= 0
            ? group.logical_frame_index + 1
            : group.group_index + 1;
        if (frame_index < 1) frame_index = 1;
        base = QString("trichrome_frame_%1").arg(frame_index, 4, 10, QLatin1Char('0'));
    }
    return base + NormalizeExportNameSuffix(suffix);
}

inline QString UniqueExportPath(const QString& folder,
                                const QString& base_name,
                                const QString& extension,
                                QSet<QString>* reserved_paths = nullptr) {
    QDir dir(folder);
    QString clean_extension = extension;
    if (clean_extension.startsWith(QLatin1Char('.'))) clean_extension.remove(0, 1);
    if (clean_extension.isEmpty()) clean_extension = QStringLiteral("tiff");

    for (int suffix = 1;; ++suffix) {
        const QString file_name = suffix == 1
            ? QString("%1.%2").arg(base_name, clean_extension)
            : QString("%1_%2.%3").arg(base_name).arg(suffix).arg(clean_extension);
        const QString path = dir.filePath(file_name);
        const QString key = QDir::cleanPath(path);
        if (reserved_paths && reserved_paths->contains(key)) continue;
        if (QFileInfo::exists(path)) continue;
        if (reserved_paths) reserved_paths->insert(key);
        return path;
    }
}

inline QString UniqueExportPathForGroup(const QString& folder,
                                        const ProjectTrichromeGroup& group,
                                        const QString& extension,
                                        const QString& suffix = QStringLiteral("_rgb"),
                                        QSet<QString>* reserved_paths = nullptr) {
    return UniqueExportPath(folder, ExportBaseNameForGroup(group, suffix), extension, reserved_paths);
}

inline QString UniqueExportPathForGroup(const QString& folder,
                                        const ProjectTrichromeGroup& group,
                                        const QString& extension,
                                        QSet<QString>* reserved_paths) {
    return UniqueExportPathForGroup(
        folder, group, extension, QStringLiteral("_rgb"), reserved_paths);
}

}  // namespace softloaf::trichrome::desktop
