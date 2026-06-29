#pragma once

#include <filesystem>

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QIODevice>
#include <QString>
#include <QUrl>

namespace softloaf::trichrome::desktop {

inline std::filesystem::path PathFromQString(const QString& value) {
#if defined(_WIN32)
    return std::filesystem::path(value.toStdWString());
#else
    return std::filesystem::path(value.toUtf8().constData());
#endif
}

inline QString QStringFromPath(const std::filesystem::path& path) {
#if defined(_WIN32)
    return QString::fromStdWString(path.wstring());
#else
    const auto utf8 = path.u8string();
    return QString::fromUtf8(reinterpret_cast<const char*>(utf8.c_str()),
                             static_cast<qsizetype>(utf8.size()));
#endif
}

inline QString CleanNativePath(QString path) {
    if (path.isEmpty()) return {};
    return QDir::cleanPath(QDir::fromNativeSeparators(path));
}

inline QString LocalPathFromUrl(const QUrl& url) {
    if (!url.isValid() || url.isEmpty()) return {};
    if (url.isLocalFile()) return CleanNativePath(url.toLocalFile());
    return CleanNativePath(url.toString(QUrl::PreferLocalFile | QUrl::FullyDecoded));
}

inline QByteArray ReadFileBytes(const std::filesystem::path& path) {
    QFile file(QStringFromPath(path));
    if (!file.open(QIODevice::ReadOnly)) return {};
    return file.readAll();
}

inline bool WriteFileBytes(const std::filesystem::path& path, const QByteArray& bytes) {
    QFile file(QStringFromPath(path));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
    return file.write(bytes) == bytes.size();
}

}  // namespace softloaf::trichrome::desktop
