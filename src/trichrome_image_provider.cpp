#include "trichrome_image_provider.hpp"

#include <QSize>

namespace softloaf::trichrome::desktop {

TrichromeImageProvider::TrichromeImageProvider()
    : QQuickImageProvider(QQuickImageProvider::Image) {}

QImage TrichromeImageProvider::requestImage(const QString&, QSize* size,
                                            const QSize& requestedSize) {
    std::lock_guard<std::mutex> lock(mu_);
    if (size) *size = preview_.size();
    if (requestedSize.isValid() && !preview_.isNull()) {
        return preview_.scaled(requestedSize, Qt::KeepAspectRatio,
                               Qt::SmoothTransformation);
    }
    return preview_;
}

void TrichromeImageProvider::setPreview(const QImage& image) {
    std::lock_guard<std::mutex> lock(mu_);
    preview_ = image;
}

void TrichromeImageProvider::clearPreview() {
    std::lock_guard<std::mutex> lock(mu_);
    preview_ = QImage();
}

}  // namespace softloaf::trichrome::desktop
