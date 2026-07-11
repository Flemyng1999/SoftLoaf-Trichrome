#pragma once

#include <QByteArray>
#include <QImage>
#include <QImageWriter>
#include <QSaveFile>
#include <QString>

namespace softloaf::trichrome::desktop {

inline bool WriteExportImageSafely(const QImage& image,
                                   const QString& path,
                                   const QByteArray& format_name,
                                   QString* reason) {
    if (!QImageWriter::supportedImageFormats().contains(format_name)) {
        if (reason) {
            *reason = QString("%1 export unavailable: Qt image format plugin is missing")
                .arg(QString::fromLatin1(format_name).toUpper());
        }
        return false;
    }

    // QSaveFile keeps an interrupted/remotely-disconnected write from leaving a
    // corrupt final export. Some NAS filesystems cannot atomically rename a
    // temporary file, so allow Qt's direct-write fallback for those targets.
    QSaveFile file(path);
    file.setDirectWriteFallback(true);
    if (!file.open(QIODevice::WriteOnly)) {
        if (reason) *reason = file.errorString();
        return false;
    }

    QImageWriter writer(&file, format_name);
    if (!writer.write(image)) {
        if (reason) *reason = writer.errorString();
        file.cancelWriting();
        return false;
    }
    if (!file.commit()) {
        if (reason) *reason = file.errorString();
        return false;
    }
    if (reason) *reason = QStringLiteral("ok");
    return true;
}

}  // namespace softloaf::trichrome::desktop
