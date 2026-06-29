#pragma once

#include <filesystem>

#include <QFileInfo>
#include <QString>
#include <QUrl>

namespace softloaf::trichrome::desktop {

inline std::filesystem::path PathFromQString(const QString& value) {
    return QFileInfo(value).filesystemFilePath();
}

inline QString QStringFromPath(const std::filesystem::path& path) {
    return QFileInfo(path).filePath();
}

inline QString LocalPathFromUrl(const QUrl& url) {
    if (url.isLocalFile()) return url.toLocalFile();
    return url.toString(QUrl::PreferLocalFile | QUrl::FullyDecoded);
}

}  // namespace softloaf::trichrome::desktop
