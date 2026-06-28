#pragma once

#include <mutex>

#include <QImage>
#include <QQuickImageProvider>

namespace softloaf::trichrome::desktop {

class TrichromeImageProvider : public QQuickImageProvider {
 public:
    TrichromeImageProvider();

    QImage requestImage(const QString& id, QSize* size,
                        const QSize& requestedSize) override;
    void setPreview(const QImage& image);
    void clearPreview();

 private:
    std::mutex mu_;
    QImage preview_;
};

}  // namespace softloaf::trichrome::desktop
